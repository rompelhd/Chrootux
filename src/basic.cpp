#include "basic.hpp"
#include <cstdlib>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>

bool debug = true;
std::string machines_folder;

void printDebugMessage(const std::string& message) {
    if (debug) {
        std::cerr << "[DEBUG] " << message << std::endl;
    }
}

void setTermuxPath() {
    const char* termuxPath = "/data/data/com.termux/files/usr/bin";
    const char* currentPath = std::getenv("PATH");

    if (currentPath) {
        std::string newPath = termuxPath;
        newPath += ":";
        newPath += currentPath;
        setenv("PATH", newPath.c_str(), 1);
    } else {
        setenv("PATH", termuxPath, 1);
    }
}

bool isTermux() {
    struct stat st;
    if (stat("/data/data/com.termux/files/home", &st) == 0) {
        return true;
    } else {
        return false;
    }
}

std::string createMachinesFolderIfNotExists(const std::string& home_dir) {
    if (isTermux()) {
        setTermuxPath();
        std::string home_folder = "/data/data/com.termux/files/home/";
        machines_folder = home_folder + "machines";
    } else {
        std::cout << "No estás en Termux." << std::endl;
        return "";
    }

    struct stat info;
    if (stat(machines_folder.c_str(), &info) != 0) {
        if (!S_ISDIR(info.st_mode)) {
            std::cerr << "El path '" << machines_folder << "' existe pero no es un directorio." << std::endl;
            return "";
        }
    } else {
        printDebugMessage("La carpeta 'machines' ya existe en: " + machines_folder);
        return machines_folder;
    }

    if (!machines_folder.empty()) {
        if (mkdir(machines_folder.c_str(), 0755) == 0) {
            std::cout << "Se ha creado la carpeta 'machines' en: " << machines_folder << std::endl;
            return machines_folder;
        } else {
            std::cerr << "Error al crear la carpeta 'machines' en: " << machines_folder << std::endl;
            return "";
        }
    } else {
        std::cerr << "No se pudo obtener la ruta de la carpeta 'machines'." << std::endl;
        return "";
    }
}
