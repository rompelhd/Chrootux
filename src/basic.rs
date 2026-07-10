use anyhow::{anyhow, Context, Result};
use std::collections::HashSet;
use std::fs::{self, File};
use std::io::{BufRead, BufReader, Read, Write};
use std::os::unix::fs::PermissionsExt;
use std::path::{Path, PathBuf};
use walkdir::WalkDir;

#[derive(Clone, Debug)]
pub struct MountData {
    pub source: String,
    pub target: String,
    pub fs_type: String,
}

pub fn is_termux() -> bool {
    Path::new("/data/data/com.termux/files/home").exists()
}

pub fn get_terminal_width() -> u16 {
    std::env::var("COLUMNS")
        .ok()
        .and_then(|v| v.parse::<u16>().ok())
        .unwrap_or(80)
}

pub fn setup_chrootux_config() -> Result<()> {
    let home_dir = if is_termux() {
        PathBuf::from("/data/data/com.termux/files/home")
    } else {
        PathBuf::from(std::env::var("HOME").context("Could not determine HOME")?)
    };

    let config_dir = home_dir.join(".config/Chrootux");
    let config_file = config_dir.join("local.conf");

    if !config_dir.exists() {
        fs::create_dir_all(&config_dir).context("creating Chrootux config directory")?;
        fs::set_permissions(&config_dir, fs::Permissions::from_mode(0o700))?;
    }

    if !config_file.exists() {
        let mut f = File::create(&config_file).context("creating local.conf")?;
        writeln!(f, "# Chrootux Config")?;
        writeln!(f, "")?;
        writeln!(
            f,
            "machines_folder=\"{}\"",
            home_dir.join("machines").display()
        )?;
    }

    Ok(())
}

pub fn create_machines_folder_if_not_exists(home_dir: &Path) -> Result<PathBuf> {
    let machines_folder = if is_termux() {
        let mut path = std::env::var("PATH").unwrap_or_default();
        let termux_path = "/data/data/com.termux/files/usr/bin";
        if !path.starts_with(termux_path) {
            path = format!("{}:{}", termux_path, path);
            std::env::set_var("PATH", path);
        }
        PathBuf::from("/data/data/com.termux/files/home/machines")
    } else {
        home_dir.join("machines")
    };

    if !machines_folder.exists() {
        fs::create_dir_all(&machines_folder).context("creating machines folder")?;
        fs::set_permissions(&machines_folder, fs::Permissions::from_mode(0o755))?;
    }

    Ok(machines_folder)
}

pub fn get_directories(folder_path: &Path) -> Result<Vec<PathBuf>> {
    let mut out = Vec::new();
    for entry in
        fs::read_dir(folder_path).with_context(|| format!("reading {}", folder_path.display()))?
    {
        let entry = entry?;
        if entry.file_type()?.is_dir() {
            out.push(entry.path());
        }
    }
    Ok(out)
}

pub fn archchecker() -> String {
    std::env::consts::ARCH.to_string()
}

pub fn archoutinfo(bin_path: &Path) -> String {
    let binaries = ["bash", "busybox", "apt", "sh", "ls"];
    for b in binaries {
        let p = bin_path.join(b);
        if let Ok(machine) = read_elf_machine(&p) {
            return match machine {
                0x28 => "arm",
                0x03 => "x86",
                0x3E => "x86_64",
                0xB7 => "arm64",
                _ => "unknown",
            }
            .to_string();
        }
    }
    "unknown".to_string()
}

fn read_elf_machine(path: &Path) -> Result<u16> {
    let mut f = File::open(path)?;
    let mut ident = [0u8; 18];
    f.read_exact(&mut ident)?;
    if &ident[0..4] != b"\x7fELF" {
        return Err(anyhow!("not elf"));
    }
    Ok(u16::from_le_bytes([ident[18 - 2], ident[18 - 1]]))
}

pub fn check_emulation(arch: &str, archost: &str) -> &'static str {
    match (arch, archost) {
        ("arm", "arm64") | ("x86", "x86_64") => "Native",
        ("arm64", "arm")
        | ("x86_64", "x86")
        | ("arm64", "x86")
        | ("x86_64", "arm")
        | ("arm", "x86_64")
        | ("x86", "arm64") => "Needed",
        _ if arch == archost => "No",
        _ => "Unknown",
    }
}

use std::process::Command;

pub fn download_file(url: &str, filename: &Path) -> Result<()> {
    if !url.starts_with("https://") {
        return Err(anyhow!("Refusing non-HTTPS download URL"));
    }

    let status = Command::new("curl")
        .args([
            "-L",
            "--fail",
            "--progress-bar",
            "-o",
            filename.to_str().unwrap(),
            url,
        ])
        .status()?;

    if !status.success() {
        return Err(anyhow!("download failed"));
    }

    if fs::metadata(filename)?.len() == 0 {
        return Err(anyhow!("Downloaded file is empty"));
    }

    Ok(())
}

pub fn format_size(size: u64) -> String {
    let sizes = ["B", "KB", "MB", "GB", "TB"];
    let mut val = size as f64;
    let mut idx = 0usize;
    while val >= 1024.0 && idx < sizes.len() - 1 {
        val /= 1024.0;
        idx += 1;
    }
    format!("{:.2} {}", val, sizes[idx])
}

pub fn get_directory_size(path: &Path) -> u64 {
    WalkDir::new(path)
        .into_iter()
        .filter_map(|e| e.ok())
        .filter(|e| e.path().is_file())
        .filter_map(|e| e.metadata().ok().map(|m| m.len()))
        .sum()
}

pub fn get_name_field(filepath: &Path) -> Option<String> {
    let f = File::open(filepath).ok()?;
    let r = BufReader::new(f);
    for line in r.lines().map_while(Result::ok) {
        if let Some(rest) = line.strip_prefix("NAME=") {
            return Some(rest.trim_matches('"').to_string());
        }
    }
    None
}

pub fn machines_on(machines_folder: &Path) -> Result<()> {
    let dirs = get_directories(machines_folder)?;
    let _ = get_terminal_width();
    for d in dirs {
        let etc = d.join("etc/os-release");
        let bin = d.join("bin");
        let os_name = get_name_field(&etc).unwrap_or_else(|| "N/A".to_string());
        let size = format_size(get_directory_size(&d));
        let arch = archoutinfo(&bin);
        let host = archchecker();
        let emu = check_emulation(&arch, &host);
        println!(
            "OS: {os_name} | Size: {size} | Path: {} | Arch: {arch} | Emulation: {emu}",
            d.display()
        );
    }
    Ok(())
}

pub fn is_mounted(path: &Path) -> bool {
    if let Ok(file) = File::open("/proc/mounts") {
        let reader = BufReader::new(file);
        let p = path.to_string_lossy();
        for line in reader.lines().map_while(Result::ok) {
            let cols: Vec<&str> = line.split_whitespace().collect();
            if cols.len() >= 2 && cols[1] == p {
                return true;
            }
        }
    }
    false
}

pub fn check_targets_mounted(base_path: &Path, dir: &str) -> bool {
    let mut found: HashSet<&'static str> = HashSet::new();
    let targets = ["/proc", "/dev", "/sys"];
    if let Ok(file) = File::open("/proc/mounts") {
        for line in BufReader::new(file).lines().map_while(Result::ok) {
            if line.contains(&format!("{}/{}", base_path.display(), dir)) {
                for t in targets {
                    if line.contains(t) {
                        found.insert(t);
                    }
                }
            }
        }
    }
    found.len() == 3
}

pub fn check_mounts(base_path: &Path) {
    if let Ok(file) = File::open("/proc/mounts") {
        let mut checked = HashSet::new();
        for line in BufReader::new(file).lines().map_while(Result::ok) {
            if !line.contains(&base_path.to_string_lossy().to_string()) {
                continue;
            }
            for t in ["/proc", "/dev", "/sys"] {
                if let Some(pos) = line.find(t) {
                    let before = &line[..pos];
                    if let Some(last) = before.rsplit('/').next() {
                        if checked.insert(last.to_string())
                            && check_targets_mounted(base_path, last)
                        {
                            println!("Already mounted /proc /dev /sys towards: {last}");
                        }
                    }
                }
            }
        }
    }
}

pub fn check_machines_folder() -> Result<PathBuf> {
    let home = std::env::var("HOME").context("HOME is unset")?;
    let machines = create_machines_folder_if_not_exists(Path::new(&home))?;
    check_mounts(&machines);
    Ok(machines)
}

fn kill_processes_matching<F: Fn(&str) -> bool>(pred: F) {
    if let Ok(entries) = fs::read_dir("/proc") {
        for e in entries.flatten() {
            let name = e.file_name();
            let pid_str = name.to_string_lossy();
            if !pid_str.chars().all(|c| c.is_ascii_digit()) {
                continue;
            }
            let cmdline_path = e.path().join("cmdline");
            if let Ok(mut f) = File::open(cmdline_path) {
                let mut buf = String::new();
                if f.read_to_string(&mut buf).is_ok() && pred(&buf) {
                    if let Ok(pid) = pid_str.parse::<i32>() {
                        unsafe {
                            libc::kill(pid, libc::SIGKILL);
                        }
                    }
                }
            }
        }
    }
}

pub fn unmount_all(machines_folder: &Path) {
    let mf = machines_folder.to_string_lossy().to_string();
    kill_processes_matching(|cmd| cmd.contains(&mf));
    loop {
        let mut still_mounted = false;
        let Ok(entries) = fs::read_dir(machines_folder) else {
            break;
        };
        for e in entries.flatten() {
            let path = e.path();
            if !path.is_dir() {
                continue;
            }
            let _ = nix::mount::umount2(&path, nix::mount::MntFlags::MNT_DETACH);
            if let Ok(subs) = fs::read_dir(&path) {
                for s in subs.flatten() {
                    if s.path().is_dir() {
                        let _ = nix::mount::umount2(&s.path(), nix::mount::MntFlags::MNT_DETACH);
                    }
                }
            }
            still_mounted |= is_mounted(&path);
        }
        if !still_mounted {
            break;
        }
    }
    kill_processes_matching(|cmd| cmd.contains("chrootux") || cmd.contains("Chrootux"));
}
