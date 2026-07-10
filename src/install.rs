use crate::basic::MountData;
use anyhow::{anyhow, Context, Result};
use std::fs::{self, File};
use std::io::{self, BufRead, Read, Write};
use std::os::unix::fs::PermissionsExt;
use std::path::{Component, Path, PathBuf};
use walkdir::WalkDir;

#[derive(Clone, Debug)]
pub struct OperatingSystem {
    pub name: &'static str,
    pub url: &'static str,
}

#[derive(Clone, Debug)]
pub struct InstallResult {
    pub name: String,
    pub url: String,
    pub filename: String,
}

pub fn show_menu(os_list: &[OperatingSystem]) -> Result<usize> {
    println!("Select the operating system you want to download:");
    for (i, os) in os_list.iter().enumerate() {
        println!("{}. {}", i + 1, os.name);
    }
    print!("Enter the corresponding number: ");
    io::stdout().flush()?;

    let mut line = String::new();
    io::stdin().read_line(&mut line)?;
    let choice: usize = line.trim().parse().context("invalid selection")?;
    if choice == 0 || choice > os_list.len() {
        return Err(anyhow!("selection out of range"));
    }
    Ok(choice - 1)
}

pub fn install(archost: &str) -> Result<InstallResult> {
    let os_list = match archost {
        "arm" | "armhf" => vec![
            OperatingSystem { name: "Arch-Linux", url: "https://fl.us.mirror.archlinuxarm.org/os/ArchLinuxARM-armv7-latest.tar.gz" },
            OperatingSystem { name: "Alpine-Linux", url: "https://dl-cdn.alpinelinux.org/alpine/v3.23/releases/armhf/alpine-minirootfs-3.23.3-armhf.tar.gz" },
            OperatingSystem { name: "Kali-Linux", url: "https://kali.download/nethunter-images/current/rootfs/kali-nethunter-rootfs-minimal-armhf.tar.xz" },
            OperatingSystem { name: "Debian", url: "https://github.com/termux/proot-distro/releases/download/v4.29.0/debian-trixie-arm-pd-v4.29.0.tar.xz" },
            OperatingSystem { name: "Ubuntu-Noble", url: "https://cdimage.ubuntu.com/ubuntu-base/releases/noble/release/ubuntu-base-24.04.3-base-armhf.tar.gz" },
            OperatingSystem { name: "OpenSUSE", url: "https://github.com/termux/proot-distro/releases/download/v4.21.0/opensuse-arm-pd-v4.21.0.tar.xz" },
        ],
        "aarch64" | "arm64" => vec![
            OperatingSystem { name: "Arch-Linux", url: "https://fl.us.mirror.archlinuxarm.org/os/ArchLinuxARM-aarch64-latest.tar.gz" },
            OperatingSystem { name: "Alpine-Linux", url: "https://dl-cdn.alpinelinux.org/alpine/v3.23/releases/aarch64/alpine-minirootfs-3.23.3-aarch64.tar.gz" },
            OperatingSystem { name: "Artix", url: "https://github.com/termux/proot-distro/releases/download/v4.29.0/artix-aarch64-pd-v4.29.0.tar.xz" },
            OperatingSystem { name: "Kali-Linux", url: "https://kali.download/nethunter-images/current/rootfs/kali-nethunter-rootfs-minimal-arm64.tar.xz" },
            OperatingSystem { name: "Debian", url: "https://github.com/termux/proot-distro/releases/download/v4.29.0/debian-trixie-aarch64-pd-v4.29.0.tar.xz" },
            OperatingSystem { name: "Manjaro", url: "https://github.com/termux/proot-distro/releases/download/v4.34.2/manjaro-aarch64-pd-v4.34.2.tar.xz" },
            OperatingSystem { name: "Ubuntu-Noble", url: "https://cdimage.ubuntu.com/ubuntu-base/releases/noble/release/ubuntu-base-24.04.3-base-arm64.tar.gz" },
            OperatingSystem { name: "Fedora", url: "https://github.com/termux/proot-distro/releases/download/v4.31.0/fedora-aarch64-pd-v4.31.0.tar.xz" },
            OperatingSystem { name: "Chimera-Linux", url: "https://github.com/termux/proot-distro/releases/download/v4.29.0/chimera-aarch64-pd-v4.29.0.tar.xz" },
            OperatingSystem { name: "OpenSUSE", url: "https://github.com/termux/proot-distro/releases/download/v4.34.2/opensuse-aarch64-pd-v4.34.2.tar.xz" },
            OperatingSystem { name: "Oracle-Linux", url: "https://github.com/termux/proot-distro/releases/download/v4.31.0/oraclelinux-aarch64-pd-v4.31.0.tar.xz" },
        ],
        "x86_64" => vec![
            OperatingSystem { name: "Arch-Linux", url: "https://github.com/termux/proot-distro/releases/download/v4.34.2/archlinux-x86_64-pd-v4.34.2.tar.xz" },
            OperatingSystem { name: "Alpine-Linux", url: "https://dl-cdn.alpinelinux.org/alpine/v3.23/releases/x86_64/alpine-minirootfs-3.23.3-x86_64.tar.gz" },
            OperatingSystem { name: "Kali-Linux", url: "https://kali.download/nethunter-images/current/rootfs/kali-nethunter-rootfs-minimal-amd64.tar.xz" },
            OperatingSystem { name: "Debian", url: "https://github.com/termux/proot-distro/releases/download/v4.29.0/debian-trixie-x86_64-pd-v4.29.0.tar.xz" },
            OperatingSystem { name: "Chimera-Linux", url: "https://github.com/termux/proot-distro/releases/download/v4.29.0/chimera-x86_64-pd-v4.29.0.tar.xz" },
            OperatingSystem { name: "Ubuntu-Noble", url: "https://cdimage.ubuntu.com/ubuntu-base/releases/noble/release/ubuntu-base-24.04.3-base-amd64.tar.gz" },
            OperatingSystem { name: "Fedora", url: "https://github.com/termux/proot-distro/releases/download/v4.31.0/fedora-x86_64-pd-v4.31.0.tar.xz" },
            OperatingSystem { name: "OpenSUSE", url: "https://github.com/termux/proot-distro/releases/download/v4.34.2/opensuse-x86_64-pd-v4.34.2.tar.xz" },
            OperatingSystem { name: "Oracle-Linux", url: "https://github.com/termux/proot-distro/releases/download/v4.31.0/oraclelinux-x86_64-pd-v4.31.0.tar.xz" },
        ],
        _ => return Err(anyhow!("Unsupported architecture: {archost}")),
    };

    let mut selection = None;
    while selection.is_none() {
        selection = show_menu(&os_list).ok();
    }

    let selected = &os_list[selection.unwrap()];
    
    let ext = if selected.url.ends_with(".tar.gz") {
        ".tar.gz"
    } else if selected.url.ends_with(".tar.xz") {
        ".tar.xz"
    } else if selected.url.ends_with(".tar") {
        ".tar"
    } else {
        ".tar"
    };

    let filename = format!("{}-{}{}", selected.name, archost, ext);

    Ok(InstallResult {
        name: selected.name.to_string(),
        url: selected.url.to_string(),
        filename,
    })
}

fn contains_linux_standard_dirs(dir: &Path) -> bool {
    dir.join("bin").exists() && dir.join("etc").exists() && dir.join("usr").exists()
}

fn sanitize_archive_path(path: &Path) -> Result<PathBuf> {
    let mut out = PathBuf::new();
    for comp in path.components() {
        match comp {
            Component::Normal(c) => out.push(c),
            Component::CurDir => {}
            Component::ParentDir | Component::RootDir | Component::Prefix(_) => {
                return Err(anyhow!("unsafe archive path: {}", path.display()))
            }
        }
    }
    Ok(out)
}

fn extract_tar_reader<R: Read>(reader: R, output_dir: &Path) -> Result<()> {
    let mut archive = tar::Archive::new(reader);

    for entry in archive.entries()? {
        let mut entry = entry?;

        let path = entry.path()?.to_path_buf();
        let kind = entry.header().entry_type();

        if kind.is_hard_link() {
            continue;
        }

        println!("[{:?}] {}", kind, path.display());

        let rel = sanitize_archive_path(&path)?;
        let out = output_dir.join(rel);

        if let Some(parent) = out.parent() {
            fs::create_dir_all(parent)?;
        }

        entry.unpack(&out)?;
    }

    Ok(())
}

fn find_linux_root(dir: &Path) -> Option<PathBuf> {
    for entry in WalkDir::new(dir).into_iter().filter_map(|e| e.ok()) {
        let p = entry.path();
        if p.is_dir() && contains_linux_standard_dirs(p) {
            return Some(p.to_path_buf());
        }
    }
    None
}

pub fn extract_archive(filename: &Path, output_directory: &Path) -> Result<(PathBuf, bool)> {
    let base = filename
        .file_name()
        .and_then(|f| f.to_str())
        .unwrap_or("rootfs")
        .trim_end_matches(".tar.gz")
        .trim_end_matches(".tar.xz")
        .trim_end_matches(".tgz")
        .trim_end_matches(".txz")
        .trim_end_matches(".tar");

    let output_dir = output_directory.join(base);

    fs::create_dir_all(&output_dir)?;
    fs::set_permissions(&output_dir, fs::Permissions::from_mode(0o755))?;

    let file = File::open(filename)?;
    let name = filename.to_string_lossy();

    if name.ends_with(".tar.gz") || name.ends_with(".tgz") {
        let decoder = flate2::read::GzDecoder::new(file);
        extract_tar_reader(decoder, &output_dir)?;
    } else if name.ends_with(".tar.xz") || name.ends_with(".txz") {
        let decoder = xz2::read::XzDecoder::new(file);
        extract_tar_reader(decoder, &output_dir)?;
    } else if name.ends_with(".tar") {
        extract_tar_reader(file, &output_dir)?;
    } else {
        return Err(anyhow!("unsupported archive format"));
    }

    if !contains_linux_standard_dirs(&output_dir) {
        if let Some(root) = find_linux_root(&output_dir) {
            for entry in fs::read_dir(&root)? {
                let e = entry?;
                let target = output_dir.join(e.file_name());

                if target.exists() {
                    continue;
                }

                fs::rename(e.path(), target)?;
            }

            let _ = fs::remove_dir_all(root);
        }
    }

    print!(
        "Do you want to delete the compressed file '{}'?' (yes/no): ",
        filename.display()
    );

    io::stdout().flush()?;

    let mut input = String::new();
    io::stdin().lock().read_line(&mut input)?;

    if input.trim().eq_ignore_ascii_case("yes") {
        let _ = fs::remove_file(filename);
    }

    Ok((output_dir, true))
}

#[derive(Clone, Debug)]
pub struct ChrootCommand {
    pub cmd: String,
    pub args: Vec<String>,
    pub stdin_data: Option<String>,
}

pub fn auto_commands(
    name: &str,
    rootfs_dir: &Path,
) -> Result<(Vec<MountData>, Vec<ChrootCommand>)> {
    let mut commands = Vec::new();

    commands.push(ChrootCommand {
        cmd: "/bin/sh".into(),
        args: vec![
            "-c".into(),
            "rm -f /etc/resolv.conf && rm -rf /boot/* && chmod 1777 /tmp && echo 'nameserver 8.8.8.8' > /etc/resolv.conf".into(),
        ],
        stdin_data: None,
    });

    match name {
        "Arch-Linux" => {
            commands.push(ChrootCommand {
                cmd: "/usr/bin/sed".into(),
                args: vec![
                    "-i".into(),
                    "s/^CheckSpace/#CheckSpace/".into(),
                    "/etc/pacman.conf".into(),
                ],
                stdin_data: None,
            });

            commands.push(ChrootCommand {
                cmd: "/usr/bin/pacman-key".into(),
                args: vec!["--init".into()],
                stdin_data: None,
            });

            commands.push(ChrootCommand {
                cmd: "/usr/bin/pacman-key".into(),
                args: vec!["--populate".into(), "archlinuxarm".into()],
                stdin_data: None,
            });

            print!("Creación de usuario - Nombre de usuario: ");
            io::stdout().flush()?;
            let mut username = String::new();
            io::stdin().read_line(&mut username)?;
            let username = username.trim().to_string();

            print!("Contraseña: ");
            io::stdout().flush()?;
            let mut password = String::new();
            io::stdin().read_line(&mut password)?;
            let password = password.trim().to_string();

            commands.push(ChrootCommand {
                cmd: "/usr/sbin/useradd".into(),
                args: vec![
                    "-m".into(),
                    "-s".into(),
                    "/bin/bash".into(),
                    username.clone(),
                ],
                stdin_data: None,
            });

            commands.push(ChrootCommand {
                cmd: "/usr/sbin/chpasswd".into(),
                args: vec![],
                stdin_data: Some(format!("{}:{}\n", username, password)),
            });
        }

        "Debian"
        | "Kali-Linux"
        | "Alpine-Linux"
        | "Ubuntu-Noble"
        | "Fedora"
        | "OpenSUSE"
        | "Oracle-Linux"
        | "Manjaro"
        | "Artix"
        | "Chimera-Linux" => {}

        _ => {
            eprintln!(
                "Warning: '{}' has no specific configuration. Applying common configuration only.",
                name
            );
        }
    }

    Ok((Vec::new(), commands))
}
