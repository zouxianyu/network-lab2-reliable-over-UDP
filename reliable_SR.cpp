#include <thread>
#include <condition_variable>
#include <utility>
#include <deque>
#include <set>
#include <functional>
#include "log.h"
#include "unreliable.h"
#include "reliable_SR.h"

const static auto waitTime = std::chrono::milliseconds(50);
const static uint32_t N = 3;

struct Task {
    std::mutex m;
    std::condition_variable cv;
    bool ackReceived = false;
    std::function<void(std::shared_ptr<Task>)> sender;
};

class WindowSR {

    const uint32_t N;
    uint32_t base;
    uint32_t end;

    std::deque<std::shared_ptr<Task>> queue;
    std::mutex m;
    std::condition_variable cvQueue;
public:
    WindowSR(uint32_t base, uint32_t end, uint32_t N)
            : base(base), end(end), N(N) {}

    void push(std::shared_ptr<Task> task) {
        std::unique_lock lock(m);

        cvQueue.wait(lock, [this] { return queue.size() < N; });

        queue.push_back(task);

        std::thread senderThread(task->sender, task);
        senderThread.detach();
    }

    void recvAck(uint32_t ack) {
        std::lock_guard lock(m);

        // invalid ack
        if (ack < base || ack >= base + queue.size()) {
            return;
        }

        // notify sender thread, so the sender thread stop sending packet and exit
        const auto &currTask = queue.at(ack - base);
        {
            std::lock_guard taskLock(currTask->m);
            currTask->ackReceived = true;
            currTask->cv.notify_all();
        }

        // try to move window
        uint32_t moving = 0;
        for (const auto &task: queue) {
            if (task == currTask) {
                moving++;
                break;
            }

            std::lock_guard taskLock(task->m);
            if (task->ackReceived) {
                moving++;
            } else {
                break;
            }
        }

        if (moving > 0) {
            for (uint32_t i = 0; i < moving; i++) {
                LOG << "move window" << std::endl;
                base++;
                queue.pop_front();
            }
            cvQueue.notify_all();
        }
    }

};

ReliableSR::ReliableSR(Unreliable unreliable)
        : unreliable(std::move(unreliable)) {}

bool ReliableSR::send(uint8_t *buf, int len) {
    const int dataSize = MAX_PACKET_SIZE - sizeof(Packet);

    uint32_t seq = 0;
    uint32_t end = ROUND_UP(len, dataSize) / dataSize;

    WindowSR window(seq, end, N);

    std::set<uint32_t> allSeqs;
    for (uint32_t i = seq; i < end; i++) {
        allSeqs.insert(i);
    }

    std::thread ackReceiver([this, &window, &allSeqs] {
        while (true) {
            auto packet = unreliable.recv();
            if (packet &&
                PacketHelper::isValidPacket(packet) &&
                packet->type == PacketType::ACK) {

                LOG << "recveive ACK " << packet->num << std::endl;

                window.recvAck(packet->num);
                allSeqs.erase(packet->num);

                if (allSeqs.empty()) {
                    break;
                }
            } else {
                LOG << "invalid ACK" << std::endl;
            }
        }
        LOG << "receive ACK thread exit" << std::endl;
    });

    for (uint8_t *sliceBuf = buf;
         sliceBuf < buf + len;
         sliceBuf += dataSize, seq++) {

        int sliceLen = (std::min)(static_cast<int>(len - (sliceBuf - buf)), dataSize);

        auto task = std::make_shared<Task>();

        task->sender = [this, seq, sliceBuf, sliceLen](std::shared_ptr<Task> task) {
            std::unique_lock lock(task->m);
            do {
                LOG << "sending slice " << seq << std::endl;
                unreliable.send(PacketHelper::makePacket(
                        PacketType::DATA,
                        seq,
                        sliceBuf,
                        sliceLen
                ));
            } while (!task->cv.wait_for(lock, waitTime,
                                        [&] { return task->ackReceived; }));
            LOG << "slice " << seq << " sent successfully" << std::endl;
        };

        window.push(task);

    }

    // waiting for received all ACKs
    ackReceiver.join();

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

    LOG << "sent all slices successfully" << std::endl;

    return true;
}

int ReliableSR::recv(uint8_t *buf, int len) {
    const int dataSize = MAX_PACKET_SIZE - sizeof(Packet);
    uint32_t recvSize = 0;
    while (true) {

        auto packet = unreliable.recv();
        if (packet &&
            PacketHelper::isValidPacket(packet) &&
            packet->type == PacketType::DATA) {

            LOG << "received slice " << packet->num << std::endl;

            int sliceLen = packet->len - sizeof(Packet);
            memcpy(buf + packet->num * dataSize, packet->data, sliceLen);
            recvSize = (std::max)(recvSize, packet->num * dataSize + sliceLen);

            LOG << "sending ACK " << packet->num << std::endl;

            unreliable.send(PacketHelper::makePacket(PacketType::ACK, packet->num));
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

    return recvSize;
}

