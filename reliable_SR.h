#ifndef RELIABLE_OVER_UDP_RELIABLE_SR_H
#define RELIABLE_OVER_UDP_RELIABLE_SR_H

#include <string>
#include <cstddef>
#include "reliable_interface.h"

class ReliableSR : public IReliable {
    Unreliable unreliable;
public:
    ReliableSR(Unreliable unreliable);

    bool send(uint8_t *buf, int len) override;

    int recv(uint8_t *buf, int len) override;
};

#endif //RELIABLE_OVER_UDP_RELIABLE_SR_H
