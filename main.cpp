#include <iostream>
#include <thread>
#include <winsock2.h>
#include "log.h"
#include "reliable.h"

int main(int argc, char *argv[]) {
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        std::cout << "WSAStartup failed: " << result << std::endl;
        return 1;
    }

    // sender
    // program.exe server <port>
    if (argc == 3 && std::string_view(argv[1]) == "server") {
        // arg parse
        uint16_t port = std::stoi(argv[2]);

        const auto sendBufferSize = MAX_PACKET_SIZE * 10;
        auto mem = std::make_unique<uint8_t[]>(sendBufferSize);
        memset(mem.get(), 0, sendBufferSize);
        Reliable reliable = ReliableHelper::listen(port);
        reliable.send(mem.get(), sendBufferSize);
    }

    // receiver
    // program.exe client <server ip> <server port>
    if (argc == 4 && std::string_view(argv[1]) == "client") {
        // arg parse
        std::string ip = argv[2];
        uint16_t port = std::stoi(argv[3]);

        const auto recvBufferSize = MAX_PACKET_SIZE * 20;
        auto mem = std::make_unique<uint8_t[]>(recvBufferSize);
        memset(mem.get(), 0xff, recvBufferSize);
        Reliable reliable = ReliableHelper::connect(ip, port);
        int received = reliable.recv(mem.get(), recvBufferSize);
        LOG << "received " << received << " bytes" << std::endl;
    }

    WSACleanup();
    return 0;
}
