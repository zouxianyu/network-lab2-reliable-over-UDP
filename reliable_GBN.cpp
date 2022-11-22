#include <thread>
#include <condition_variable>
#include <utility>
#include "log.h"
#include "reliable_GBN.h"

ReliableGBN::ReliableGBN(Unreliable unreliable)
        : unreliable(std::move(unreliable)) {}

inline static uint32_t seqFlip(uint32_t seq) {
    return (~seq) & 1;
}

bool ReliableGBN::send(uint8_t *buf, int len) {
    const auto waitTime = std::chrono::milliseconds(50);
    const int dataSize = MAX_PACKET_SIZE - sizeof(Packet);

    uint32_t seq = 0;
    for (uint8_t *sliceBuf = buf;
         sliceBuf < buf + len;
         sliceBuf += dataSize, seq = seqFlip(seq)) {

        int sliceLen = (std::min)(static_cast<int>(len - (sliceBuf - buf)), dataSize);

        std::mutex m;
        std::condition_variable cv;
        bool ackReceived = false;

        std::thread sender([this, &m, &cv, &ackReceived, seq, sliceBuf, sliceLen, waitTime] {
            std::unique_lock lock(m);
            do {
                LOG << "sending slice " << seq << std::endl;
                unreliable.send(PacketHelper::makePacket(
                        PacketType::DATA,
                        seq,
                        sliceBuf,
                        sliceLen
                ));
            } while (!cv.wait_for(lock, waitTime,[&]{return ackReceived;}));
            LOG << "slice " << seq << " sent successfully" << std::endl;
        });

        std::thread ackReceiver([this, &m, &cv, &ackReceived, seq] {
            while (true) {
                auto packet = unreliable.recv();
                if (packet &&
                    PacketHelper::isValidPacket(packet) &&
                    packet->type == PacketType::ACK &&
                    packet->num == seq) {
                    std::lock_guard lock(m);
                    LOG << "received ACK " << seq << std::endl;
                    ackReceived = true;
                    cv.notify_one();
                    break;
                }
            }
        });

        sender.join();
        ackReceiver.join();

    }

    if (!unreliable.send(PacketHelper::makePacket(PacketType::FIN))) {
        LOG << "failed to send FIN" << std::endl;
        return false;
    }

    std::unique_ptr<Packet> packet = unreliable.recv();

    if (packet == nullptr ||
        !PacketHelper::isValidPacket(packet) ||
        packet->type != PacketType::FIN_ACK) {
        LOG << "failed to receive FIN_ACK" << std::endl;
        return false;
    }

    LOG << "sent all slices successfully" << std::endl;

    return true;
}

int ReliableGBN::recv(uint8_t *buf, int len) {
    uint32_t seq = 0;
    uint8_t *curr = buf;
    while (true) {
        if (curr >= buf + len) {
            LOG << "buffer overflow" << std::endl;
            throw std::runtime_error("buffer overflow");
        }

        LOG << "waiting for slice " << seq << std::endl;

        auto packet = unreliable.recv();
        if (packet &&
            PacketHelper::isValidPacket(packet) &&
            packet->type == PacketType::DATA &&
            packet->num == seq) {

            LOG << "received slice " << seq << std::endl;

            int sliceLen = packet->len - sizeof(Packet);
            memcpy(curr, packet->data, sliceLen);
            curr += sliceLen;

            LOG << "sending ACK " << seq << std::endl;

            unreliable.send(PacketHelper::makePacket(PacketType::ACK, seq));
            seq = seqFlip(seq);
        } else if (packet &&
                   PacketHelper::isValidPacket(packet) &&
                   packet->type == PacketType::FIN) {

            LOG << "received FIN" << std::endl;

            LOG << "sending FIN_ACK" << std::endl;

            unreliable.send(PacketHelper::makePacket(PacketType::FIN_ACK));
            break;
        } else {

            LOG << "received invalid packet" << std::endl;

            LOG << "sending another ACK seq" << std::endl;

            unreliable.send(PacketHelper::makePacket(PacketType::ACK, seqFlip(seq)));
        }
    }

    return curr - buf;
}

