use libc::{
    c_char, c_int, c_ulong, c_void, ssize_t, utsname, AT_HWCAP, AT_HWCAP2, O_CREAT, RTLD_NEXT,
};
use once_cell::sync::Lazy;
use std::env;
use std::ffi::{CStr, CString};
use std::ptr;
use std::sync::atomic::{AtomicBool, Ordering};

static INTERCEPTED_VERSION: AtomicBool = AtomicBool::new(false);

type UnameFn = unsafe extern "C" fn(*mut utsname) -> c_int;
type GetauxvalFn = unsafe extern "C" fn(c_ulong) -> c_ulong;
type OpenatFn = unsafe extern "C" fn(c_int, *const c_char, c_int, libc::mode_t) -> c_int;
type ReadFn = unsafe extern "C" fn(c_int, *mut c_void, usize) -> ssize_t;

static REAL_UNAME: Lazy<UnameFn> =
    Lazy::new(|| unsafe { std::mem::transmute(libc::dlsym(RTLD_NEXT, c"uname".as_ptr())) });
static REAL_GETAUXVAL: Lazy<GetauxvalFn> =
    Lazy::new(|| unsafe { std::mem::transmute(libc::dlsym(RTLD_NEXT, c"getauxval".as_ptr())) });
static REAL_OPENAT: Lazy<OpenatFn> =
    Lazy::new(|| unsafe { std::mem::transmute(libc::dlsym(RTLD_NEXT, c"openat".as_ptr())) });
static REAL_READ: Lazy<ReadFn> =
    Lazy::new(|| unsafe { std::mem::transmute(libc::dlsym(RTLD_NEXT, c"read".as_ptr())) });

unsafe fn write_cstr(dst: &mut [libc::c_char], value: &str) {
    let bytes = value.as_bytes();
    let n = bytes.len().min(dst.len().saturating_sub(1));
    ptr::copy_nonoverlapping(bytes.as_ptr(), dst.as_mut_ptr() as *mut u8, n);
    dst[n] = 0;
}

#[no_mangle]
pub unsafe extern "C" fn uname(un: *mut utsname) -> c_int {
    let ret = (REAL_UNAME)(un);
    if ret != 0 || un.is_null() {
        return ret;
    }

    if let Ok(fake_sysname) = env::var("FAKE_SYSNAME") {
        write_cstr(&mut (*un).sysname, &fake_sysname);
    }
    if let Ok(fake_version) = env::var("FAKE_VERSION") {
        write_cstr(&mut (*un).version, &fake_version);
    }
    if let Ok(fake_release) = env::var("FAKE_KERNEL_RELEASE") {
        write_cstr(&mut (*un).release, &fake_release);
    }

    ret
}

#[no_mangle]
pub unsafe extern "C" fn getauxval(ty: c_ulong) -> c_ulong {
    if ty == AT_HWCAP as c_ulong {
        if let Ok(v) = env::var("FAKE_HWCAP") {
            if let Ok(parsed) = c_ulong::from_str_radix(
                v.trim_start_matches("0x"),
                if v.starts_with("0x") { 16 } else { 10 },
            ) {
                return parsed;
            }
        }
    }
    if ty == AT_HWCAP2 as c_ulong {
        if let Ok(v) = env::var("FAKE_HWCAP2") {
            if let Ok(parsed) = c_ulong::from_str_radix(
                v.trim_start_matches("0x"),
                if v.starts_with("0x") { 16 } else { 10 },
            ) {
                return parsed;
            }
        }
    }
    (REAL_GETAUXVAL)(ty)
}

#[no_mangle]
pub unsafe extern "C" fn openat(
    dirfd: c_int,
    pathname: *const c_char,
    flags: c_int,
    mode: libc::mode_t,
) -> c_int {
    if pathname.is_null() {
        return (REAL_OPENAT)(dirfd, pathname, flags, mode);
    }

    let path = CStr::from_ptr(pathname).to_string_lossy();
    if path == "/proc/version" && !INTERCEPTED_VERSION.load(Ordering::SeqCst) {
        INTERCEPTED_VERSION.store(true, Ordering::SeqCst);
        let fake = CString::new("/fake_proc_version").unwrap();
        return (REAL_OPENAT)(dirfd, fake.as_ptr(), flags | O_CREAT, 0o644);
    }

    (REAL_OPENAT)(dirfd, pathname, flags, mode)
}

#[no_mangle]
pub unsafe extern "C" fn read(fd: c_int, buf: *mut c_void, count: usize) -> ssize_t {
    let bytes = (REAL_READ)(fd, buf, count);
    if bytes <= 0 || buf.is_null() {
        return bytes;
    }

    if let Ok(fake_version) = env::var("FAKE_KERNEL_VERSION") {
        if !INTERCEPTED_VERSION.load(Ordering::SeqCst) {
            let slice = std::slice::from_raw_parts_mut(buf as *mut u8, bytes as usize);
            if slice
                .windows("Linux version".len())
                .any(|w| w == b"Linux version")
            {
                let text = format!("Linux version {}\n", fake_version);
                let t = text.as_bytes();
                let n = t.len().min(count.saturating_sub(1));
                ptr::copy_nonoverlapping(t.as_ptr(), buf as *mut u8, n);
                *((buf as *mut u8).add(n)) = 0;
                INTERCEPTED_VERSION.store(true, Ordering::SeqCst);
                return n as ssize_t;
            }
        }
    }

    bytes
}
