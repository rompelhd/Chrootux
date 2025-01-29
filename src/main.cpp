#include <sys/wait.h>
#include <string>
#include <sys/statvfs.h>
#include <iostream>
#include <cstdlib>
#include <unistd.h> // For fork(), exec(), chroot()
#include <vector>
#include <sys/stat.h> // For mkdir()
#include <fstream>
#include <sys/mount.h> // For mount(), unmount()
#include <random>
#include <chrono>
#include <algorithm>
#include <string>
#include <thread>
#include <dirent.h>
#include <filesystem>
#include <unordered_set>
#include <cstring>


#include "../include/basic.hpp"
#include "../include/install.hpp"
#include "../include/snow-effect.hpp"

#ifndef SYS_pivot_root
#define SYS_pivot_root 155
#endif

#ifndef SYS_unshare
#define SYS_unshare 272
#endif

namespace fs = std::filesystem;

std::string ROOTFS_DIR;

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
    std::cout << "\n" << Colours::purpleColour << "Uso: chrootux [OPCIONES] [COMANDOS]\n\n"
              << "Opciones:\n"
              << "  -i INFO               La informacion actual del chroot\n"
              << "  -c CREATE             Permite la creacion de un usuario\n"
              << "  -n NAME               Para iniciar la shell con otro usuario\n"
              << "  -p PASSW              Cambia la contraseña de un usuario existente\n"
              << "  -u UPDATE             Actualizar el chroot selecionado\n"
              << "  -h HELP               Enseña esta ayuda\n\n"
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

bool isMounted(const std::string& path) {
    struct statvfs fsInfo;
    return statvfs(path.c_str(), &fsInfo) == 0;
}

unsigned long long getFilesystemID(const std::string& path) {
    struct statvfs fsInfo;
    if (statvfs(path.c_str(), &fsInfo) != 0) {
        perror("statvfs failed");
        return 0;
    }
    return fsInfo.f_fsid;
}


std::string extractDirectoryBefore(const std::string& path, const std::vector<std::string>& targets) {
    for (const auto& target : targets) {
        size_t pos = path.find(target);
        if (pos != std::string::npos) {
            size_t lastSlash = path.rfind('/', pos - 1);
            if (lastSlash != std::string::npos) {
                return path.substr(lastSlash + 1, pos - lastSlash - 1);
            }
        }
    }
    return "";
}


bool checkTargetsMounted(const std::string& base_path, const std::string& dir) {
    std::ifstream mounts("/proc/mounts");
    std::string line;
    std::unordered_set<std::string> targets_found;

    std::vector<std::string> targets = {"/proc", "/dev", "/sys"};

    while (std::getline(mounts, line)) {
        if (line.find(base_path + "/" + dir) != std::string::npos) {
            for (const auto& target : targets) {
                if (line.find(target) != std::string::npos) {
                    targets_found.insert(target);
                }
            }
        }
    }

    return targets_found.size() == 3;
}

void checkMounts(const std::string& base_path) {
    std::ifstream mounts("/proc/mounts");
    std::string line;
    std::vector<std::string> targets = {"/proc", "/dev", "/sys"};
    std::unordered_set<std::string> checked_dirs;

    while (std::getline(mounts, line)) {
        if (line.find(base_path) != std::string::npos) {
            std::string mount_point = line.substr(line.find(base_path));
            std::string dir = extractDirectoryBefore(mount_point, targets);

            if (!dir.empty() && checked_dirs.find(dir) == checked_dirs.end()) {
                if (checkTargetsMounted(base_path, dir)) {
                    std::cout << "It was already mounted /proc /dev /sys towards: " << dir << "\n" << std::endl;
                }
                checked_dirs.insert(dir);
            }
        }
    }
}

void mountFileSystem(const std::string& source, const std::string& target, const std::string& fs_type) {
    int flags = MS_BIND | MS_REC;

    if (mount(source.c_str(), target.c_str(), fs_type.c_str(), flags, nullptr) != 0) {
        std::cerr << "Error mounting the file system: " << strerror(errno) << std::endl;
        exit(EXIT_FAILURE);
    }

    // No Host
    if (mount(nullptr, target.c_str(), nullptr, MS_PRIVATE, nullptr) != 0) {
        std::cerr << "Error converting mount to private: " << strerror(errno) << std::endl;
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
            std::cerr << "Error unmounting the file system: " << strerror(errno) << std::endl;
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

        std::string dbus_dir = "/run/dbus";
        if (mkdir(dbus_dir.c_str(), 0755) != 0 && errno != EEXIST) {
            perror("Error al crear el directorio /run/dbus");
            exit(EXIT_FAILURE);
        }

        pid_t dbus_pid = fork();
        if (dbus_pid == 0) {
            int dev_null = open("/dev/null", O_WRONLY);
            if (dev_null == -1) {
                perror("Error al abrir /dev/null");
                exit(EXIT_FAILURE);
            }
            dup2(dev_null, STDOUT_FILENO);
            close(dev_null);

            if (execl("/usr/bin/dbus-daemon", "dbus-daemon", "--system", "--nofork", "--nopidfile", (char *)NULL) == -1) {
              //  perror("Error al ejecutar dbus-daemon");
                exit(EXIT_FAILURE);
            }
        } else if (dbus_pid > 0) {
            std::cout << "\n";
            ChrootingTime();
            std::cout << "\n";

            pid_t bash_pid = fork();
            if (bash_pid == 0) {
                if (execl("/bin/bash", "bash", (char *)NULL) == -1) {
                    perror("Error al ejecutar el shell interactivo de bash");
                    exit(EXIT_FAILURE);
                }
            } else if (bash_pid > 0) {
                int bash_status;
                waitpid(bash_pid, &bash_status, 0);
            } else {
                perror("Error al crear el proceso hijo para bash");
                exit(EXIT_FAILURE);
            }

            if (kill(dbus_pid, SIGTERM) == -1) {
                perror("Error al terminar dbus-daemon");
                exit(EXIT_FAILURE);
            }
        } else {
            perror("Error al crear el proceso hijo para dbus-daemon");
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

void chrootAndLaunchShell(const std::string& ROOTFS_DIR, const std::vector<MountData>& mount_list) {
    pid_t pid = fork();

    if (pid == -1) {
        perror("Error al crear el proceso hijo");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        if (unshare(CLONE_NEWPID | CLONE_NEWNS) == -1) {
            perror("Error al crear un nuevo PID y Mount namespaces");
            exit(EXIT_FAILURE);
        }

        pid_t child_pid = fork();

        if (child_pid == -1) {
            perror("Error al hacer fork en el nuevo namespace");
            exit(EXIT_FAILURE);
        }

        if (child_pid == 0) {
            if (mount("proc", "/proc", "proc", 0, NULL) == -1) {
                perror("Error al montar /proc en el nuevo namespace");
                exit(EXIT_FAILURE);
            }

            if (chroot(ROOTFS_DIR.c_str()) != 0) {
                perror("Error al hacer chroot");
                exit(EXIT_FAILURE);
            }

            if (chdir("/") != 0) {
                perror("Error al cambiar el directorio de trabajo");
                exit(EXIT_FAILURE);
            }

            if (mount("proc", "/proc", "proc", 0, NULL) == -1) {
                perror("Error al montar /proc en el nuevo rootfs");
                exit(EXIT_FAILURE);
            }

            std::string dbus_dir = "/run/dbus";
            if (mkdir(dbus_dir.c_str(), 0755) != 0 && errno != EEXIST) {
                perror("Error al crear el directorio /run/dbus");
                exit(EXIT_FAILURE);
            }

            pid_t dbus_pid = fork();
            if (dbus_pid == 0) {
                int dev_null = open("/dev/null", O_WRONLY);
                if (dev_null != -1) {
                    dup2(dev_null, STDOUT_FILENO);
                    dup2(dev_null, STDERR_FILENO);
                    close(dev_null);
                }

                execl("/usr/bin/dbus-daemon", "dbus-daemon", "--system", "--nofork", "--nopidfile", (char*)NULL);
                perror("Error al ejecutar dbus-daemon");
                exit(EXIT_FAILURE);
            }

            sleep(1);

            execl("/bin/bash", "bash", (char*)NULL);
            perror("Error al ejecutar el shell interactivo de bash");
            exit(EXIT_FAILURE);
        } else {
            int status;
            waitpid(child_pid, &status, 0);

            exit(status);
        }
    } else {
        int status;
        waitpid(pid, &status, 0);

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
    return "";
}

int performInstall(const std::string& arch) {
    checkRoot();

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

void create_simulated_hwmon() {
    const char* home_dir = std::getenv("HOME");
    std::string SIMULATED_HWMON_DIR = std::string(home_dir) + "/.config/Chrootux/hwmon";

    if (std::filesystem::exists(SIMULATED_HWMON_DIR)) {
        std::cout << "Using existing simulated hwmon directory: " << SIMULATED_HWMON_DIR << std::endl;
        return;
    }

    //std::filesystem::create_directories(SIMULATED_HWMON_DIR);

    DIR* dir = opendir("/sys/class/thermal/");
    if (dir == nullptr) {
        std::cerr << "Error: Unable to open thermal directory." << std::endl;
        return;
    }

    struct dirent* entry;
    int hwmon_counter = 0;

    while ((entry = readdir(dir)) != nullptr) {
        std::string name(entry->d_name);
        if (name.find("thermal_zone") != std::string::npos) {
            std::string type_file = "/sys/class/thermal/" + name + "/type";
            std::string temp_file = "/sys/class/thermal/" + name + "/temp";

            std::ifstream type_stream(type_file);
            std::string type;
            std::getline(type_stream, type);

            if (!type.empty() && std::filesystem::exists(temp_file)) {
                std::string hwmon_dir = SIMULATED_HWMON_DIR + "/hwmon" + std::to_string(hwmon_counter++);
                std::filesystem::create_directories(hwmon_dir);

                std::ofstream name_file(hwmon_dir + "/name");
                name_file << type << std::endl;

                std::string temp1_input_link = hwmon_dir + "/temp1_input";
                std::filesystem::create_symlink(temp_file, temp1_input_link);

                std::ofstream temp1_label_file(hwmon_dir + "/temp1_label");
                temp1_label_file << type << std::endl;

            }
        }
    }
    closedir(dir);
}

void check_and_add_hwmon_mounts(const std::string& ROOTFS_DIR, std::vector<MountData>& mount_list) {
    std::string SIMULATED_HWMON_DIR = "/data/data/com.termux/files/root-home/.config/Chrootux/";

    if (!std::filesystem::exists(SIMULATED_HWMON_DIR)) {
        std::cerr << "Error: Base directory does not exist: " << SIMULATED_HWMON_DIR << std::endl;
        return;
    }

    bool hwmon_found = false;

    for (const auto& entry : std::filesystem::directory_iterator(SIMULATED_HWMON_DIR)) {
        std::string dir_name = entry.path().filename().string();

        if (dir_name.find("hwmon") == 0 && std::filesystem::is_directory(entry)) {
            std::cout << "Found " << SIMULATED_HWMON_DIR + "/" + dir_name << std::endl;

            std::string mount_dir = ROOTFS_DIR + "/sys/class/hwmon/" + dir_name;

            mount_list.push_back({SIMULATED_HWMON_DIR + "/" + dir_name, ROOTFS_DIR + "/sys/class/" + dir_name, ""});
            hwmon_found = true;
        }
    }

    if (!hwmon_found) {
        std::cout << "No found dirs hwmon." << std::endl;
    }
}

int main(int argc, char *argv[]) {
    system("clear");
    
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
    setenv("PATH", "/sbin:/usr/bin:/usr/sbin:/system/bin:/system/xbin:$PATH", 1);
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

    const char* home_dir = std::getenv("HOME");
    if (home_dir != nullptr) {
        std::string machines_folder = createMachinesFolderIfNotExists(home_dir);
        if (machines_folder.empty()) {
            std::cerr << "Error: creating or getting the 'machines' folder." << std::endl;
            return 1;
        }

        unsigned long long rootFsid = getFilesystemID(machines_folder);
        if (rootFsid != 0) {
            checkMounts(machines_folder);
        } else {
            std::cerr << "It was not possible to obtain information on the assemblies for " << machines_folder << std::endl;
        }
    } else {
        std::cerr << "The location of the home folder could not be obtained." << std::endl;
    }

    if (argc > 1) {
        std::string arg1 = argv[1];

        if (arg1 == "-h" || arg1 == "--help") {
            usage();
            return 0;
        } else if (arg1 == "-i") {
            checkRoot();
            int terminalWidth = getTerminalWidth();
            (void)terminalWidth;
            machinesOn();
            return 0;
        } else if (arg1 == "--debug") {
            bool debugMode = true;
            (void)debugMode;
            std::cout << "Debug mode enabled." << std::endl;
        } else if (arg1 == "-k" || arg1 == "--kill") {
            std::cout << "Terminating chroot sessions and unmounting systems..." << std::endl;
            return 0;
        } else if (arg1 == "-d") {
            if (argc > 2) {
                std::string arch = argv[2];

                return performInstall(arch);

            } else {
                std::string arch = archchecker();
                std::cout << "[W] The host architecture is automatically detected.\nYou can specify one manually with: -d [architecture]\n";
                return performInstall(arch);
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
            {"/dev", ROOTFS_DIR + "/dev", ""},
            {"/sys", ROOTFS_DIR + "/sys", ""},
            {"/proc", ROOTFS_DIR + "/proc", ""},
            };

        //check_and_add_hwmon_mounts(ROOTFS_DIR, mount_list); only termux

        mounting(mount_list);

        bool pivot_root_supported = check_pivot_root();
        bool unshare_supported = check_unshare();

        if (pivot_root_supported && unshare_supported) {
            chrootAndLaunchShell(ROOTFS_DIR, mount_list);
        } else {
            chrootAndLaunchShellUnsecure(ROOTFS_DIR, mount_list);
        }

        //chrootAndLaunchShell(ROOTFS_DIR, mount_list);
    };
    return 0;
}
