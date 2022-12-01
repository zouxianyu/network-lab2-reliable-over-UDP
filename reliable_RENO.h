#ifndef RELIABLE_OVER_UDP_RELIABLE_RENO_H
#define RELIABLE_OVER_UDP_RELIABLE_RENO_H

#include "unreliable.h"
#include "reliable_interface.h"

class ReliableRENO : public IReliable {
    Unreliable unreliable;
public:
    ReliableRENO(Unreliable unreliable);

    bool send(uint8_t *buf, int len) override;

    int recv(uint8_t *buf, int len) override;
};

#endif //RELIABLE_OVER_UDP_RELIABLE_RENO_H
