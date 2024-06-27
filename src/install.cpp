#include "../include/install.hpp"
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


std::string arch;

std::string archchecker() {
#if defined(_M_X64) || defined(__amd64__)
    arch = "x86_64";
#elif defined(_M_IX86) || defined(__i386__)
    arch = "x86";
#elif defined(_M_ARM) || defined(__arm__)
    arch = "arm";
#elif defined(_M_ARM64) || defined(__aarch64__)
    arch = "aarch64";
#elif defined(__linux__)
    struct utsname buffer;
    if (uname(&buffer) == 0) {
        arch = buffer.machine;
    } else {
        arch = "unknown";
    }
#else
    arch = "unknown";
#endif
    return arch;
}

std::pair<std::string, std::string> install() {
    std::vector<OperatingSystem> osList;
    if (arch == "arm") {
        osList = {
            {"Arch-Linux", "http://fl.us.mirror.archlinuxarm.org/os/ArchLinuxARM-armv7-latest.tar.gz"},
            {"Alpine-Linux", "https://dl-cdn.alpinelinux.org/alpine/v3.19/releases/armhf/alpine-minirootfs-3.19.1-armhf.tar.gz"},
            {"Kali-Linux-Minimal", "https://kali.download/nethunter-images/current/rootfs/kalifs-armhf-minimal.tar.xz"},
            {"Debian", "https://rcn-ee.net/rootfs/debian-armhf-12-bookworm-minimal-mainline/2024-05-08/am57xx-debian-12.5-minimal-armhf-2024-05-08-2gb.img.xz"},
            {"ParrotOS", "https://raw.githubusercontent.com/EXALAB/AnLinux-Resources/master/Rootfs/Parrot/armhf/parrot-rootfs-armhf.tar.gz"}
        };
    } else if (arch == "aarch64") {
        osList = {
            {"Arch-Linux", "https://fl.us.mirror.archlinuxarm.org/os/ArchLinuxARM-aarch64-latest.tar.gz"},
            {"Alpine-Linux", "https://dl-cdn.alpinelinux.org/alpine/v3.19/releases/aarch64/alpine-minirootfs-3.19.0-aarch64.tar.gz"},
            {"Kali-Linux-Minimal", ""},
            {"Debian", ""},
            {"ParrotOS", ""}
        };
    } else if (arch == "x86_64") {
        osList = {
            {"Arch-Linux", ""},
            {"Alpine-Linux", ""},
            {"Kali-Linux-Minimal", ""},
            {"Debian", ""},
            {"ParrotOS", ""}
        };
    } else {
        std::cerr << "Arquitectura no compatible: " << arch << std::endl;
        return {};
    }

    int selection;
    do {
        selection = showMenu(osList);
    } while (selection == -1);

    std::string name = osList[selection].name;
    std::string url = osList[selection].url;
    std::string filename;
    if (!arch.empty()) {
        filename = name + "-" + arch + url.substr(url.find_last_of('.'));
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

bool extractArchive(const std::string& filename, const std::string& outputDirectory) {
    std::string baseFilename = filename.substr(0, filename.find_first_of("."));

    std::string outputDir = outputDirectory + "/" + baseFilename;
    mkdir(outputDir.c_str(), 0755);

    struct archive* a = archive_read_new();
    struct archive* ext = archive_write_disk_new();

    archive_read_support_format_all(a);
    archive_read_support_filter_all(a);

    archive_write_disk_set_options(ext, ARCHIVE_EXTRACT_TIME);

    archive_read_open_filename(a, filename.c_str(), 10240);

    int r;
    struct archive_entry* entry;
    while ((r = archive_read_next_header(a, &entry)) == ARCHIVE_OK) {
        const char* entry_pathname = archive_entry_pathname(entry);
        std::string output_path = outputDir + "/" + entry_pathname;

        std::cout << output_path << std::endl;

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

    std::cout << "Descarga completada.                             " << std::endl;

    return true;
}
