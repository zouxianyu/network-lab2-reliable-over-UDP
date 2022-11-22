#ifndef RELIABLE_OVER_UDP_RELIABLE_GBN_H
#define RELIABLE_OVER_UDP_RELIABLE_GBN_H

#include "unreliable.h"
#include "reliable_interface.h"

class ReliableGBN : public IReliable {
    Unreliable unreliable;
public:
    ReliableGBN(Unreliable unreliable);

    bool send(uint8_t *buf, int len) override;

    int recv(uint8_t *buf, int len) override;
};

#endif //RELIABLE_OVER_UDP_RELIABLE_GBN_H
