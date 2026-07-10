mod basic;
mod emulation;
mod install;

use anyhow::{anyhow, Context, Result};
use basic::{
    archchecker, archoutinfo, check_machines_folder, download_file, is_mounted, is_termux,
    machines_on, setup_chrootux_config, unmount_all, MountData,
};
use install::{auto_commands, extract_archive, install, ChrootCommand};
use nix::mount::{mount, umount2, MntFlags, MsFlags};
use nix::sched::{unshare, CloneFlags};
use nix::sys::wait::waitpid;
use nix::unistd::{access, chdir, chroot, execv, fork, ForkResult};
use rand::seq::SliceRandom;
use std::ffi::{CStr, CString};
use std::fs;
use std::io::Write;
use std::path::{Path, PathBuf};
use std::time::Duration;

const GREEN: &str = "\x1b[1;32m";
const RED: &str = "\x1b[1;31m";
const PURPLE: &str = "\x1b[1;35m";
const END: &str = "\x1b[0m";

fn install_fakeker(rootfs: &Path) -> Result<()> {
    let target = rootfs.join("usr/lib/libfakeker.so");
    let tmp = Path::new("/tmp/libfakeker.so");

    if !tmp.exists() {
        download_file(
            "https://github.com/rompelhd/chrootux/releases/download/v0.4.0/libfakeker.so",
            tmp,
        )?;
    }

    fs::copy(tmp, &target)
        .with_context(|| {
            format!(
                "Failed copying libfakeker.so to {}",
                target.display()
            )
        })?;

    Ok(())
}

fn apply_seccomp_filter() -> Result<()> {
    unsafe {
        if libc::prctl(libc::PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0 {
            return Err(anyhow!("PR_SET_NO_NEW_PRIVS failed"));
        }

        let filter: [libc::sock_filter; 28] = [
            libc::sock_filter {
                code: (libc::BPF_LD | libc::BPF_W | libc::BPF_ABS) as u16,
                jt: 0,
                jf: 0,
                k: 0,
            },
            libc::sock_filter {
                code: (libc::BPF_JMP | libc::BPF_JEQ | libc::BPF_K) as u16,
                jt: 0,
                jf: 1,
                k: libc::SYS_unshare as u32,
            },
            libc::sock_filter {
                code: (libc::BPF_RET | libc::BPF_K) as u16,
                jt: 0,
                jf: 0,
                k: (libc::SECCOMP_RET_ERRNO | (libc::EPERM as u32)),
            },
            libc::sock_filter {
                code: (libc::BPF_JMP | libc::BPF_JEQ | libc::BPF_K) as u16,
                jt: 0,
                jf: 1,
                k: libc::SYS_mount as u32,
            },
            libc::sock_filter {
                code: (libc::BPF_RET | libc::BPF_K) as u16,
                jt: 0,
                jf: 0,
                k: (libc::SECCOMP_RET_ERRNO | (libc::EPERM as u32)),
            },
            libc::sock_filter {
                code: (libc::BPF_JMP | libc::BPF_JEQ | libc::BPF_K) as u16,
                jt: 0,
                jf: 1,
                k: libc::SYS_umount2 as u32,
            },
            libc::sock_filter {
                code: (libc::BPF_RET | libc::BPF_K) as u16,
                jt: 0,
                jf: 0,
                k: (libc::SECCOMP_RET_ERRNO | (libc::EPERM as u32)),
            },
            libc::sock_filter {
                code: (libc::BPF_JMP | libc::BPF_JEQ | libc::BPF_K) as u16,
                jt: 0,
                jf: 1,
                k: libc::SYS_pivot_root as u32,
            },
            libc::sock_filter {
                code: (libc::BPF_RET | libc::BPF_K) as u16,
                jt: 0,
                jf: 0,
                k: (libc::SECCOMP_RET_ERRNO | (libc::EPERM as u32)),
            },
            libc::sock_filter {
                code: (libc::BPF_JMP | libc::BPF_JEQ | libc::BPF_K) as u16,
                jt: 0,
                jf: 1,
                k: libc::SYS_capset as u32,
            },
            libc::sock_filter {
                code: (libc::BPF_RET | libc::BPF_K) as u16,
                jt: 0,
                jf: 0,
                k: (libc::SECCOMP_RET_ERRNO | (libc::EPERM as u32)),
            },
            libc::sock_filter {
                code: (libc::BPF_JMP | libc::BPF_JEQ | libc::BPF_K) as u16,
                jt: 0,
                jf: 1,
                k: libc::SYS_reboot as u32,
            },
            libc::sock_filter {
                code: (libc::BPF_RET | libc::BPF_K) as u16,
                jt: 0,
                jf: 0,
                k: (libc::SECCOMP_RET_ERRNO | (libc::EPERM as u32)),
            },
            libc::sock_filter {
                code: (libc::BPF_JMP | libc::BPF_JEQ | libc::BPF_K) as u16,
                jt: 0,
                jf: 1,
                k: libc::SYS_kexec_load as u32,
            },
            libc::sock_filter {
                code: (libc::BPF_RET | libc::BPF_K) as u16,
                jt: 0,
                jf: 0,
                k: (libc::SECCOMP_RET_ERRNO | (libc::EPERM as u32)),
            },
            libc::sock_filter {
                code: (libc::BPF_JMP | libc::BPF_JEQ | libc::BPF_K) as u16,
                jt: 0,
                jf: 1,
                k: libc::SYS_init_module as u32,
            },
            libc::sock_filter {
                code: (libc::BPF_RET | libc::BPF_K) as u16,
                jt: 0,
                jf: 0,
                k: (libc::SECCOMP_RET_ERRNO | (libc::EPERM as u32)),
            },
            libc::sock_filter {
                code: (libc::BPF_JMP | libc::BPF_JEQ | libc::BPF_K) as u16,
                jt: 0,
                jf: 1,
                k: libc::SYS_delete_module as u32,
            },
            libc::sock_filter {
                code: (libc::BPF_RET | libc::BPF_K) as u16,
                jt: 0,
                jf: 0,
                k: (libc::SECCOMP_RET_ERRNO | (libc::EPERM as u32)),
            },
            libc::sock_filter {
                code: (libc::BPF_JMP | libc::BPF_JEQ | libc::BPF_K) as u16,
                jt: 0,
                jf: 1,
                k: libc::SYS_ptrace as u32,
            },
            libc::sock_filter {
                code: (libc::BPF_RET | libc::BPF_K) as u16,
                jt: 0,
                jf: 0,
                k: (libc::SECCOMP_RET_ERRNO | (libc::EPERM as u32)),
            },
            libc::sock_filter {
                code: (libc::BPF_JMP | libc::BPF_JEQ | libc::BPF_K) as u16,
                jt: 0,
                jf: 1,
                k: libc::SYS_keyctl as u32,
            },
            libc::sock_filter {
                code: (libc::BPF_RET | libc::BPF_K) as u16,
                jt: 0,
                jf: 0,
                k: (libc::SECCOMP_RET_ERRNO | (libc::EPERM as u32)),
            },
            libc::sock_filter {
                code: (libc::BPF_JMP | libc::BPF_JEQ | libc::BPF_K) as u16,
                jt: 0,
                jf: 1,
                k: libc::SYS_bpf as u32,
            },
            libc::sock_filter {
                code: (libc::BPF_RET | libc::BPF_K) as u16,
                jt: 0,
                jf: 0,
                k: (libc::SECCOMP_RET_ERRNO | (libc::EPERM as u32)),
            },
            libc::sock_filter {
                code: (libc::BPF_JMP | libc::BPF_JEQ | libc::BPF_K) as u16,
                jt: 0,
                jf: 1,
                k: libc::SYS_open_by_handle_at as u32,
            },
            libc::sock_filter {
                code: (libc::BPF_RET | libc::BPF_K) as u16,
                jt: 0,
                jf: 0,
                k: (libc::SECCOMP_RET_ERRNO | (libc::EPERM as u32)),
            },
            libc::sock_filter {
                code: (libc::BPF_RET | libc::BPF_K) as u16,
                jt: 0,
                jf: 0,
                k: libc::SECCOMP_RET_ALLOW,
            },
        ];

        let mut prog = libc::sock_fprog {
            len: filter.len() as u16,
            filter: filter.as_ptr() as *mut libc::sock_filter,
        };

        if libc::prctl(libc::PR_SET_SECCOMP, libc::SECCOMP_MODE_FILTER, &mut prog) != 0 {
            return Err(anyhow!("PR_SET_SECCOMP failed"));
        }
    }

    Ok(())
}

fn drop_dangerous_caps() {
    const CAP_SYS_ADMIN: libc::c_ulong = 21;
    const CAP_SYS_MODULE: libc::c_ulong = 16;
    const CAP_SYS_PTRACE: libc::c_ulong = 19;
    const CAP_SYS_BOOT: libc::c_ulong = 22;
    const CAP_SYS_RAWIO: libc::c_ulong = 17;
    unsafe {
        libc::prctl(libc::PR_CAPBSET_DROP, CAP_SYS_ADMIN, 0, 0, 0);
        libc::prctl(libc::PR_CAPBSET_DROP, CAP_SYS_MODULE, 0, 0, 0);
        libc::prctl(libc::PR_CAPBSET_DROP, CAP_SYS_PTRACE, 0, 0, 0);
        libc::prctl(libc::PR_CAPBSET_DROP, CAP_SYS_BOOT, 0, 0, 0);
        libc::prctl(libc::PR_CAPBSET_DROP, CAP_SYS_RAWIO, 0, 0, 0);
        libc::prctl(libc::PR_SET_DUMPABLE, 0);
    }
}

fn check_root() -> Result<()> {
    if unsafe { libc::geteuid() } != 0 {
        return Err(anyhow!("{}[*] You're not root{}", RED, END));
    }
    Ok(())
}

fn clear_screen() {
    print!("\x1B[2J\x1B[1;1H");
}

fn usage() {
    println!(
        "\n{}Usage: chrootux [OPTIONS]\n\nOptions:\n  -i INFO\n  -h HELP\n  -d [ARCH] download distro\n  -ka kill and unmount all\n  -m MOUNTS comma-separated mounts (proc,sys,dev)\n  --emulation enable qemu/binfmt setup\n  -ks spoof kernel information\n{}",
        PURPLE, END
    );
}

fn mount_file_system(source: &str, target: &str, fs_type: &str) -> Result<()> {
    if fs_type == "bind" {
        mount(
            Some(source),
            target,
            None::<&str>,
            MsFlags::MS_BIND | MsFlags::MS_REC,
            None::<&str>,
        )?;
    } else {
        mount(
            Some(source),
            target,
            Some(fs_type),
            MsFlags::empty(),
            None::<&str>,
        )?;
    }
    mount(
        None::<&str>,
        target,
        None::<&str>,
        MsFlags::MS_PRIVATE,
        None::<&str>,
    )?;
    Ok(())
}

fn mounting(mount_list: &[MountData]) -> Result<()> {
    for m in mount_list {
        mount_file_system(&m.source, &m.target, &m.fs_type)?;
        if is_mounted(Path::new(&m.target)) {
            println!("[{}✔{}] Mounted: {} ==> {}", GREEN, END, m.source, m.target);
        }
    }
    Ok(())
}

fn unmount_file_system(target: &str) {
    if is_mounted(Path::new(target)) {
        let _ = umount2(target, MntFlags::MNT_DETACH);
        println!("[{}✔{}] Unmounted: {}", GREEN, END, target);
    }
}

fn generate_random_variation() -> &'static str {
    let variations = [
        "[ Chr@oting ... ]",
        "[ C$root@ng ... ]",
        "[ Chr$otin@ ... ]",
        "[ $h%ooti$g ... ]",
    ];
    let mut rng = rand::thread_rng();
    variations
        .choose(&mut rng)
        .copied()
        .unwrap_or("[ Chrooting ... ]")
}

fn chrooting_time() {
    print!("\x1b[?25l");
    let _ = std::io::stdout().flush();
    for _ in 0..10 {
        print!("{}\r", generate_random_variation());
        let _ = std::io::stdout().flush();
        std::thread::sleep(Duration::from_millis(500));
    }
    println!("\x1b[?25h");
}

fn run_commands_inside_chroot(rootfs: &Path, commands: &[ChrootCommand]) -> Result<()> {
    match unsafe { fork()? } {
        ForkResult::Child => {
            chroot(rootfs)?;
            chdir("/")?;
            for cmd in commands {
                match unsafe { fork()? } {
                    ForkResult::Child => {
                        if let Some(data) = &cmd.stdin_data {
                            let _ = std::fs::write("/tmp/.chrootux_stdin", data);
                        }
                        let c_cmd = CString::new(cmd.cmd.clone())?;
                        let mut args = vec![CString::new(cmd.cmd.clone())?];
                        for a in &cmd.args {
                            args.push(CString::new(a.as_str())?);
                        }
                        let refs: Vec<&CStr> = args.iter().map(|a| a.as_c_str()).collect();
                        let _ = execv(&c_cmd, &refs);
                        std::process::exit(1);
                    }
                    ForkResult::Parent { child } => {
                        let _ = waitpid(child, None);
                    }
                }
            }
            std::process::exit(0);
        }
        ForkResult::Parent { child } => {
            let _ = waitpid(child, None);
        }
    }
    Ok(())
}

fn check_pivot_root() -> bool {
    unsafe {
        libc::syscall(libc::SYS_pivot_root, c".".as_ptr(), c".".as_ptr()) == -1
            && *libc::__errno_location() == libc::EINVAL
    }
}

fn check_unshare() -> bool {
    let r = unsafe { libc::syscall(libc::SYS_unshare, libc::CLONE_NEWPID | libc::CLONE_NEWNS) };
    if r == -1 {
        unsafe { *libc::__errno_location() == libc::EPERM }
    } else {
        true
    }
}

fn chroot_and_launch_shell_unsecure(rootfs: &Path, mount_list: &[MountData], spoof_kernel: bool) -> Result<()> {
    match unsafe { fork()? } {
        ForkResult::Child => {
            if !is_termux() {
                unshare(CloneFlags::CLONE_NEWNS | CloneFlags::CLONE_NEWPID)?;
            } else {
                unshare(CloneFlags::CLONE_NEWNS)?;
            }

            match unsafe { fork()? } {
                ForkResult::Child => {
                    mount(
                        None::<&str>,
                        "/",
                        None::<&str>,
                        MsFlags::MS_REC | MsFlags::MS_PRIVATE,
                        None::<&str>,
                    )?;

                    chroot(rootfs)?;
                    chdir("/")?;
                    
                    if spoof_kernel {
                        std::env::set_var(
                            "LD_PRELOAD",
                            "/usr/lib/libfakeker.so"
                        );
                    } else {
                        std::env::remove_var("LD_PRELOAD");
                    }

                    if !is_termux() {
                        let _ = fs::create_dir_all("/proc");
                        let _ = mount(
                            Some("proc"),
                            "/proc",
                            Some("proc"),
                            MsFlags::empty(),
                            None::<&str>,
                        );
                    }

                    apply_seccomp_filter()?;
                    drop_dangerous_caps();

                    println!();
                    chrooting_time();
                    println!();

                    for shell in ["/bin/bash", "/bin/ash", "/bin/sh"] {
                        if access(shell, nix::unistd::AccessFlags::X_OK).is_ok() {
                            let c = CString::new(shell)?;
                            let argv = [c.as_c_str()];
                            let _ = execv(c.as_c_str(), &argv);
                        }
                    }

                    std::process::exit(1);
                }
                ForkResult::Parent { child } => {
                    let _ = waitpid(child, None);
                    std::process::exit(0);
                }
            }
        }
        ForkResult::Parent { child } => {
            let _ = waitpid(child, None);
            for m in mount_list {
                unmount_file_system(&m.target);
            }
        }
    }
    Ok(())
}

fn build_mount_list(rootfs_dir: &Path, mounts: &[String]) -> Vec<MountData> {
    let mut list = Vec::new();
    for m in mounts {
        match m.as_str() {
            "proc" => list.push(MountData {
                source: "proc".into(),
                target: rootfs_dir.join("proc").display().to_string(),
                fs_type: "proc".into(),
            }),
            "sys" => list.push(MountData {
                source: "sysfs".into(),
                target: rootfs_dir.join("sys").display().to_string(),
                fs_type: "sysfs".into(),
            }),
            "dev" => list.push(MountData {
                source: "none".into(),
                target: rootfs_dir.join("dev").display().to_string(),
                fs_type: "devtmpfs".into(),
            }),
            _ => {}
        }
    }
    list
}

fn create_dev_node(path: &Path, mode: libc::mode_t, major_num: u64, minor_num: u64) {
    let dev = nix::sys::stat::makedev(major_num, minor_num);
    let _ = nix::sys::stat::mknod(
        path,
        nix::sys::stat::SFlag::S_IFCHR,
        nix::sys::stat::Mode::from_bits_truncate(mode),
        dev,
    );
}

fn setup_minimal_dev(dev_path: &Path) {
    let _ = fs::create_dir_all(dev_path);
    let _ = mount(
        Some("none"),
        dev_path,
        Some("tmpfs"),
        MsFlags::empty(),
        Some("mode=755"),
    );

    create_dev_node(&dev_path.join("null"), 0o666, 1, 3);
    create_dev_node(&dev_path.join("zero"), 0o666, 1, 5);
    create_dev_node(&dev_path.join("random"), 0o666, 1, 8);
    create_dev_node(&dev_path.join("urandom"), 0o666, 1, 9);
    create_dev_node(&dev_path.join("tty"), 0o666, 5, 0);

    let pts = dev_path.join("pts");
    let _ = fs::create_dir_all(&pts);
    let _ = mount(
        Some("devpts"),
        &pts,
        Some("devpts"),
        MsFlags::empty(),
        None::<&str>,
    );
}

fn teardown_minimal_dev(dev_path: &Path) {
    let pts = dev_path.join("pts");
    if is_mounted(&pts) {
        let _ = umount2(&pts, MntFlags::MNT_DETACH);
    }
    if is_mounted(dev_path) {
        let _ = umount2(dev_path, MntFlags::MNT_DETACH);
    }
}

fn cleanup_dev_files(dev_path: &Path) {
    if let Ok(entries) = fs::read_dir(dev_path) {
        for e in entries.flatten() {
            let _ = fs::remove_dir_all(e.path()).or_else(|_| fs::remove_file(e.path()));
        }
    }
}

fn select_rootfs_dir(machines_folder: &Path) -> Result<PathBuf> {
    let mut dirs = Vec::new();
    for entry in fs::read_dir(machines_folder)? {
        let e = entry?;
        if e.file_type()?.is_dir() {
            dirs.push(e.path());
        }
    }

    if dirs.is_empty() {
        return Err(anyhow!("No system was found on machines."));
    }
    if dirs.len() == 1 {
        return Ok(dirs.remove(0));
    }

    for (i, d) in dirs.iter().enumerate() {
        println!("{}) {}", i + 1, d.display());
    }
    print!("\nNo system selected, which system do you want to start?: ");
    let _ = std::io::stdout().flush();

    let mut line = String::new();
    std::io::stdin().read_line(&mut line)?;
    let idx: usize = line.trim().parse().context("invalid selection")?;
    dirs.get(idx - 1)
        .cloned()
        .ok_or_else(|| anyhow!("selection out of range"))
}

fn perform_install(arch: &str, machines_folder: &Path) -> Result<()> {
    let result = install(arch)?;
    let filename = PathBuf::from(&result.filename);

    download_file(&result.url, &filename)?;
    println!("Rootfs downloaded: {}", filename.display());

    let (output_dir, ok) = extract_archive(&filename, machines_folder)?;
    if !ok {
        return Err(anyhow!("Error extracting rootfs"));
    }

    let (_mounts, commands) = auto_commands(&result.name, &output_dir)?;
    run_commands_inside_chroot(&output_dir, &commands)?;
    Ok(())
}

fn print_banner() {
    let phrases = [
        ("Engineered by hackers,  ", "tailored for hackers.   "),
        ("In Rust, engineered for ", "safety and speed.       "),
        ("If she's not into you,  ", "get a new shell.        "),
        ("bash code.sh 2>/dev/null", "Fucking /bin/bash       "),
        ("U can download a rootfs ", "with the -d option.     "),
    ];
    let mut rng = rand::thread_rng();
    let (line1, line2) = phrases.choose(&mut rng).copied().unwrap();

    println!(
        r#"

                                                                                 _nnnn_      ,""""""""""""""""""""""""".
                                                                                dGGGGMMb     | {line1}|
                                                                               @p~qp~~qMb    | {line2}|
                                                                               M|@||@) M|   _;.........................'
 ██████╗██╗  ██╗██████╗  ██████╗  ██████╗ ████████╗██╗   ██╗██╗  ██╗           @,----.JM| -'
██╔════╝██║  ██║██╔══██╗██╔═══██╗██╔═══██╗╚══██╔══╝██║   ██║╚██╗██╔╝          JS^\__/  qKL
██║     ███████║██████╔╝██║   ██║██║   ██║   ██║   ██║   ██║ ╚███╔╝          dZP        qKRb
██║     ██╔══██║██╔══██╗██║   ██║██║   ██║   ██║   ██║   ██║ ██╔██╗         dZP          qKKb
╚██████╗██║  ██║██║  ██║╚██████╔╝╚██████╔╝   ██║   ╚██████╔╝██╔╝ ██╗       fZP            SMMb
 ╚═════╝╚═╝  ╚═╝╚═╝  ╚═╝ ╚═════╝  ╚═════╝    ╚═╝    ╚═════╝ ╚═╝  ╚═╝       HZM            MMMM
                                                                           FqM            MMMM
v1.0.0                                      Tool Written By Rompelhd     __| ".        |\dS"qML
                                                                         |    `.       | `' \Zq
                                                                        _)      \.___.,|     .'
                                                                        \____   )MMMMMM|   .'
                                                                             `-'       `--'
"#
    );
}

fn main() -> Result<()> {
    check_root()?;
    clear_screen();
    setup_chrootux_config()?;
    print_banner();

    std::env::set_var(
        "PATH",
        "/sbin:/usr/bin:/usr/sbin:/bin:/system/bin:/system/xbin",
    );
    std::env::set_var("LD_LIBRARY_PATH", "/lib:/usr/lib");
    std::env::set_var("USER", "root");
    std::env::set_var("HOME", "/root");
    std::env::set_var("SHELL", "/bin/bash");
    std::env::set_var("TERM", "xterm");
    std::env::set_var("DEVPTS_MOUNT", "/dev/pts");
    std::env::set_var("TMPDIR", "/tmp");
    std::env::set_var("TZ", "Europe/Madrid");
    std::env::set_var("LANG", "C.UTF-8");
    std::env::set_var("LANGUAGE", "C.UTF-8");
    std::env::set_var("LC_ALL", "C.UTF-8");

    //std::env::remove_var("LD_PRELOAD");
    std::env::remove_var("LD_AUDIT");
    std::env::remove_var("LD_DEBUG");

    let machines_folder = check_machines_folder()?;

    let args: Vec<String> = std::env::args().collect();
    let mut mounts: Vec<String> = Vec::new();
    let mut enable_emulation = false;
    let mut spoof_kernel = false;

    let mut i = 1usize;
    while i < args.len() {
        match args[i].as_str() {
            "-h" | "--help" => {
                usage();
                return Ok(());
            }
            "-i" => {
                machines_on(&machines_folder)?;
                return Ok(());
            }
            "--debug" => {
                println!("Debug mode enabled.");
            }
            "--emulation" => {
                enable_emulation = true;
            }
            "-ks" | "--kernel-spoof" => {
                spoof_kernel = true;
            }
            "-ka" | "--killall" => {
                println!("Terminating chroot sessions and unmounting systems...");
                unmount_all(&machines_folder);
                return Ok(());
            }
            "-d" => {
                let arch = if i + 1 < args.len() && !args[i + 1].starts_with('-') {
                    i += 1;
                    args[i].clone()
                } else {
                    archchecker()
                };
                return perform_install(&arch, &machines_folder);
            }
            "-m" => {
                if i + 1 >= args.len() {
                    return Err(anyhow!("-m requires a comma-separated list of mounts"));
                }
                i += 1;
                mounts.extend(
                    args[i]
                        .split(',')
                        .filter(|s| !s.is_empty())
                        .map(|s| s.to_string()),
                );
            }
            other => {
                return Err(anyhow!("Unknown option: {}", other));
            }
        }
        i += 1;
    }

    if spoof_kernel {
        std::env::set_var("FAKE_SYSNAME", "Linux");
        std::env::set_var("FAKE_NODENAME", "Chrootux");
        std::env::set_var("FAKE_KERNEL_RELEASE", "7.1.3.chrootux");
        std::env::set_var(
            "FAKE_KERNEL_VERSION",
            "#1 SMP PREEMPT_DYNAMIC chrootux 7.1.3",
        );
    }

    let rootfs_dir = select_rootfs_dir(&machines_folder)?;

    if enable_emulation {
        let host_arch = archchecker();
        let rootfs_arch = archoutinfo(&rootfs_dir.join("bin"));
        emulation::setup_emulation(&rootfs_dir, &rootfs_arch, &host_arch)?;
    }

    let emulate_dev = !mounts.iter().any(|m| m == "dev");
    let mount_list = build_mount_list(&rootfs_dir, &mounts);
    mounting(&mount_list)?;

    if emulate_dev {
        setup_minimal_dev(&rootfs_dir.join("dev"));
    }

    let _pivot_root_supported = check_pivot_root();
    let _unshare_supported = check_unshare();

    if spoof_kernel {
        install_fakeker(&rootfs_dir)?;
    }

    chroot_and_launch_shell_unsecure(&rootfs_dir, &mount_list, spoof_kernel)?;

    if emulate_dev {
        teardown_minimal_dev(&rootfs_dir.join("dev"));
        cleanup_dev_files(&rootfs_dir.join("dev"));
    }

    Ok(())
}
