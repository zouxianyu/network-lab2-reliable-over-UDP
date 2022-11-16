#ifndef RELIABLE_OVER_UDP_RELIABLE_H
#define RELIABLE_OVER_UDP_RELIABLE_H

#include <string>
#include <cstddef>
#include "unreliable.h"

class Reliable {
    Unreliable unreliable;
public:
    Reliable(Unreliable unreliable);

    bool send(uint8_t *buf, int len);

    int recv(uint8_t *buf, int len);
};

namespace ReliableHelper {
    Reliable listen(uint16_t port);
}

namespace ReliableHelper {
    Reliable connect(const std::string &ip, uint16_t port);
}

#endif //RELIABLE_OVER_UDP_RELIABLE_H
