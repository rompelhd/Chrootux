#pragma once
#include <string>

bool setup_emulation(
    const std::string& rootfs,
    const std::string& rootfs_arch,
    const std::string& host_arch
);
