#ifndef RELIABLE_OVER_UDP_PACKET_H
#define RELIABLE_OVER_UDP_PACKET_H

#include <cstdint>
#include <type_traits>
#include <memory>

#define MAX_PACKET_SIZE (10240)

#pragma pack(push, 1)

enum class PacketType : uint16_t {
    DATA,
    ACK,
    SYN,
    SYN_ACK,
    FIN,
    FIN_ACK,
};

struct Packet {
    // header
    PacketType type;
    uint16_t checksum;
    uint32_t num;
    uint32_t len;

    // data (if type is DATA)
    uint8_t data[0];
};

static_assert(std::is_trivial_v<Packet>);

namespace PacketHelper {
    bool isValidPacket(const std::unique_ptr<Packet> &packet);

    std::unique_ptr<Packet> makePacket(
            PacketType type,
            uint32_t num = 0, /* seq or ack */
            const void *data = nullptr,
            uint32_t len = 0
    );
}

#pragma pack(pop)

#endif //RELIABLE_OVER_UDP_PACKET_H
