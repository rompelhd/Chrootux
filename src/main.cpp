#include <sys/wait.h>
#include <string>
#include <sys/statvfs.h>
#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <vector>
#include <sys/mount.h>
#include "../include/basic.hpp"
#include "../include/install.hpp"

struct MountData {
    std::string source;
    std::string target;
    std::string fs_type;
};

void printBanner() {
    std::cout <<
R"(
                                                                                  _nnnn_
                                                                                 dGGGGMMb     ,""""""""""""""""""""".
                                                                                @p~qp~~qMb    | Fucking /bin/bash ! |
                                                                                M|@||@) M|   _;.....................'
 ██████╗██╗  ██╗██████╗  ██████╗  ██████╗ ████████╗██╗   ██╗██╗  ██╗            @,----.JM| -'
██╔════╝██║  ██║██╔══██╗██╔═══██╗██╔═══██╗╚══██╔══╝██║   ██║╚██╗██╔╝           JS^\__/  qKL
██║     ███████║██████╔╝██║   ██║██║   ██║   ██║   ██║   ██║ ╚███╔╝          dZP        qKRb
██║     ██╔══██║██╔══██╗██║   ██║██║   ██║   ██║   ██║   ██║ ██╔██╗         dZP          qKKb
╚██████╗██║  ██║██║  ██║╚██████╔╝╚██████╔╝   ██║   ╚██████╔╝██╔╝ ██╗        fZP            SMMb
 ╚═════╝╚═╝  ╚═╝╚═╝  ╚═╝ ╚═════╝  ╚═════╝    ╚═╝    ╚═════╝ ╚═╝  ╚═╝        HZM            MMMM
                                                                            FqM            MMMM
v0.3.7                                      Tool Written By Rompelhd      __| ".        |\dS"qML
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
    if (mount(source.c_str(), target.c_str(), fs_type.c_str(), MS_BIND, nullptr) != 0) {
        std::cerr << "Error al montar el sistema de archivos: " << strerror(errno) << std::endl;
        exit(EXIT_FAILURE);
    }
}

void unmountFileSystem(const std::string& target) {
    if (umount(target.c_str()) != 0) {
        perror("Error al desmontar el sistema de archivos");
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[]) {
    system("clear");
    printBanner();

    // EnvVariables
    setenv("PATH", "/sbin:/usr/bin:/usr/sbin:/system/bin:/system/xbin:$PATH", 1);
    setenv("USER", "root", 1);
    setenv("HOME", "/", 1);
    setenv("SHELL", "/bin/bash", 1);
    setenv("TERM", "xterm", 1);
    setenv("TZ", "Europe/Madrid", 1);
    setenv("LANGUAGE", "C", 1);
    setenv("LANG", "C", 1);

    // ScriptVariables
//    const std::string ROOTFS_DIR = "/data/data/com.termux/files/home/arch";
    const std::string ROOTFS_DIR = "/data/data/com.termux/files/home/machines/kalifs-armhf-minimal/kali-armhf";

    std::vector<MountData> mount_list = {
    {"/dev", ROOTFS_DIR + "/dev", ""},
    {"/sys", ROOTFS_DIR + "/sys", ""},
    {"/proc", ROOTFS_DIR + "/proc", ""},
    {"/dev/pts", ROOTFS_DIR + "/dev/pts", ""},
//    {"/sdcard", ROOTFS_DIR + "/sdcard", ""},
//    {"/tmp", ROOTFS_DIR + "/tmp", "tmpfs"},
 //   {"/android", ROOTFS_DIR + "/android", ""}
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
            std::cout << "Hola" << std::endl;
            return 0;
        } else if (std::string(argv[1]) == "-d") {
            install();
            return 0;
        }
    }

    checkRoot();

    int archResult = archchecker();
    if (archResult != 0) {
        std::cerr << "Error al verificar la arquitectura. Saliendo del programa." << std::endl;
        return 1;
    }

    for (const auto& entry : mount_list) {
        mountFileSystem(entry.source, entry.target, entry.fs_type);

        if (isMounted(entry.target)) {
            std::cout << "[" << Colours::greenColour << "✔" << Colours::endColour << "] Mounted: " << entry.source << " ==> " << entry.target << std::endl;
        } else {
            std::cerr << Colours::redColour << "[" << "✘" << Colours::endColour << "] Error: Mounting in " << entry.target << std::endl;
    }
}

    std::cout << "\n" << "[ Chrooting ... ]\n" << std::endl;

    if (chroot(ROOTFS_DIR.c_str()) != 0) {
        perror("Error al hacer chroot");
        exit(EXIT_FAILURE);
    }

    if (chdir("/") != 0) {
        perror("Error al cambiar el directorio de trabajo");
        exit(EXIT_FAILURE);
    }

    if (execl("/bin/bash", "bash", NULL) == -1) {
        perror("Error al ejecutar el shell interactivo de bash");
        exit(EXIT_FAILURE);
    }

    //Alpine linux
    //if (execl("/bin/sh", "sh", NULL) == -1) {
    //    perror("Error al ejecutar el shell interactivo de bash");
    //    exit(EXIT_FAILURE);
    //}

    // Desmontaje
    for (const auto& entry : mount_list) {
    std::cout << "[ Desmontando " << entry.target << " ... ]" << std::endl;
    unmountFileSystem(entry.target);
    }

    return 0;
}
