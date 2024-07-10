#include <sys/wait.h>
#include <string>
#include <sys/statvfs.h>
#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <vector>
#include <sys/stat.h>
#include <fstream>
#include <sys/mount.h>
#include <random>
#include <chrono>
#include <algorithm>
#include <string>
#include <thread>
#include "../include/basic.hpp"
#include "../include/install.hpp"

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
                                                                             `-'       `--' hjm

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
        std::cout << "\n" << Colours::redColour << "[*] No eres fucking root" << Colours::endColour << std::endl;
        exit(0);
    }
}

bool isMounted(const std::string& path) {
    struct statvfs fsInfo;
    return statvfs(path.c_str(), &fsInfo) == 0;
}

void mountFileSystem(const std::string& source, const std::string& target, const std::string& fs_type) {
    int flags = MS_BIND | MS_REC;

    if (mount(source.c_str(), target.c_str(), fs_type.c_str(), flags, nullptr) != 0) {
        std::cerr << "Error al montar el sistema de archivos: " << strerror(errno) << std::endl;
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
    if (umount2(target.c_str(), MNT_DETACH) != 0) {
        std::cerr << "Error al desmontar el sistema de archivos: " << strerror(errno) << std::endl;
    } else {
        std::cout << "[" << Colours::greenColour << "✔" << Colours::endColour << "] Unmounted: " << target << std::endl;
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
    for (int i = 0; i < 10; ++i) {
        std::string variation = generateRandomVariation();

        std::cout << variation << std::flush;

        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        std::cout << '\r';
    }

    std::cout << std::endl;
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

        if (setenv("DEVPTS_MOUNT", "/dev/pts", 1) != 0) {
            perror("Error al establecer la variable de entorno");
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

void chrootAndLaunchShell(const std::string& ROOTFS_DIR, const std::vector<MountData>& mount_list) {
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
                perror("Error al ejecutar dbus-daemon");
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

int main(int argc, char *argv[]) {
    system("clear");

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
    setenv("TZ", "Europe/Madrid", 1);
    setenv("LANGUAGE", "C", 1);
    setenv("LANG", "C", 1);

    // ScriptVariables
    const std::string ROOTFS_DIR = "/data/data/com.termux/files/home/machines/Arch-Linux-arm";

    std::vector<MountData> mount_list = {
    {"/dev", ROOTFS_DIR + "/dev", ""},
    {"/sys", ROOTFS_DIR + "/sys", ""},
    {"/proc", ROOTFS_DIR + "/proc", ""},
    };

    const char* home_dir = std::getenv("HOME");
    if (home_dir != nullptr) {
        std::string machines_folder = createMachinesFolderIfNotExists(home_dir);
    } else {
        std::cerr << "No se pudo obtener la ubicación de la carpeta home." << std::endl;
    }

    if (argc > 1) {
        if (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help") {
            usage();
            return 0;
        } else if (std::string(argv[1]) == "-i") {
            checkRoot();
            int terminalWidth = getTerminalWidth();
 //           hightable(terminalWidth);
 //           intertable(terminalWidth, "Contenido dentro.", terminalWidth);
 //           lowtable(terminalWidth);
            machinesOn();
            return 0;
        } else if (std::string(argv[1]) == "--debug") {
            bool flag = true;
        } else if (std::string(argv[1]) == "-d") {
            checkRoot();
            AutoCommands();
            return 0;
        }
    }

    std::string archResult = archchecker();

    if (archResult.empty()) {
        std::cerr << "Error al verificar la arquitectura. Saliendo del programa." << std::endl;
        return 1;
    }

    mounting(mount_list);
    chrootAndLaunchShell(ROOTFS_DIR, mount_list);

    return 0;
}
