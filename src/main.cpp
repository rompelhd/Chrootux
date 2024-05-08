#include <sys/wait.h>
#include <string>
#include <sys/statvfs.h>
#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <vector>
#include <sys/mount.h>

// Colors
#define greenColour "\e[0;32m\033[1m"
#define endColour "\033[0m\e[0m"
#define redColour "\e[0;31m\033[1m"
#define blueColour "\e[0;34m\033[1m"
#define yellowColour "\e[0;33m\033[1m"
#define purpleColour "\e[0;35m\033[1m"
#define turquoiseColour "\e[0;36m\033[1m"
#define grayColour "\e[0;37m\033[1m"

struct MountData {
    std::string source;
    std::string target;
    std::string fs_type;
};

bool debug = false;

void printDebugMessage(const std::string& message) {
    if (debug) {
        std::cerr << "[DEBUG] " << message << std::endl;
    }
}

void printBanner() {
    std::cout <<
R"(
                                                                                      _nnnn_
                                                                                     dGGGGMMb     ,""""""""""""""""""""".
                                                                                    @p~qp~~qMb    | Fucking /bin/bash ! |
                                                                                    M|@||@) M|   _;.....................'
 ██████╗██╗  ██╗██████╗  ██████╗  ██████╗ ████████╗██╗   ██╗██╗  ██╗                @,----.JM| -'
██╔════╝██║  ██║██╔══██╗██╔═══██╗██╔═══██╗╚══██╔══╝██║   ██║╚██╗██╔╝               JS^\__/  qKL
██║     ███████║██████╔╝██║   ██║██║   ██║   ██║   ██║   ██║ ╚███╔╝              dZP        qKRb
██║     ██╔══██║██╔══██╗██║   ██║██║   ██║   ██║   ██║   ██║ ██╔██╗             dZP          qKKb
╚██████╗██║  ██║██║  ██║╚██████╔╝╚██████╔╝   ██║   ╚██████╔╝██╔╝ ██╗            fZP            SMMb
 ╚═════╝╚═╝  ╚═╝╚═╝  ╚═╝ ╚═════╝  ╚═════╝    ╚═╝    ╚═════╝ ╚═╝  ╚═╝            HZM            MMMM
v0.3.7                                      Tool Written By Rompelhd            FqM            MMMM
                                                                              __| ".        |\dS"qML
                                                                              |    `.       | `' \Zq
                                                                             _)      \.___.,|     .'
                                                                             \____   )MMMMMM|   .'
                                                                                  `-'       `--' hjm

)";
}

void usage() {
    std::cout << "\n" << purpleColour << "Uso: chrootux [OPCIONES] [COMANDOS]\n\n"
              << "Opciones:\n"
              << "  -i INFO               La informacion actual del chroot\n"
              << "  -c CREATE             Permite la creacion de un usuario\n"
              << "  -n NAME               Para iniciar la shell con otro usuario\n"
              << "  -p PASSW              Cambia la contraseña de un usuario existente\n"
              << "  -u UPDATE             Actualizar el chroot selecionado\n"
              << "  -h HELP               Enseña esta ayuda\n\n"
              << "Warning, use of this script is at your own risk.\n"
              << "I am not responsible for your wrongdoings.\n"
              << endColour << std::endl;
}

void checkRoot() {
    if (geteuid() != 0) {
        std::cout << "\n" << redColour << "[*] No eres fucking root" << endColour << std::endl;
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
    setenv("TERM", "xterm", 1);
    setenv("LANGUAGE", "C", 1);
    setenv("LANG", "C", 1);

    // ScriptVariables
    const std::string ROOTFS_DIR = "/data/data/com.termux/files/home/arch";

    std::vector<MountData> mount_list = {
    {"/dev", ROOTFS_DIR + "/dev", ""},
    {"/sys", ROOTFS_DIR + "/sys", ""},
    {"/proc", ROOTFS_DIR + "/proc", ""},
    {"/dev/pts", ROOTFS_DIR + "/dev/pts", ""},
//    {"/sdcard", ROOTFS_DIR + "/sdcard", ""},
//    {"/tmp", ROOTFS_DIR + "/tmp", "tmpfs"},
 //   {"/android", ROOTFS_DIR + "/android", ""}
    };

    if (argc > 1) {
        if (std::string(argv[1]) == "-h") {
            usage();
            return 0;
        } else if (std::string(argv[1]) == "-i") {
            std::cout << "Hola" << std::endl;
            return 0;
        }
    }

    checkRoot();

    for (const auto& entry : mount_list) {
        mountFileSystem(entry.source, entry.target, entry.fs_type);

        if (isMounted(entry.target)) {
            std::cout << "[" << greenColour << "✔" << endColour << "] Mounted: " << entry.source << " ==> " << entry.target << std::endl;
        } else {
            std::cerr << redColour << "[" << "✘" << endColour << "] Error: Mounting in " << entry.target << std::endl;
    }
}

    std::cout << "\n" << "[ Chrooting ... ]\n" << std::endl;

    if (chroot(ROOTFS_DIR.c_str()) != 0) {
        perror("Error al hacer chroot");
        exit(EXIT_FAILURE);
    }

    printDebugMessage("Chroot realizado correctamente");

    if (chdir("/") != 0) {
        perror("Error al cambiar el directorio de trabajo");
        exit(EXIT_FAILURE);
    }

    printDebugMessage("Directorio de trabajo cambiado correctamente");

    printDebugMessage("Ejecutando shell interactivo de Bash");

    if (execl("/bin/bash", "bash", NULL) == -1) {
        perror("Error al ejecutar el shell interactivo de bash");
        exit(EXIT_FAILURE);
    }

    printDebugMessage("Shell interactivo de Bash finalizado");

    // Desmontaje
    for (const auto& entry : mount_list) {
    std::cout << "[ Desmontando " << entry.target << " ... ]" << std::endl;
    unmountFileSystem(entry.target);
    }

    return 0;
}
