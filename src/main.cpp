#include <sys/wait.h>
#include <string>
#include <iostream>
#include <unistd.h> // For fork(), exec(), chroot()
#include <vector>
#include <sys/stat.h> // For mkdir()
#include <fstream>
#include <random>
#include <algorithm>
#include <string>
#include <dirent.h>
#include <filesystem>
#include <fcntl.h>     // Para O_WRONLY y otros flags de open()
#include <unistd.h>    // Para open(), close(), fork(), etc.

#include <thread>
#include <sys/mount.h> // For mount(), unmount()

#include <cstring>

#include "../include/basic.hpp"
#include "../include/install.hpp"
#include "../include/snow-effect.hpp"

#include <cerrno>
#include <sys/sysmacros.h>

#ifndef SYS_pivot_root 
#define SYS_pivot_root 155
#endif

#ifndef SYS_unshare
#define SYS_unshare 272
#endif

namespace fs = std::filesystem;                                                                                                                                                                           std::string ROOTFS_DIR;                                                                              
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
    std::cout << "\n" << Colours::purpleColour << "Usage: chrootux [OPTIONS] [COMMANDS]\n\n"
              << "Options:\n"
              << "  -i INFO               Current information about the chroot\n"
              << "  -c CREATE             Allows the creation of a user\n"
              << "  -n NAME               Start the shell with another user\n"
              << "  -p PASSW              Change the password of an existing user\n"
              << "  -u UPDATE             Update the selected chroot\n"
              << "  -h HELP               Show this help\n"
              << "  -d [ARCH]             Open menu to download distros\n"
              << "                        If ARCH is provided, list distros for that architecture\n\n"
              << "  -ka                   Kill all: Unmount everything and terminate all chrootux processes.\n"
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
        perror("Error al crear el proceso hijo");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {

        if (chroot(ROOTFS_DIR.c_str()) != 0) {
            perror("Error al hacer chroot");
            exit(EXIT_FAILURE);
        }

        if (chdir("/") != 0) {
            perror("Error al cambiar el directorio de trabajo");
            exit(EXIT_FAILURE);
        }

        for (const auto& cmd : commands) {
            pid_t pid = fork();
            if (pid == 0) {
                char* args[] = {const_cast<char*>(shell_path.c_str()), (char*)"-c", const_cast<char*>(cmd.c_str()), nullptr};
                if (execvp(shell_path.c_str(), args) == -1) {
                    perror("Error al ejecutar el comando");
                    exit(EXIT_FAILURE);
                }
            } else if (pid < 0) {
                perror("Error al crear un proceso hijo");
                exit(EXIT_FAILURE);
            } else {
                int status;
                waitpid(pid, &status, 0);
                if (WIFEXITED(status)) {
                    int exit_status = WEXITSTATUS(status);
                    if (exit_status != 0) {
                        std::cerr << "El comando \"" << cmd << "\" terminó con un estado de salida " << exit_status << std::endl;
                    }
                } else {
                    std::cerr << "El comando \"" << cmd << "\" terminó anormalmente" << std::endl;
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

void chrootAndLaunchShellUnsecure(const std::string& ROOTFS_DIR, const std::vector<MountData>& mount_list) {
    pid_t pid = fork();

    if (pid == -1) {
        perror("Error creating child process");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {

        if (chroot(ROOTFS_DIR.c_str()) != 0) {
            perror("Error while chrooting");
            exit(EXIT_FAILURE);
        }

        if (chdir("/") != 0) {
            perror("Error while changing the working directory");
            exit(EXIT_FAILURE);
        }

        std::string dbus_dir = "/run/dbus";
        if (mkdir(dbus_dir.c_str(), 0755) != 0 && errno != EEXIST) {
            perror("Error when creating the /run/dbus directory");
            exit(EXIT_FAILURE);
        }

        pid_t dbus_pid = fork();
        if (dbus_pid == 0) {
            int dev_null = open("/dev/null", O_WRONLY);
            if (dev_null == -1) {
                perror("Error opening /dev/null");
                exit(EXIT_FAILURE);
            }
            dup2(dev_null, STDOUT_FILENO);
            close(dev_null);

            if (execl("/usr/bin/dbus-daemon", "dbus-daemon", "--system", "--nofork", "--nopidfile", (char *)NULL) == -1) {
                exit(EXIT_FAILURE);
            }
        } else if (dbus_pid > 0) {
            std::cout << "\n";
            ChrootingTime();
            std::cout << "\n";

            pid_t shell_pid = fork();
            if (shell_pid == 0) {
                std::vector<std::string> shells = {"/bin/bash", "/bin/ash", "/bin/sh"};

                for (const auto& shell : shells) {
                    if (access(shell.c_str(), X_OK) == 0) {
                        execl(shell.c_str(), shell.c_str(), (char *)NULL);
                    }
                }

                perror("An executable shell (bash, ash, sh) could not be found.");
                exit(EXIT_FAILURE);
            } else if (shell_pid > 0) {
                int shell_status;
                waitpid(shell_pid, &shell_status, 0);
            } else {
                perror("Error creating child process for shell");
                exit(EXIT_FAILURE);
            }

            if (kill(dbus_pid, SIGTERM) == -1) {
                perror("Error terminating dbus-daemon");
                exit(EXIT_FAILURE);
            }
        } else {
            perror("Error creating child process for dbus-daemon");
            exit(EXIT_FAILURE);
        }
    } else {
        int status;
        waitpid(pid, &status, 0);
        std::cout << "\n";

        for (const auto& entry : mount_list) {
            unmountFileSystem(entry.target);
        }
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
    setenv("LANGUAGE", "C", 1);
    setenv("LANG", "C", 1);

    checkMachinesFolder();

    if (argc > 1) {
        std::string arg1 = argv[1];

        if (arg1 == "-h" || arg1 == "--help") {
            usage();
            return 0;
        } else if (arg1 == "-i") {
            int terminalWidth = getTerminalWidth();
            (void)terminalWidth;
            machinesOn();
            return 0;
        } else if (arg1 == "--debug") {
            bool debugMode = true;
            (void)debugMode;
            std::cout << "Debug mode enabled." << std::endl;
        } else if (arg1 == "-ka" || arg1 == "--killall") {
            std::cout << "Terminating chroot sessions and unmounting systems..." << std::endl;
            UnmountAll(machines_folder);
            return 0;
        } else if (arg1 == "-d") {
            if (argc > 2) {
                std::string arch = argv[2];

                return performInstall(arch);
                return 1;

            } else {
                std::string arch = archchecker();
                std::cout << "[W] The host architecture is automatically detected.\nYou can specify one manually with: -d [architecture]\n";
                return performInstall(arch);
                return 1;
            }
        } else {
            std::cerr << "Unknown option: " << arg1 << std::endl;
            usage();
            return 1;
        }
    } else {
        std::string ROOTFS_DIR = select_rootfs_dir(machines_folder);
        std::cout << ROOTFS_DIR << std::endl;

        std::vector<MountData> mount_list = {
            //{"none", ROOTFS_DIR + "/dev", "devtmpfs"},
            {"proc", ROOTFS_DIR + "/proc", "proc"},
            //{"/sys", ROOTFS_DIR + "/sys", ""},
            };

        mounting(mount_list);

        std::string devRoot = ROOTFS_DIR + "/dev";
        setupMinimalDev(devRoot);

        bool pivot_root_supported = check_pivot_root();
        bool unshare_supported = check_unshare();

        if (pivot_root_supported && unshare_supported) {
            chrootAndLaunchShellUnsecure(ROOTFS_DIR, mount_list);
        } else {
            chrootAndLaunchShellUnsecure(ROOTFS_DIR, mount_list);
        }

        teardownMinimalDev(devRoot);
        cleanupDevFiles(devRoot);
        
    };
    return 0;
}
