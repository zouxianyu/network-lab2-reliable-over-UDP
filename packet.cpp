#include "packet.h"

#define ROUND_UP(a, b) (((uint32_t)(a) + ((uint32_t)(b) - 1)) & ~((uint32_t)(b) - 1))

static uint16_t checksum(void *buf, int len) {
    uint32_t sum = 0;
    const uint16_t *p = reinterpret_cast<uint16_t *>(buf);
    for (int i = 0; i < len / 2; i++) {
        sum += p[i];
        if (sum & 0xFFFF0000) {
            sum &= 0xFFFF;
            sum++;
        }
    }
    return ~(sum & 0xFFFF);
}

bool PacketHelper::isValidPacket(const std::unique_ptr<Packet> &packet) {
    return checksum(packet.get(), ROUND_UP(packet->len, sizeof(uint16_t))) == 0;
}

std::unique_ptr<Packet> PacketHelper::makePacket(
        PacketType type,
        uint32_t num,
        const void *data,
        uint32_t len
) {
    // do round up, so we can calculate checksum
    int alignedPacketSize = ROUND_UP(sizeof(Packet) + len, sizeof(uint16_t));

    // allocate memory and clear it to all 0
    auto packet = reinterpret_cast<Packet *>(new uint8_t[alignedPacketSize]{});

    // fill packet
    packet->type = type;
    packet->num = num;
    packet->len = sizeof(Packet) + len;
    if (type == PacketType::DATA) {
        memcpy(packet->data, data, len);
    }

    // calc checksum
    // first set checksum to 0
    packet->checksum = 0;
    // calculate it now
    packet->checksum = checksum(packet, alignedPacketSize);

    return std::unique_ptr<Packet>(packet);
}
