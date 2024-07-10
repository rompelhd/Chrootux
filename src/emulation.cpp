#include <iostream>
#include <cstdlib>
#include <string>

void run_arm64_binary(const std::string& binary_path) {
    std::string command = "qemu-aarch64 -L /data/data/com.termux/files/home/Chrootux/temp/ " + binary_path;
    int result = std::system(command.c_str());

    if (result == 0) {
        std::cout << "Binary executed successfully." << std::endl;
    } else {
        std::cerr << "Failed to execute binary." << std::endl;
    }
}

int main() {
    std::string binary_path = "/data/data/com.termux/files/home/Chrootux/ls";
    run_arm64_binary(binary_path);
    return 0;
}


// Testing
