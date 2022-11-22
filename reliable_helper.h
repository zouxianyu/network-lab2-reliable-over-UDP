#ifndef RELIABLE_OVER_UDP_RELIABLE_HELPER_H
#define RELIABLE_OVER_UDP_RELIABLE_HELPER_H

#include <string>
#include <memory>
#include <cstdint>
#include <stdexcept>
#include <iostream>
#include "log.h"
#include "reliable_interface.h"

namespace ReliableHelper {

    template <typename Ty>
    typename std::enable_if_t<std::is_base_of_v<IReliable, Ty>, std::unique_ptr<IReliable>>
    listen(uint16_t port) {
        SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (s == INVALID_SOCKET) {
            LOG << "socket() failed: " << WSAGetLastError() << std::endl;
            throw std::runtime_error("socket() failed");
        }

        sockaddr_in listenAddr;
        listenAddr.sin_family = AF_INET;
        listenAddr.sin_port = htons(port);
        listenAddr.sin_addr.s_addr = INADDR_ANY;

        if (bind(s, (sockaddr *) &listenAddr, sizeof(listenAddr)) == SOCKET_ERROR) {
            LOG << "bind() failed: " << WSAGetLastError() << std::endl;
            throw std::runtime_error("bind() failed");
        }

        Unreliable unreliable(s);

        // 1. recv SYN
        std::unique_ptr<Packet> packet = unreliable.recv();

        if (packet == nullptr ||
            !PacketHelper::isValidPacket(packet) ||
            packet->type != PacketType::SYN) {

            LOG << "failed to receive packet from client" << std::endl;
            throw std::runtime_error("failed to receive packet from client");
        }

        LOG << "received SYN from client" << std::endl;

        // 2. send SYN_ACK

        if (!unreliable.send(PacketHelper::makePacket(PacketType::SYN_ACK))) {
            LOG << "failed to send SYN_ACK to client" << std::endl;
            throw std::runtime_error("failed to send SYN_ACK to client");
        }

        LOG << "sent SYN_ACK to client" << std::endl;

        LOG << "connect established" << std::endl;
        return std::make_unique<Ty>(std::move(unreliable));
    }

    template <typename Ty>
    typename std::enable_if_t<std::is_base_of_v<IReliable, Ty>, std::unique_ptr<IReliable>>
    connect(const std::string &ip, uint16_t port) {
        SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (s == INVALID_SOCKET) {
            LOG << "socket() failed: " << WSAGetLastError() << std::endl;
            throw std::runtime_error("socket() failed");
        }

        // create UDP socket wrapper, given the remote addr (server addr)
        Unreliable unreliable(s, ip, port);

        // 1. send SYN

        if (!unreliable.send(PacketHelper::makePacket(PacketType::SYN))) {
            LOG << "failed to send SYN to server" << std::endl;
            throw std::runtime_error("failed to send SYN to server");
        }

        LOG << "sent SYN to server" << std::endl;

        // 2. recv SYN_ACK

        std::unique_ptr<Packet> packet = unreliable.recv();

        if (packet == nullptr ||
            !PacketHelper::isValidPacket(packet) ||
            packet->type != PacketType::SYN_ACK) {

            LOG << "failed to receive packet from server" << std::endl;
            throw std::runtime_error("failed to receive packet from server");
        }

        LOG << "received SYN_ACK from server" << std::endl;

        LOG << "connect established" << std::endl;

        return std::make_unique<Ty>(std::move(unreliable));
    }
}

#endif //RELIABLE_OVER_UDP_RELIABLE_HELPER_H
