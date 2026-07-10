use crate::basic::check_emulation;
use anyhow::{anyhow, Result};
use std::fs::{self, OpenOptions};
use std::io::Write;
use std::path::Path;

fn mount_binfmt() -> bool {
    let _ = fs::create_dir_all("/proc/sys/fs/binfmt_misc");
    match nix::mount::mount(
        Some("binfmt_misc"),
        "/proc/sys/fs/binfmt_misc",
        Some("binfmt_misc"),
        nix::mount::MsFlags::empty(),
        None::<&str>,
    ) {
        Ok(_) => true,
        Err(nix::errno::Errno::EBUSY) => true,
        Err(_) => false,
    }
}

fn binfmt_exists(name: &str) -> bool {
    Path::new("/proc/sys/fs/binfmt_misc").join(name).exists()
}

fn register_qemu(arch: &str) -> Result<()> {
    let mut reg = OpenOptions::new()
        .write(true)
        .open("/proc/sys/fs/binfmt_misc/register")?;
    match arch {
        "x86_64" => reg.write_all(b":qemu-x86_64:M::\x7fELF\x02\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x02\x00\x3e::/usr/bin/qemu-x86_64-static:CF")?,
        "arm64" | "aarch64" => reg.write_all(b":qemu-aarch64:M::\x7fELF\x02\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x02\x00\xb7::/usr/bin/qemu-aarch64-static:CF")?,
        _ => return Err(anyhow!("unsupported emulation arch: {arch}")),
    }
    Ok(())
}

pub fn setup_emulation(rootfs: &Path, rootfs_arch: &str, host_arch: &str) -> Result<()> {
    let mode = check_emulation(rootfs_arch, host_arch);
    println!("[emu] Host: {host_arch}");
    println!("[emu] Rootfs: {rootfs_arch}");
    println!("[emu] Mode: {mode}");

    if mode == "No" || mode == "Native" {
        println!("[emu] No emulation required");
        return Ok(());
    }
    if mode != "Needed" {
        return Err(anyhow!("[emu] Unsupported architecture combination"));
    }

    if !mount_binfmt() || !Path::new("/proc/sys/fs/binfmt_misc").exists() {
        return Err(anyhow!("[emu] Kernel does not support binfmt_misc"));
    }

    let qemu_bin = format!("/usr/bin/qemu-{rootfs_arch}-static");
    if !Path::new(&qemu_bin).exists() {
        return Err(anyhow!("[emu] Missing {qemu_bin} on host"));
    }

    let target_dir = rootfs.join("usr/bin");
    fs::create_dir_all(&target_dir)?;
    let target = target_dir.join(
        Path::new(&qemu_bin)
            .file_name()
            .ok_or_else(|| anyhow!("invalid qemu bin path"))?,
    );
    fs::copy(&qemu_bin, target)?;

    let key = if rootfs_arch == "arm64" {
        "qemu-aarch64"
    } else {
        &format!("qemu-{rootfs_arch}")
    };
    if !binfmt_exists(key) {
        register_qemu(rootfs_arch)?;
    }

    println!("[emu] Emulation ready ({rootfs_arch})");
    Ok(())
}
