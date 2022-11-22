#include <iostream>
#include "log.h"
#include "unreliable.h"

Unreliable::Unreliable(SOCKET s, const std::string &ip, uint16_t port) {
    this->s = s;

    remoteAddr.sin_family = AF_INET;
    remoteAddr.sin_port = htons(port);
    remoteAddr.sin_addr.s_addr = inet_addr(ip.c_str());
}

Unreliable::Unreliable(SOCKET s)
        : Unreliable(s, "0.0.0.0", 0) {}

Unreliable::Unreliable(Unreliable &&obj) {
    s = obj.s;
    remoteAddr = obj.remoteAddr;
    obj.s = INVALID_SOCKET;
}

Unreliable &Unreliable::operator=(Unreliable &&obj) {
    s = obj.s;
    remoteAddr = obj.remoteAddr;
    obj.s = INVALID_SOCKET;
    return *this;
}

Unreliable::~Unreliable() {
    if (s != INVALID_SOCKET) {
        closesocket(s);
    }
}

bool Unreliable::send(void *buf, int len) {

    int result = sendto(s, (char *) buf, len, 0, (sockaddr *) &remoteAddr, sizeof(remoteAddr));
    if (result == SOCKET_ERROR) {
        LOG << "sendto() failed: " << WSAGetLastError() << std::endl;
        return false;
    }

    return true;
}

bool Unreliable::send(const std::unique_ptr<Packet> &packet) {
    return send(packet.get(), packet->len);
}

bool Unreliable::recv(void *buf, int len) {
    sockaddr_in senderAddr;
    int addr_len = sizeof(senderAddr);
    int result = recvfrom(s, (char *) buf, len, 0, (sockaddr *) &senderAddr, &addr_len);
    if (result == SOCKET_ERROR) {
        LOG << "recvfrom() failed: " << WSAGetLastError() << std::endl;
        return false;
    }

    // the first received packet
    if (remoteAddr.sin_addr.s_addr == ADDR_ANY) {
        remoteAddr = senderAddr;
        LOG << "sender from : " << inet_ntoa(remoteAddr.sin_addr)
            << ":" << ntohs(remoteAddr.sin_port) << std::endl;
        return true;
    }

    // not the first time receive packet
    if (senderAddr.sin_addr.s_addr != remoteAddr.sin_addr.s_addr ||
        senderAddr.sin_port != remoteAddr.sin_port) {
        LOG << "recvfrom() failed: sender address mismatch" << std::endl;
        return false;
    }

    // all check passed
    return true;
}

std::unique_ptr<Packet> Unreliable::recv() {
    auto packet = reinterpret_cast<Packet *>(new uint8_t[MAX_PACKET_SIZE]{});
    if (!recv(packet, MAX_PACKET_SIZE) ||
        packet->len > MAX_PACKET_SIZE ||
        packet->len == 0) {
        return nullptr;
    }

    return std::unique_ptr<Packet>(packet);
}