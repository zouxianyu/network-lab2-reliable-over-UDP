#ifndef RELIABLE_OVER_UDP_UNRELIABLE_H
#define RELIABLE_OVER_UDP_UNRELIABLE_H

#include <string>
#include <memory>
#include <cstddef>
#include <winsock2.h>
#include "packet.h"

class Unreliable {
    SOCKET s;
    sockaddr_in remoteAddr{};
public:
    Unreliable(SOCKET s, const std::string &ip, uint16_t port);

    Unreliable(SOCKET s);

    ~Unreliable();

    Unreliable(const Unreliable &) = delete;

    Unreliable &operator=(const Unreliable &) = delete;

    Unreliable(Unreliable &&obj);

    Unreliable &operator=(Unreliable &&obj);

    bool send(void *buf, int len);

    bool send(const std::unique_ptr<Packet> &packet);

    bool recv(void *buf, int len);

    std::unique_ptr<Packet> recv();
};

#endif //RELIABLE_OVER_UDP_UNRELIABLE_H
