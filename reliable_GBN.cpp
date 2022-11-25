#include <thread>
#include <condition_variable>
#include <utility>
#include <deque>
#include "log.h"
#include "packet.h"
#include "reliable_GBN.h"

const static auto waitTime = std::chrono::milliseconds(50);
const static auto sendAckDelay = std::chrono::milliseconds(10);
const static uint32_t N = 3;

class WindowGBN {
    // window size
    const uint32_t N;

    // send & recv utility
    Unreliable &unreliable;

    // window queue
    uint32_t base;
    uint32_t end;
    std::deque<std::unique_ptr<Packet>> queue;
    std::mutex m;
    std::condition_variable cvQueue;

    // timeout
    std::thread timeoutThread;
    std::condition_variable cvTimeout;

public:
    WindowGBN(uint32_t base, uint32_t end, uint32_t N, Unreliable &unreliable)
            : base(base), end(end), N(N), unreliable(unreliable) {

        // create timeout thread, for resending packets
        timeoutThread = std::thread([this] {
            std::unique_lock lock(m);
            while (true) {
                // set timer
                if (cvTimeout.wait_for(lock, waitTime) == std::cv_status::timeout) {

                    if (this->base == this->end) {
                        break;
                    }

                    LOG << "timeout" << std::endl;
                    for (auto &packet: queue) {
                        this->unreliable.send(packet);
                    }
                }
            }
        });
    }

    void waitTimerToExit() {
        timeoutThread.join();
    }

    void push(std::unique_ptr<Packet> packet) {
        std::unique_lock lock(m);

        cvQueue.wait(lock, [this] { return queue.size() < N; });

        LOG << "sent packet " << packet->num << std::endl;

        unreliable.send(packet);
        queue.push_back(std::move(packet));

        LOG << "after push, queue size = " << queue.size() << std::endl;
    }

    void recvAck(uint32_t ack) {
        std::lock_guard lock(m);

        LOG << "received ack " << ack << std::endl;

        if (base < ack) {
            while (base < ack) {
                LOG << "move window" << std::endl;

                queue.pop_front();
                base++;
            }
            LOG << "after move, queue size = " << queue.size() << std::endl;
            cvTimeout.notify_all(); // reset timer
            cvQueue.notify_all(); // send next packet / notify finished
        }
    }
};

ReliableGBN::ReliableGBN(Unreliable unreliable)
        : unreliable(std::move(unreliable)) {}

bool ReliableGBN::send(uint8_t *buf, int len) {
    const int dataSize = MAX_PACKET_SIZE - sizeof(Packet);
    uint32_t seq = 0;
    uint32_t end = ROUND_UP(len, dataSize) / dataSize;

    WindowGBN window(seq, end, N, unreliable);

    std::thread ackReceiver([this, &window, &end] {
        while (true) {
            auto packet = unreliable.recv();
            if (packet &&
                PacketHelper::isValidPacket(packet) &&
                packet->type == PacketType::ACK) {
                window.recvAck(packet->num);

                if (packet->num == end) {
                    break;
                }
            }
        }
    });

    for (uint8_t *sliceBuf = buf;
         sliceBuf < buf + len;
         sliceBuf += dataSize, seq++) {

        int sliceLen = (std::min)(static_cast<int>(len - (sliceBuf - buf)), dataSize);

        window.push(PacketHelper::makePacket(
                PacketType::DATA,
                seq,
                sliceBuf,
                sliceLen
        ));
    }

    ackReceiver.join();
    window.waitTimerToExit();

    LOG << "sending FIN" << std::endl;
    if (!unreliable.send(PacketHelper::makePacket(PacketType::FIN))) {
        LOG << "failed to send FIN" << std::endl;
        return false;
    }

    LOG << "receiving FIN_ACK" << std::endl;
    std::unique_ptr<Packet> packet = unreliable.recv();

    if (packet == nullptr ||
        !PacketHelper::isValidPacket(packet) ||
        packet->type != PacketType::FIN_ACK) {
        return false;
    }

    return true;
}

int ReliableGBN::recv(uint8_t *buf, int len) {
    uint32_t seq = 0;
    bool exit = false;
    std::mutex m;

    std::thread t([this, &m, &seq, &exit] {
        while (true) {
            std::this_thread::sleep_for(sendAckDelay);

            std::lock_guard lock(m);
            if (exit) {
                break;
            }

            LOG << "sending ACK " << seq << std::endl;

            unreliable.send(PacketHelper::makePacket(PacketType::ACK, seq));
        }
    });

    uint8_t *curr = buf;
    while (true) {
        if (curr >= buf + len) {
            LOG << "buffer overflow" << std::endl;
            throw std::runtime_error("buffer overflow");
        }

        LOG << "waiting for slice " << seq << std::endl;

        auto packet = unreliable.recv();

        std::unique_lock lock(m);

        if (packet &&
            PacketHelper::isValidPacket(packet) &&
            packet->type == PacketType::DATA &&
            packet->num == seq) {

            LOG << "received slice " << seq << std::endl;

            int sliceLen = packet->len - sizeof(Packet);
            memcpy(curr, packet->data, sliceLen);
            curr += sliceLen;

            seq++;
        } else if (packet &&
                   PacketHelper::isValidPacket(packet) &&
                   packet->type == PacketType::FIN) {

            LOG << "received FIN" << std::endl;

            LOG << "sending FIN_ACK" << std::endl;

            unreliable.send(PacketHelper::makePacket(PacketType::FIN_ACK));
            exit = true;
            lock.unlock();
            t.join();
            break;
        } else {

            LOG << "received invalid packet" << std::endl;

        }
    }

    return curr - buf;
}

