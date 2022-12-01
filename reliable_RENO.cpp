#include <thread>
#include <condition_variable>
#include <utility>
#include <deque>
#include "log.h"
#include "packet.h"
#include "reliable_RENO.h"

const static auto waitTime = std::chrono::milliseconds(50);

class WindowRENO {

    // send & recv utility
    Unreliable &unreliable;

    // for RENO
    uint32_t prevAck;
    uint32_t duplicateCnt;
    float    cwnd;
    uint32_t threshold;

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
    WindowRENO(uint32_t base, uint32_t end, uint32_t threshold, Unreliable &unreliable)
            : base(base),
              end(end),
              unreliable(unreliable),
              cwnd(1),
              threshold(threshold),
              prevAck(-1),
              duplicateCnt(0) {

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

                    // for RENO (timeout)
                    this->threshold = this->cwnd / 2;
                    this->cwnd = 1;
                    this->duplicateCnt = 0;
                    this->prevAck = -1;
                    logRENO();
                    cvQueue.notify_all();
                }
            }
        });
    }

    void waitTimerToExit() {
        timeoutThread.join();
    }

    void push(std::unique_ptr<Packet> packet) {
        std::unique_lock lock(m);

        cvQueue.wait(lock, [this] { return queue.size() < cwnd; });

        LOG << "sent packet " << packet->num << std::endl;

        unreliable.send(packet);
        queue.push_back(std::move(packet));

        LOG << "after push, queue size = " << queue.size() << std::endl;
    }

    void recvAck(uint32_t ack) {
        std::lock_guard lock(m);

        LOG << "received ack " << ack << std::endl;

        // for RENO (fast retransmit)
        if (ack == prevAck) {

            duplicateCnt++;

            if (duplicateCnt == 3) {

                threshold = cwnd / 2;
                cwnd = threshold + 3;
                logRENO();
                cvQueue.notify_all();

                LOG << "fast retransmit" << std::endl;
                for (auto &packet: queue) {
                    if (packet->num == ack) {
                        unreliable.send(packet);
                        break;
                    }
                }

            } else if (duplicateCnt > 3) {

                cwnd++;
                logRENO();
                cvQueue.notify_all();

            }
        } else {
            duplicateCnt = 0;

            if (cwnd < threshold) {
                cwnd++;
            } else {
                cwnd += 1 / cwnd;
            }
            logRENO();

        }
        prevAck = ack;


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

    void logRENO() const {
        LOG << "RENO: " << "cwnd: " << cwnd << " threshold: " << threshold << std::endl;
    }
};

ReliableRENO::ReliableRENO(Unreliable unreliable)
        : unreliable(std::move(unreliable)) {}

bool ReliableRENO::send(uint8_t *buf, int len) {
    const int dataSize = MAX_PACKET_SIZE - sizeof(Packet);
    uint32_t seq = 0;
    uint32_t end = ROUND_UP(len, dataSize) / dataSize;

    WindowRENO window(seq, end, 16, unreliable);

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

int ReliableRENO::recv(uint8_t *buf, int len) {
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

            seq++;
            LOG << "sending ACK: " << seq << std::endl;
            unreliable.send(PacketHelper::makePacket(PacketType::ACK, seq));

        } else if (packet &&
                   PacketHelper::isValidPacket(packet) &&
                   packet->type == PacketType::FIN) {

            LOG << "received FIN" << std::endl;

            LOG << "sending FIN_ACK" << std::endl;

            unreliable.send(PacketHelper::makePacket(PacketType::FIN_ACK));
            break;
        } else {

            LOG << "received invalid packet" << std::endl;

        }
    }

    return curr - buf;
}

