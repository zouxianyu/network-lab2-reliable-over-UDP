#include <iostream>
#include <fstream>
#include <thread>
#include <winsock2.h>
#include "log.h"
#include "reliable_GBN.h"
#include "reliable_helper.h"

int main(int argc, char *argv[]) {
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        std::cout << "WSAStartup failed: " << result << std::endl;
        return 1;
    }

    // sender
    // program.exe server <port> <filename>
    if (argc == 4 && std::string_view(argv[1]) == "server") {
        // arg parse
        uint16_t port = std::stoi(argv[2]);
        std::string filename = argv[3];

        // open file
        std::ifstream f(filename, std::ios::binary);
        if (!f.is_open()) {
            std::cout << "file not found: " << filename << std::endl;
            return 1;
        }

        // get file size
        f.seekg(0, std::ios::end);
        int fileSize = f.tellg();

        // read file
        auto mem = std::make_unique<uint8_t[]>(fileSize);
        f.seekg(0, std::ios::beg);
        f.read((char *) mem.get(), fileSize);

        // send file
        std::unique_ptr<IReliable> reliable = ReliableHelper::listen<ReliableGBN>(port);
        reliable->send(mem.get(), fileSize);
    }

    // receiver
    // program.exe client <server ip> <server port> <filename>
    if (argc == 5 && std::string_view(argv[1]) == "client") {
        // arg parse
        std::string ip = argv[2];
        uint16_t port = std::stoi(argv[3]);
        std::string filename = argv[4];

        const auto recvBufferSize = 20 * 1024 * 1024; // 20M
        auto mem = std::make_unique<uint8_t[]>(recvBufferSize);
        memset(mem.get(), 0xff, recvBufferSize);
        std::unique_ptr<IReliable> reliable = ReliableHelper::connect<ReliableGBN>(ip, port);
        int received = reliable->recv(mem.get(), recvBufferSize);
        LOG << "received " << received << " bytes" << std::endl;

        // write to file
        std::ofstream f(filename, std::ios::binary);
        f.write((char *) mem.get(), received);
    }

    WSACleanup();
    return 0;
}
