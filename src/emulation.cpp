#include "../include/emulation.hpp"
#include "../include/basic.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sys/mount.h>
#include <cerrno>

namespace fs = std::filesystem;

static bool mount_binfmt() {
    fs::create_directories("/proc/sys/fs/binfmt_misc");
    if (mount("binfmt_misc", "/proc/sys/fs/binfmt_misc", "binfmt_misc", 0, nullptr) == 0) {
        return true;
    }
    return errno == EBUSY;
}

static bool binfmt_exists(const std::string& name) {
    return fs::exists("/proc/sys/fs/binfmt_misc/" + name);
}

static bool register_qemu(const std::string& arch) {
    std::ofstream reg("/proc/sys/fs/binfmt_misc/register");
    if (!reg) return false;

    if (arch == "x86_64") {
        reg << ":qemu-x86_64:M::\x7f""ELF\x02\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x02\x00\x3e::"
               "/usr/bin/qemu-x86_64-static:CF";
        return true;
    }
    if (arch == "arm64") {
        reg << ":qemu-aarch64:M::\x7f""ELF\x02\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x02\x00\xb7::"
               "/usr/bin/qemu-aarch64-static:CF";
        return true;
    }
    return false;
}

bool setup_emulation(
    const std::string& rootfs,
    const std::string& rootfs_arch,
    const std::string& host_arch
) {
    const char* result = check_emulation(
        rootfs_arch.c_str(),
        host_arch.c_str()
    );

    std::cout << "[emu] Host: " << host_arch << "\n";
    std::cout << "[emu] Rootfs: " << rootfs_arch << "\n";
    std::cout << "[emu] Mode: " << result << "\n";

    if (strcmp(result, "No") == 0 || strcmp(result, "Native") == 0) {
        std::cout << "[emu] No emulation required\n";
        return true;
    }
    if (strcmp(result, "Needed") != 0) {
        std::cerr << "[emu] Unsupported architecture combination\n";
        return false;
    }

    if (!mount_binfmt() || !fs::exists("/proc/sys/fs/binfmt_misc")) {
        std::cerr << "[emu] Kernel does not support binfmt_misc\n";
        return false;
    }

    std::string qemu_bin = "/usr/bin/qemu-" + rootfs_arch + "-static";
    std::string target = rootfs + "/usr/bin/" + fs::path(qemu_bin).filename().string();

    if (!fs::exists(qemu_bin)) {
        std::cerr << "[emu] Missing " << qemu_bin << " on host\n";
        return false;
    }

    fs::create_directories(rootfs + "/usr/bin");
    fs::copy_file(qemu_bin, target, fs::copy_options::overwrite_existing);

    if (!binfmt_exists("qemu-" + rootfs_arch)) {
        if (!register_qemu(rootfs_arch)) {
            std::cerr << "[emu] Failed to register binfmt\n";
            return false;
        }
    }

    std::cout << "[emu] Emulation ready (" << rootfs_arch << ")\n";
    return true;
}
