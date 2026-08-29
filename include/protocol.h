#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <cstdint>
#include <cstring>
#include <arpa/inet.h>
#include <sys/types.h>

// Wire message for a bid. We deliberately do NOT send a raw C++ struct
// over the socket the way the original version did: struct padding, the
// size of pid_t, and byte order are not guaranteed to match across
// compilers or architectures. Instead we serialize into a fixed 12-byte
// buffer using network byte order, the same way any real wire protocol
// would, so this would still work correctly between two different
// machines/compilers.
struct BidMessage {
    int32_t process_id;
    int32_t bid_value;
    int32_t remaining_work_ms;
};

constexpr size_t BID_MESSAGE_WIRE_SIZE = 12; // 3 x int32_t, explicitly packed

inline void serializeBid(const BidMessage& msg, uint8_t* buffer) {
    uint32_t netPid = htonl(static_cast<uint32_t>(msg.process_id));
    uint32_t netBid = htonl(static_cast<uint32_t>(msg.bid_value));
    uint32_t netWork = htonl(static_cast<uint32_t>(msg.remaining_work_ms));
    std::memcpy(buffer, &netPid, 4);
    std::memcpy(buffer + 4, &netBid, 4);
    std::memcpy(buffer + 8, &netWork, 4);
}

inline BidMessage deserializeBid(const uint8_t* buffer) {
    uint32_t netPid, netBid, netWork;
    std::memcpy(&netPid, buffer, 4);
    std::memcpy(&netBid, buffer + 4, 4);
    std::memcpy(&netWork, buffer + 8, 4);
    BidMessage msg;
    msg.process_id = static_cast<int32_t>(ntohl(netPid));
    msg.bid_value = static_cast<int32_t>(ntohl(netBid));
    msg.remaining_work_ms = static_cast<int32_t>(ntohl(netWork));
    return msg;
}

#endif
