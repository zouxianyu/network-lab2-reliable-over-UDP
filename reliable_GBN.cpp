#include <thread>
#include <condition_variable>
#include <utility>
#include <deque>
#include "log.h"
#include "reliable_GBN.h"

const auto waitTime = std::chrono::milliseconds(50);
const auto sendAckDelay = std::chrono::milliseconds(10);
const uint32_t N = 3;

class Window {
    // window size
    const uint32_t N;

    // send & recv utility
    Unreliable &unreliable;

    // window queue
    uint32_t base;
    std::deque<std::unique_ptr<Packet>> queue;
    std::mutex m;
    std::condition_variable cvQueue;

    // timeout
    std::thread timeoutThread;
    std::condition_variable cvTimeout;

    // status
    std::atomic_bool exit;
public:
    Window(uint32_t base, uint32_t N, Unreliable &unreliable)
            : base(base), N(N), unreliable(unreliable), exit(false) {

        // create timeout thread, for resending packets
        timeoutThread = std::thread([this, &unreliable] {
            std::unique_lock lock(m);
            while (!exit) {
                // set timer
                if (cvTimeout.wait_for(lock, waitTime) == std::cv_status::timeout &&
                    !queue.empty()) {
                    LOG << "timeout" << std::endl;
                    for (auto &packet: queue) {
                        unreliable.send(packet);
                    }
                }
            }
        });
    }

    void stopTimer() {
        // stop timeout thread
        exit = true;
        cvTimeout.notify_one();
        timeoutThread.join();
    }

    void waitForEmpty() {
        std::unique_lock lock(m);
        cvQueue.wait(lock, [this] { return queue.empty(); });
    }

    void push(std::unique_ptr<Packet> packet) {
        std::unique_lock lock(m);

        cvQueue.wait(lock, [this] { return queue.size() < N; });

        LOG << "sent packet " << packet->num << std::endl;

        unreliable.send(packet);
        queue.push_back(std::move(packet));
    }

    void recvAck(uint32_t ack) {
        std::lock_guard lock(m);

        LOG << "received ack " << ack << std::endl;

        // TODO: 解决累计确认问题
        if (ack - 1 == base) {

            LOG << "move window" << std::endl;

            queue.pop_front();
            base++;
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

    Window window(seq, N, unreliable);

    std::thread ackReceiver([this, &window] {
        while (true) {
            auto packet = unreliable.recv();
            if (packet &&
                PacketHelper::isValidPacket(packet) &&
                packet->type == PacketType::ACK) {
                window.recvAck(packet->num);
            }
        }
    });

    ackReceiver.detach();

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

    window.waitForEmpty();

    window.stopTimer();

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
    std::mutex m;
    std::atomic_bool exit = false;

    std::thread t([this, &m, &seq, &exit]{
        while (!exit) {
            std::this_thread::sleep_for(sendAckDelay);

            std::lock_guard lock(m);

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

        std::lock_guard lock(m);

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
            t.join();
            break;
        } else {

            LOG << "received invalid packet" << std::endl;

        }
    }

    return curr - buf;
}

