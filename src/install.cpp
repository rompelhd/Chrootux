#include "../include/install.hpp"
#include "../include/basic.hpp"
#include <iostream>
#include <fstream>
#include <chrono>
#include <curl/curl.h>
#include <archive.h>
#include <archive_entry.h>
#include <sys/stat.h>

std::pair<std::string, std::string> fileInfo;

struct OperatingSystem {
    std::string name;
    std::string url;
};

int showMenu(const std::vector<OperatingSystem>& osList) {
    std::cout << "Seleccione el sistema operativo que desea descargar:\n";
    for (size_t i = 0; i < osList.size(); ++i) {
        std::cout << i + 1 << ". " << osList[i].name << "\n";
    }

    int choice;
    std::cout << "Ingrese el número correspondiente: ";
    std::cin >> choice;
    std::cout << "\n";

    if (choice < 1 || choice > static_cast<int>(osList.size())) {
        std::cout << "Selección no válida. Por favor, elija un número entre 1 y " << osList.size() << ".\n";
        return -1;
    }

    return choice - 1;
}

std::pair<std::string, std::string> install() {
    std::vector<OperatingSystem> osList;
    if (archost == "arm") {
        osList = {
            {"Arch-Linux", "http://fl.us.mirror.archlinuxarm.org/os/ArchLinuxARM-armv7-latest.tar.gz"},
            {"Alpine-Linux", "https://dl-cdn.alpinelinux.org/alpine/v3.19/releases/armhf/alpine-minirootfs-3.19.1-armhf.tar.gz"},
            {"Kali-Linux-Minimal", "https://kali.download/nethunter-images/current/rootfs/kalifs-armhf-minimal.tar.xz"},
            {"Debian", "https://github.com/termux/proot-distro/releases/download/v4.7.0/debian-bookworm-arm-pd-v4.7.0.tar.xz"},
            {"ParrotOS", "https://raw.githubusercontent.com/EXALAB/AnLinux-Resources/master/Rootfs/Parrot/armhf/parrot-rootfs-armhf.tar.gz"}
        };
    } else if (archost == "aarch64") {
        osList = {
            {"Arch-Linux", "https://fl.us.mirror.archlinuxarm.org/os/ArchLinuxARM-aarch64-latest.tar.gz"},
            {"Alpine-Linux", "https://dl-cdn.alpinelinux.org/alpine/v3.19/releases/aarch64/alpine-minirootfs-3.19.0-aarch64.tar.gz"},
            {"Kali-Linux-Minimal", ""},
            {"Debian", "https://github.com/termux/proot-distro/releases/download/v4.7.0/debian-bookworm-aarch64-pd-v4.7.0.tar.xz"},
            {"ParrotOS", ""}
        };
    } else if (archost == "x86_64") {
        osList = {
            {"Arch-Linux", ""},
            {"Alpine-Linux", ""},
            {"Kali-Linux-Minimal", ""},
            {"Debian", ""},
            {"ParrotOS", ""}
        };
    } else {
        std::cerr << "Arquitectura no compatible: " << archost << std::endl;
        return {};
    }

    int selection;
    do {
        selection = showMenu(osList);
    } while (selection == -1);

    std::string name = osList[selection].name;
    std::string url = osList[selection].url;
    std::string filename;
    if (!archost.empty()) {
        filename = name + "-" + archost + url.substr(url.find_last_of('.'));
    } else {
        filename = name + url.substr(url.find_last_of('/'));
    }

    bool success = downloadFile(url, filename);

    std::string outputDirectory = "/data/data/com.termux/files/home/machines";

    if (success) {
        std::cout << "Se descargó el archivo: " << filename << std::endl;
        success = extractArchive(filename, outputDirectory);
        if (success) {
            std::cout << "Se extrajo el archivo correctamente." << std::endl;
        } else {
            std::cerr << "Error al extraer el archivo." << std::endl;
        }
    } else {
        std::cerr << "Error al descargar el archivo." << std::endl;
    }

    return {name, filename};
}

namespace fs = std::filesystem;

bool containsLinuxStandardDirs(const fs::path& dir) {
    return fs::exists(dir / "bin") && fs::exists(dir / "etc") && fs::exists(dir / "usr");
}

fs::path findLinuxRoot(const fs::path& dir) {
    for (const auto& entry : fs::recursive_directory_iterator(dir)) {
        if (fs::is_directory(entry) && containsLinuxStandardDirs(entry)) {
            return entry;
        }
    }
    return "";
}

bool extractArchive(const std::string& filename, const std::string& outputDirectory) {
    std::string baseFilename = filename.substr(0, filename.find_first_of("."));
    std::string outputDir = outputDirectory + "/" + baseFilename;

    mkdir(outputDir.c_str(), 0755);

    struct archive* a = archive_read_new();
    struct archive* ext = archive_write_disk_new();

    archive_read_support_format_all(a);
    archive_read_support_filter_all(a);

    archive_write_disk_set_options(ext, ARCHIVE_EXTRACT_TIME);

    if (archive_read_open_filename(a, filename.c_str(), 10240) != ARCHIVE_OK) {
        std::cerr << "Error opening archive." << std::endl;
        return false;
    }

    int r;
    struct archive_entry* entry;
    while ((r = archive_read_next_header(a, &entry)) == ARCHIVE_OK) {
        const char* entry_pathname = archive_entry_pathname(entry);
        std::string output_path = outputDir + "/" + entry_pathname;

        std::cout << "Extracting: " << output_path << std::endl;

        archive_entry_set_pathname(entry, output_path.c_str());
        archive_write_header(ext, entry);

        if (archive_entry_size(entry) > 0) {
            char buff[8192];
            ssize_t size;
            while ((size = archive_read_data(a, buff, 8192)) > 0) {
                archive_write_data(ext, buff, size);
            }
        }

        archive_write_finish_entry(ext);
    }

    archive_read_close(a);
    archive_read_free(a);
    archive_write_close(ext);
    archive_write_free(ext);

    if (!containsLinuxStandardDirs(outputDir)) {
        fs::path linuxRoot = findLinuxRoot(outputDir);
        if (!linuxRoot.empty()) {
            for (const auto& entry : fs::directory_iterator(linuxRoot)) {
                fs::rename(entry.path(), outputDir + "/" + entry.path().filename().string());
            }
            fs::remove_all(linuxRoot);
        } else {
            std::cerr << "No Linux root directory found in the extracted files." << std::endl;
        }
    }

    std::string input;
    std::cout << "¿Desea eliminar el archivo comprimido '" << filename << "'? (yes/no): ";
    std::cin >> input;

    if (input == "yes") {
        std::remove(filename.c_str());
        std::cout << "Archivo comprimido eliminado." << std::endl;
    } else {
        std::cout << "No se eliminó el archivo comprimido." << std::endl;
    }

    return r == ARCHIVE_EOF;
}

struct ProgressData {
    double lastTime;
    double totalSize;
    double downloaded;
};

int progressCallback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t, curl_off_t) {
    ProgressData* progressData = static_cast<ProgressData*>(clientp);
    if (!progressData) return 0;

    double percentage = (dltotal > 0) ? (static_cast<double>(dlnow) / static_cast<double>(dltotal)) * 100.0 : 0.0;

    auto currentTime = std::chrono::steady_clock::now();
    double deltaTime = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - std::chrono::time_point<std::chrono::steady_clock>(std::chrono::milliseconds(static_cast<long long>(progressData->lastTime)))).count() / 1000.0;
    double downloadSpeed = (deltaTime > 0) ? (static_cast<double>(dlnow) - progressData->downloaded) / deltaTime / (1024.0 * 1024.0) : 0.0;

    progressData->downloaded = static_cast<double>(dlnow);
    progressData->lastTime = static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(currentTime.time_since_epoch()).count());

    std::cout << "Downloading " << percentage << "% | MB/s: " << downloadSpeed << "\r";
    return 0;
}

bool downloadFile(const std::string& url, const std::string& filename) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "Error al inicializar libcurl." << std::endl;
        return false;
    }

    FILE* fp = fopen(filename.c_str(), "wb");
    if (!fp) {
        std::cerr << "Error al abrir el archivo de salida." << std::endl;
        curl_easy_cleanup(curl);
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);

    ProgressData progressData = {0.0, 0.0, 0.0};
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progressCallback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progressData);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        std::cerr << "Error al descargar el archivo: " << curl_easy_strerror(res) << std::endl;
        fclose(fp);
        curl_easy_cleanup(curl);
        return false;
    }

    fclose(fp);
    curl_easy_cleanup(curl);

    fp = fopen(filename.c_str(), "rb");
    if (!fp) {
        std::cerr << "Error al abrir el archivo para verificación." << std::endl;
        return false;
    }
    fseek(fp, 0, SEEK_END);
    long fileSize = ftell(fp);
    fclose(fp);

    if (fileSize == 0) {
        std::cerr << "El archivo descargado está vacío." << std::endl;
        return false;
    }

    std::cout << "Descarga completada." << std::endl;
    return true;
}

void AutoCommands() {
    std::string arch = archchecker();

    if (arch.empty()) {
        std::cerr << "Error al verificar la arquitectura. Saliendo del programa." << std::endl;
        return;
    }

    std::pair<std::string, std::string> installResult = install();
    std::string name = installResult.first;
    std::string outputDir = installResult.second;
    std::string ROOTFS_DIR = "/data/data/com.termux/files/home/machines/";
    std::string shell_path = "/bin/bash";

    std::vector<std::string> commands;
    if (name == "Arch-Linux") {
        shell_path = "/bin/bash";
        commands = {
            "rm -rf /boot", "rm /etc/resolv.conf", "echo nameserver 8.8.8.8 > /etc/resolv.conf", "sed -i 's/^CheckSpace/#CheckSpace/' /etc/pacman.conf",
            "pacman-key --init && pacman-key --populate archlinuxarm 2>/dev/null",
            R"(read -p 'Creación de usuario - Nombre de usuario: ' username && read -s -p 'Contraseña: ' password && useradd -m -s /bin/bash $username && echo $username:$password | chpasswd && mkdir -p /home/$username && chown $username:$username /home/$username && echo Usuario $username creado exitosamente en /home/$username)"
        };
        ROOTFS_DIR = "/data/data/com.termux/files/home/machines/Arch-Linux-arm";
    } else if (name == "Debian") {
        shell_path = "/bin/bash";
        commands = {"chmod 1777 /tmp", "echo nameserver 8.8.8.8 > /etc/resolv.conf"};
        ROOTFS_DIR = "/data/data/com.termux/files/home/machines/Debian-aarch64";
    } else if (name == "Kali-Linux") {
        commands = {"comando_kali_1", "comando_kali_2", "comando_kali_3"};
    } else if (name == "Alpine-Linux") {
        shell_path = "/bin/ash";
        commands = {"ls", "cat /etc/hostname", "pwd"};
        ROOTFS_DIR = "/data/data/com.termux/files/home/machines/Alpine-Linux-armhf";
    } else {
        std::cerr << "Distribución no reconocida." << std::endl;
        std::cout << name << "\n" << outputDir << std::endl;
        return;
    }

    std::vector<MountData> mount_list = {
        {"/dev", ROOTFS_DIR + "/dev", ""},
        {"/sys", ROOTFS_DIR + "/sys", ""},
        {"/proc", ROOTFS_DIR + "/proc", ""}
    };

    std::cout << "\nComandos a ejecutar:\n";
    for (const auto& cmd : commands) {
        std::cout << "- " << cmd << "\n";
    }
    std::cout << "\n- Shell a usar: " << shell_path << "\n\n";

    mountChrootAndExecute(ROOTFS_DIR, mount_list, commands, shell_path);
}
