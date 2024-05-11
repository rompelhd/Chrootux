#include "../include/install.hpp"
#include <iostream>
#include <fstream>
#include <chrono>
#include <curl/curl.h>
#include <archive.h>
#include <archive_entry.h>

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

    if (choice < 1 || choice > static_cast<int>(osList.size())) {
        std::cout << "Selección no válida. Por favor, elija un número entre 1 y " << osList.size() << ".\n";
        return -1;
    }

    return choice - 1;
}

std::vector<std::pair<std::string, std::string>> getMatchingFileInfos(const char* binaryPath) {
    std::vector<std::pair<std::string, std::string>> matchingFileInfos;

    std::string command = "file -b " + std::string(binaryPath);
    FILE* filePipe = popen(command.c_str(), "r");
    if (!filePipe) {
        std::cerr << "Error al ejecutar el comando file." << std::endl;
        return matchingFileInfos;
    }

    char fileBuffer[256];
    while (fgets(fileBuffer, 256, filePipe) != NULL) {
        std::string fileInfo = fileBuffer;
        size_t elfPos = fileInfo.find("ELF");
        size_t elfBitsPos = fileInfo.find("-bit");
        size_t armPos = fileInfo.find("ARM");
        if (elfPos != std::string::npos && elfBitsPos != std::string::npos && armPos != std::string::npos) {
            std::string elf = fileInfo.substr(elfPos, elfBitsPos - elfPos + 4);
            std::string arm = fileInfo.substr(armPos, fileInfo.find(",", armPos) - armPos);

            if (elf == "ELF 32-bit" && arm == "ARM") {
                elf = "armhf";
            } else if (elf == "ELF 64-bit" && arm == "ARM") {
                elf = "arm64";
            }
            matchingFileInfos.push_back({elf, arm});
        }
    }

    pclose(filePipe);
    return matchingFileInfos;
}

int archchecker() {
    const char* binaryPath = "/data/data/com.termux/files/usr/bin/apt";
    std::vector<std::pair<std::string, std::string>> matchingFileInfos = getMatchingFileInfos(binaryPath);

    if (matchingFileInfos.empty()) {
        std::cerr << "No se encontraron archivos ELF de 32 o 64 bits." << std::endl;
        return 1;
    }

    for (const auto& fileInfo : matchingFileInfos) {
        std::cout << fileInfo.first << std::endl;
    }
    return 0;
}

void install() {
    std::vector<OperatingSystem> osListArmhf = {
        {"Alpine Linux", "https://dl-cdn.alpinelinux.org/alpine/v3.19/releases/armhf/alpine-minirootfs-3.19.1-armhf.tar.gz"},
        {"Kali Linux Minimal", "https://kali.download/nethunter-images/current/rootfs/kalifs-armhf-minimal.tar.xz"},
        {"Debian", "https://rcn-ee.net/rootfs/debian-armhf-12-bookworm-minimal-mainline/2024-05-08/am57xx-debian-12.5-minimal-armhf-2024-05-08-2gb.img.xz"},
        {"ParrotOS", "https://raw.githubusercontent.com/EXALAB/AnLinux-Resources/master/Rootfs/Parrot/armhf/parrot-rootfs-armhf.tar.gz"}
    };

    int selection;
    do {
        selection = showMenu(osListArmhf);
    } while (selection == -1);

    std::string url = osListArmhf[selection].url;
    std::string filename = url.substr(url.find_last_of('/') + 1);

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
    double deltaTime = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - std::chrono::time_point<std::chrono::steady_clock>(std::chrono::milliseconds(static_cast<long long>(progressData->lastTime)))).count() / 1000.0; // Calcular tiempo transcurrido en segundos
    double downloadSpeed = (deltaTime > 0) ? (static_cast<double>(dlnow - progressData->downloaded) / deltaTime) / (1024 * 1024) : 0;
    progressData->lastTime = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime.time_since_epoch()).count();

    progressData->downloaded = dlnow;
    progressData->totalSize = dltotal;

    std::cout << "Descargado: " << percentage << "% | Velocidad: " << downloadSpeed << " MB/s\r" << std::flush;

    return 0;
}

bool downloadFile(const std::string& url, const std::string& filename) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "Error al inicializar libcurl." << std::endl;
        return false;
    }

    FILE* outfile = fopen(filename.c_str(), "wb");
    if (!outfile) {
        std::cerr << "Error al abrir el archivo de salida." << std::endl;
        curl_easy_cleanup(curl);
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, outfile);

    ProgressData progressData = {0.0, 0.0, 0.0};
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progressCallback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progressData);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        std::cerr << "Error al descargar el archivo: " << curl_easy_strerror(res) << std::endl;
        fclose(outfile);
        curl_easy_cleanup(curl);
        return false;
    }

    fclose(outfile);
    curl_easy_cleanup(curl);

    std::cout << "Descarga completada.                             " << std::endl;

    return true;
}
