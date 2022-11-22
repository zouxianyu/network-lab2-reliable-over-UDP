#ifndef RELIABLE_OVER_UDP_RELIABLE_INTERFACE_H
#define RELIABLE_OVER_UDP_RELIABLE_INTERFACE_H

#include <string>
#include <cstddef>
#include "unreliable.h"

class IReliable {
public:
    virtual bool send(uint8_t *buf, int len) = 0;

    virtual int recv(uint8_t *buf, int len) = 0;

    virtual ~IReliable() = default;
};

#endif //RELIABLE_OVER_UDP_RELIABLE_INTERFACE_H
