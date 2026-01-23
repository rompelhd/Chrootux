#include <sys/prctl.h>          // [SECURITY] NO_NEW_PRIVS y caps
#include <linux/capability.h>   // [SECURITY] drops CAP_SYS_ADMIN, etc.
#include <signal.h>             // [SECURITY] kill()
#include <sched.h>              // [SECURITY] unshare()

#include <linux/seccomp.h>
#include <linux/filter.h>
#include <asm/unistd.h>

#include <sys/wait.h>
#include <iostream>
#include <unistd.h> // fork(), exec(), chroot()
#include <vector>
#include <sys/stat.h> // mkdir()
#include <fstream>
#include <random>
#include <algorithm>
#include <string>
#include <dirent.h>
#include <filesystem>
#include <fcntl.h>     // O_WRONLY and other flags open()

#include <thread>
#include <sys/mount.h> // mount(), unmount()

#include <cstring>

#include "../include/basic.hpp"
#include "../include/install.hpp"
#include "../include/snow-effect.hpp"
#include "../include/emulation.hpp"

#include <cerrno>
#include <sys/sysmacros.h>

#ifndef SYS_pivot_root 
#define SYS_pivot_root 155
#endif

#ifndef SYS_unshare
#define SYS_unshare 272
#endif

// for arm64 termux
#ifndef __NR_mknod
#if defined(__aarch64__)
#define __NR_mknod 133
#endif
#endif

void applySeccompFilter() {

    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) {
        perror("PR_SET_NO_NEW_PRIVS");
        exit(EXIT_FAILURE);
    }

    struct sock_filter filter[] = {

        // Load syscall number
        BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct seccomp_data, nr)),

        // Namespace & FS escape
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_unshare, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),

        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_mount, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),

        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_umount2, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),

        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_pivot_root, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),

        // Privilege escalation
        //BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_setuid, 0, 1),
        //BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),

        //BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_setgid, 0, 1),
        //BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),

        //BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_setresuid, 0, 1),
        //BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),

        //BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_setresgid, 0, 1),
        //BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),

        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_capset, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),

        // Kernel / hardware
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_reboot, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),

        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_kexec_load, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),

        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_init_module, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),

        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_delete_module, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),

        // Process inspection
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_ptrace, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),

        // Keyring
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_keyctl, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),

        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_add_key, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),

        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_request_key, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),

        // Information leaks
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_perf_event_open, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),

        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_bpf, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),

        // Devices
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_mknod, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),

        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_open_by_handle_at, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),

        // Default allow
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
    };

    struct sock_fprog prog = {
        .len = (unsigned short)(sizeof(filter) / sizeof(filter[0])),
        .filter = filter,
    };

    if (prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog) != 0) {
        perror("PR_SET_SECCOMP");
        exit(EXIT_FAILURE);
    }
}

// [SECURITY] DROP DANGEROUS CAPABILITIES
void dropDangerousCaps() {
    prctl(PR_CAPBSET_DROP, CAP_SYS_ADMIN, 0, 0, 0);
    prctl(PR_CAPBSET_DROP, CAP_SYS_MODULE, 0, 0, 0);
    prctl(PR_CAPBSET_DROP, CAP_SYS_PTRACE, 0, 0, 0);
    prctl(PR_CAPBSET_DROP, CAP_SYS_BOOT, 0, 0, 0);
    prctl(PR_CAPBSET_DROP, CAP_SYS_RAWIO, 0, 0, 0);
    prctl(PR_SET_DUMPABLE, 0); // coredumps, gdb
}


struct MountOptions {
    std::vector<std::string> mounts; // {"proc", "sys", "dev"}
    bool emulate_dev = false;         // create /dev if dev no -m
};

std::vector<MountData> buildMountList(const std::string& rootfs_dir, const MountOptions& options) {
    std::vector<MountData> mount_list;

    for (const auto& m : options.mounts) {
        if (m == "proc") {
            mount_list.push_back({"proc", rootfs_dir + "/proc", "proc"});
        } else if (m == "sys") {
            mount_list.push_back({"sysfs", rootfs_dir + "/sys", "sysfs"});
        } else if (m == "dev") {
            mount_list.push_back({"none", rootfs_dir + "/dev", "devtmpfs"});
        } else {
            std::cerr << "Unknown mount type: " << m << std::endl;
        }
    }

    return mount_list;
}


namespace fs = std::filesystem;
bool createDevNode(const std::string& path, mode_t mode, int major_num, int minor_num) {
    if (mknod(path.c_str(), mode, makedev(major_num, minor_num)) != 0) {
        std::cerr << "Error creando " << path << ": " << strerror(errno) << "\n";
        return false;
    }
    chmod(path.c_str(), mode);
    return true;
}

void setupMinimalDev(const std::string& devPath) {
    if (mount("none", devPath.c_str(), "tmpfs", 0, "mode=755") != 0) {
        std::cerr << "Error montando tmpfs en " << devPath << ": " << strerror(errno) << "\n";
        return;                                                                                          }

    std::vector<std::tuple<std::string, mode_t, int, int>> devNodes = {
        {"/null",    S_IFCHR | 0666, 1, 3},
        {"/zero",    S_IFCHR | 0666, 1, 5},
        {"/random",  S_IFCHR | 0666, 1, 8},
        {"/urandom", S_IFCHR | 0666, 1, 9},
        {"/tty",     S_IFCHR | 0666, 5, 0}
    };

    for (const auto& [name, mode, major_num, minor_num] : devNodes) {
        createDevNode(devPath + name, mode, major_num, minor_num);
    }

    std::string ptsPath = devPath + "/pts";
    mkdir(ptsPath.c_str(), 0755);
    if (mount("devpts", ptsPath.c_str(), "devpts", 0, nullptr) != 0) {
        std::cerr << "Error montando devpts en " << ptsPath << ": " << strerror(errno) << "\n";          }
}

void cleanupDevFiles(const std::string& devPath) {
    try {
        if (!fs::exists(devPath)) {
            //std::cerr << "Ruta no existe: " << devPath << std::endl;
            return;
        }

        for (const auto& entry : fs::directory_iterator(devPath)) {
            try {
                fs::remove_all(entry);
            } catch (const fs::filesystem_error& e) {
                std::cerr << "Error al eliminar " << entry.path() << ": " << e.what() << std::endl;
            }
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Error accediendo a la ruta " << devPath << ": " << e.what() << std::endl;
    }
}

void teardownMinimalDev(const std::string& devPath) {
    std::string ptsPath = devPath + "/pts";
    if (fs::exists(ptsPath) && isMounted(ptsPath.c_str())) {
        if (umount(ptsPath.c_str()) != 0) {
            std::cerr << "Error unmounting " << ptsPath << ": " << strerror(errno) << std::endl;
        } else {
            std::cout << "[" << Colours::greenColour << "✔" << Colours::endColour << "] Unmounted: " << ptsPath << std::endl;
        }
    }

    if (fs::exists(devPath) && isMounted(devPath.c_str())) {
        if (umount(devPath.c_str()) != 0) {
            std::cerr << "Error desmontando " << devPath << ": " << strerror(errno) << std::endl;
        } else {
            std::cout << "[" << Colours::greenColour << "✔" << Colours::endColour << "] Unmounted: " << devPath << std::endl;
        }
    }
}

struct Phrase {
    std::string part1;
    std::string part2;
};

void printBanner(const std::string& myMessage, const std::string& messageLine2) {
    std::cout << R"(
                                                                                 _nnnn_      ,""""""""""""""""""""""""".
                                                                                dGGGGMMb     | )" << myMessage << R"(|
                                                                               @p~qp~~qMb    | )" << messageLine2 << R"(|
                                                                               M|@||@) M|   _;.........................'
 ██████╗██╗  ██╗██████╗  ██████╗  ██████╗ ████████╗██╗   ██╗██╗  ██╗           @,----.JM| -'
██╔════╝██║  ██║██╔══██╗██╔═══██╗██╔═══██╗╚══██╔══╝██║   ██║╚██╗██╔╝          JS^\__/  qKL
██║     ███████║██████╔╝██║   ██║██║   ██║   ██║   ██║   ██║ ╚███╔╝          dZP        qKRb
██║     ██╔══██║██╔══██╗██║   ██║██║   ██║   ██║   ██║   ██║ ██╔██╗         dZP          qKKb
╚██████╗██║  ██║██║  ██║╚██████╔╝╚██████╔╝   ██║   ╚██████╔╝██╔╝ ██╗       fZP            SMMb
 ╚═════╝╚═╝  ╚═╝╚═╝  ╚═╝ ╚═════╝  ╚═════╝    ╚═╝    ╚═════╝ ╚═╝  ╚═╝       HZM            MMMM
                                                                           FqM            MMMM
v0.3.7                                      Tool Written By Rompelhd     __| ".        |\dS"qML
                                                                         |    `.       | `' \Zq
                                                                        _)      \.___.,|     .'
                                                                        \____   )MMMMMM|   .'
                                                                             `-'       `--'

)";
}

void usage() {
    std::cout << "\n" << Colours::purpleColour 
              << "Usage: chrootux [OPTIONS] [COMMANDS]\n\n"
              << "Options:\n"
              << "  -i INFO               Current information about the chroot\n"
              << "  -c CREATE             Allows the creation of a user\n"
              << "  -n NAME               Start the shell with another user\n"
              << "  -p PASSW              Change the password of an existing user\n"
              << "  -u UPDATE             Update the selected chroot\n"
              << "  -h HELP               Show this help\n"
              << "  -d [ARCH]             Open menu to download distros\n"
              << "                        If ARCH is provided, list distros for that architecture\n"
              << "  -ka                   Kill all: Unmount everything and terminate all chrootux processes.\n"
              << "  -m MOUNTS             Comma-separated list of mounts to activate inside the chroot\n"
              << "                        Example: -m proc,sys,dev\n"
              << "                        Available mounts: proc, sys, dev\n\n"
              << "Warning, use of this script is at your own risk.\n"
              << "I am not responsible for your wrongdoings.\n"
              << Colours::endColour << std::endl;
}

void checkRoot() {
    if (geteuid() != 0) {
        std::cout << "\n" << Colours::redColour << "[*] You're not a fucking root" << Colours::endColour << std::endl;
        exit(0);
    }
}

void mountFileSystem(const std::string& source, const std::string& target, const std::string& fs_type) {
    int flags = 0;

    if (fs_type == "bind") {
        flags = MS_BIND | MS_REC;
        if (mount(source.c_str(), target.c_str(), nullptr, flags, nullptr) != 0) {
            std::cerr << "Error bind-mounting: " << strerror(errno) << "\n";
            exit(EXIT_FAILURE);
        }
    } else {
        if (mount(source.c_str(), target.c_str(), fs_type.c_str(), flags, nullptr) != 0) {
            std::cerr << "Error mounting fs_type " << fs_type << ": " << strerror(errno) << "\n";
            exit(EXIT_FAILURE);
        }
    }

    if (mount(nullptr, target.c_str(), nullptr, MS_PRIVATE, nullptr) != 0) {
        std::cerr << "Error setting mount to private: " << strerror(errno) << "\n";
        exit(EXIT_FAILURE);
    }
}


void mounting(const std::vector<MountData>& mount_list) {
    for (const auto& entry : mount_list) {
        mountFileSystem(entry.source, entry.target, entry.fs_type);

        if (isMounted(entry.target)) {
            std::cout << "[" << Colours::greenColour << "✔" << Colours::endColour << "] Mounted: " << entry.source << " ==> " << entry.target << std::endl;
        } else {
            std::cerr << Colours::redColour << "[" << "✘" << Colours::endColour << "] Error: Mounting in " << entry.target << std::endl;
        }
    }
}

void unmountFileSystem(const std::string& target) {
    if (isMounted(target)) {
        if (umount2(target.c_str(), MNT_DETACH) != 0) {
            std::cerr << "Error unmounting " << target << ": " << strerror(errno) << std::endl;
        } else {
            std::cout << "[" << Colours::greenColour << "✔" << Colours::endColour << "] Unmounted: " << target << std::endl;
        }
    } else {
        std::cout << "Skip unmounting, not mounted: " << target << std::endl;
    }
}


std::string generateRandomVariation() {
    std::vector<std::string> variations = {
        "[ Chr@oting ... ]",
        "[ C$root@ng ... ]",
        "[ Chr$otin@ ... ]",
        "[ $h%ooti$g ... ]"
    };

    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
    std::shuffle(variations.begin(), variations.end(), std::default_random_engine(seed));

    return variations[0];
}

void ChrootingTime() {
    std::cout << "\033[?25l" << std::flush;

    for (int i = 0; i < 10; ++i) {
        std::string variation = generateRandomVariation();

        std::cout << variation << std::flush;

        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        std::cout << '\r';
    }

    std::cout << "\033[?25h" << std::endl;
}

void mountChrootAndExecute(const std::string& ROOTFS_DIR, const std::vector<MountData>& mount_list, const std::vector<std::string>& commands, const std::string& shell_path) {
    pid_t pid = fork();

    if (pid == -1) {
        perror("Error creating child process");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {

        if (chroot(ROOTFS_DIR.c_str()) != 0) {
            perror("Error when chrooting");
            exit(EXIT_FAILURE);
        }

        if (chdir("/") != 0) {
            perror("Error changing the working directory");
            exit(EXIT_FAILURE);
        }

        for (const auto& cmd : commands) {
            pid_t pid = fork();
            if (pid == 0) {
                char* args[] = {const_cast<char*>(shell_path.c_str()), (char*)"-c", const_cast<char*>(cmd.c_str()), nullptr};
                if (execvp(shell_path.c_str(), args) == -1) {
                    perror("Error executing command");
                    exit(EXIT_FAILURE);
                }
            } else if (pid < 0) {
                perror("Error creating a child process");
                exit(EXIT_FAILURE);
            } else {
                int status;
                waitpid(pid, &status, 0);
                if (WIFEXITED(status)) {
                    int exit_status = WEXITSTATUS(status);
                    if (exit_status != 0) {
                        std::cerr << "The command \"" << cmd << "\" ended with a state of departure " << exit_status << std::endl;
                    }
                } else {
                    std::cerr << "The command \"" << cmd << "\" ended abnormally" << std::endl;
                }
            }
        }

        exit(EXIT_SUCCESS);
    } else {

        int status;
        waitpid(pid, &status, 0);

        for (const auto& entry : mount_list) {
            unmountFileSystem(entry.target);
        }
    }
}

bool check_pivot_root() {
    int result = syscall(SYS_pivot_root, ".", ".");
    if (result == -1 && errno == EINVAL) {
        return true;
    }
    return false;
}

bool check_unshare() {
    int result = syscall(SYS_unshare, CLONE_NEWPID | CLONE_NEWNS);
    if (result == -1 && errno == EPERM) {
        return true;
    } else if (result == -1) {
        return false;
    }
    return true;
}

void chrootAndLaunchShellUnsecure(
    const std::string& ROOTFS_DIR,
    const std::vector<MountData>& mount_list
) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        bool termux = isTermux();

        if (!termux) {
            if (unshare(CLONE_NEWNS | CLONE_NEWPID) != 0) {
                perror("unshare(CLONE_NEWNS | CLONE_NEWPID)");
                exit(EXIT_FAILURE);
            }
        } else {
            if (unshare(CLONE_NEWNS) != 0) {
                perror("unshare(CLONE_NEWNS)");
                exit(EXIT_FAILURE);
            }
        }

        pid_t inner = fork();
        if (inner < 0) {
            perror("fork (inner)");
            exit(EXIT_FAILURE);
        }

        if (inner == 0) {

            if (mount(nullptr, "/", nullptr, MS_REC | MS_PRIVATE, nullptr) != 0) {
                perror("mount MS_PRIVATE");
                exit(EXIT_FAILURE);
            }

            if (chroot(ROOTFS_DIR.c_str()) != 0) {
                perror("chroot");
                exit(EXIT_FAILURE);
            }

            if (chdir("/") != 0) {
                perror("chdir");
                exit(EXIT_FAILURE);
            }

            if (!termux) {
                mkdir("/proc", 0555);
                if (mount("proc", "/proc", "proc", 0, nullptr) != 0) {
                    perror("mount /proc");
                }
            }

            prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
            applySeccompFilter();
            dropDangerousCaps();

            std::cout << "\n";
            ChrootingTime();
            std::cout << "\n";

            std::vector<std::string> shells = {
                "/bin/bash", "/bin/ash", "/bin/sh"
            };

            for (const auto& shell : shells) {
                if (access(shell.c_str(), X_OK) == 0) {
                    execl(shell.c_str(), shell.c_str(), (char*)nullptr);
                }
            }

            perror("No shell found");
            _exit(EXIT_FAILURE);
        }

        waitpid(inner, nullptr, 0);
        _exit(EXIT_SUCCESS);
    }

    waitpid(pid, nullptr, 0);

    for (const auto& entry : mount_list) {
        unmountFileSystem(entry.target);
    }
}

std::string select_rootfs_dir(const std::string& machines_folder) {
    std::vector<std::string> directories;

    for (const auto& entry : fs::directory_iterator(machines_folder)) {
        if (entry.is_directory()) {
            directories.push_back(entry.path().string());
        }
    }

    if (directories.size() == 1) {
        return directories[0];
    }

    if (directories.size() > 1) {
        for (size_t i = 0; i < directories.size(); ++i) {
            std::cout << i + 1 << ") " << directories[i] << std::endl;
        }

        int choice;
        do {
            std::cout << "\nNo system selected, which system do you want to start?: ";
            std::cin >> choice;
        } while (choice < 1 || choice > static_cast<int>(directories.size()));

        return directories[choice - 1];
    }

    std::cerr << "No system was found on machines." << std::endl;
    std::exit(EXIT_FAILURE);
}

int performInstall(const std::string& arch) {

    auto installResult = install(arch);
    if (installResult.name.empty() || installResult.url.empty()) {
        std::cerr << "Error: 'name' or 'url' are empty. Exiting." << std::endl;
        return 1;
    }

    std::string name = installResult.name;
    std::string url = installResult.url;
    std::string filename = installResult.filename;

    bool success = downloadFile(url, filename);

    std::string outputDirectory = machines_folder;

    if (success) {
        std::cout << "Rootfs downloaded: " << filename << std::endl;

        auto extractResult = extractArchive(filename, outputDirectory);
        success = extractResult.second;
        std::string outputDir = extractResult.first;

        if (success) {
            std::cout << "Rootfs extracted successfully." << std::endl;
            AutoCommands(name, outputDir);
        } else {
            std::cerr << "Error extracting Rootfs." << std::endl;
            return 1;
        }
    } else {
        std::cerr << "Error downloading Rootfs." << std::endl;
        return 1;
    }

    return 0;
}

int main(int argc, char *argv[]) {
    checkRoot();

    system("clear");

    setupChrootuxConfig();

    snowtest();

    std::vector<Phrase> phrases = {
        {"Engineered by hackers,  ", "tailored for hackers.   "},
        {"In C++, engineered for  ", "speed and optimization. "},
        {"If she's not into you,  ", "get a new shell.        "},
        {"bash code.sh 2>/dev/null", "Fucking /bin/bash       "},
        {"U can add custom rootfs ", "with the -cfs option.   "},
    };

    std::srand(static_cast<unsigned int>(std::time(nullptr)));
    int randomIndex = std::rand() % phrases.size();
    Phrase randomPhrase = phrases[randomIndex];
    printBanner(randomPhrase.part1, randomPhrase.part2);

    // EnvVariables
    setenv("PATH", "/sbin:/usr/bin:/usr/sbin:/bin:/system/bin:/system/xbin", 1);
    setenv("LD_LIBRARY_PATH", "/lib:/usr/lib", 1);
    setenv("USER", "root", 1);
    setenv("HOME", "/root", 1);
    setenv("SHELL", "/bin/bash", 1);
    setenv("TERM", "xterm", 1);
    setenv("DEVPTS_MOUNT", "/dev/pts", 1);
    setenv("TMPDIR", "/tmp", 1);
    setenv("TZ", "Europe/Madrid", 1);
    setenv("LANG", "C.UTF-8", 1);
    setenv("LANGUAGE", "C.UTF-8", 1);
    setenv("LC_ALL", "C.UTF-8", 1);
    unsetenv("LD_PRELOAD");
    unsetenv("LD_AUDIT");
    unsetenv("LD_DEBUG");

    checkMachinesFolder();

    MountOptions options;
    bool runCommand = false;
    bool enableEmulation = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            usage();
            return 0;
        } else if (arg == "-i") {
            int terminalWidth = getTerminalWidth();
            (void)terminalWidth;
            machinesOn();
            return 0;
        } else if (arg == "--debug") {
            bool debugMode = true;
            (void)debugMode;
            std::cout << "Debug mode enabled." << std::endl;
        } else if (arg == "--emulation") {
            enableEmulation = true;
        } else if (arg == "-ka" || arg == "--killall") {
            std::cout << "Terminating chroot sessions and unmounting systems..." << std::endl;
            UnmountAll(machines_folder);
            return 0;
        } else if (arg == "-d") {
            std::string arch = (i + 1 < argc) ? argv[i + 1] : archchecker();
            return performInstall(arch);
        } else if (arg == "-m") {
            if (i + 1 < argc) {
                std::string mounts_str = argv[i + 1];
                i++;
                size_t pos = 0;
                while ((pos = mounts_str.find(',')) != std::string::npos) {
                    options.mounts.push_back(mounts_str.substr(0, pos));
                    mounts_str.erase(0, pos + 1);
                }
                if (!mounts_str.empty()) options.mounts.push_back(mounts_str);
            } else {
                std::cerr << "Error: -m requires a comma-separated list of mounts (proc,sys,dev)\n";
                return 1;
            }
        } else {
            std::cerr << "Unknown option: " << arg << std::endl;
            usage();
            return 1;
        }
    }

    std::string ROOTFS_DIR = select_rootfs_dir(machines_folder);
    std::cout << ROOTFS_DIR << std::endl;

    if (enableEmulation) {
        std::string host_arch   = archchecker();
        std::string rootfs_arch = archoutinfo(ROOTFS_DIR + "/bin");

        if (!setup_emulation(ROOTFS_DIR, rootfs_arch, host_arch)) {
            std::cerr << "Emulation setup failed\n";
            return 1;
        }
    }

    if (std::find(options.mounts.begin(), options.mounts.end(), "dev") == options.mounts.end()) {
        options.emulate_dev = true;
    }

    std::vector<MountData> mount_list = buildMountList(ROOTFS_DIR, options);
    mounting(mount_list);

    if (options.emulate_dev) {
        setupMinimalDev(ROOTFS_DIR + "/dev");
    }

    bool pivot_root_supported = check_pivot_root();
    bool unshare_supported = check_unshare();

    chrootAndLaunchShellUnsecure(ROOTFS_DIR, mount_list);

    if (options.emulate_dev) {
        teardownMinimalDev(ROOTFS_DIR + "/dev");
        cleanupDevFiles(ROOTFS_DIR + "/dev");
    }

    return 0;
}
