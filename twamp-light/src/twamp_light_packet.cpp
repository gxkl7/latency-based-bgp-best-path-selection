#include "twamp_light.hpp"

//constructor definiton
TwampLightPacket::TwampLightPacket(uint32_t seq, uint64_t sender_ts, uint64_t receiver_ts)
{
    sequence_number = seq;
    sender_timestamp = sender_ts;
    receiver_timestamp = receiver_ts;
}
//serialize definiton
std::vector<uint8_t> TwampLightPacket::serialize() const {
    //vector of 20 bytes
    //4 for the sequence number
    //8 for the sender timestamp
    //8 for the receiver timestamp.
    std::vector<uint8_t> buffer(20);
    //interges to big endian byte order
    uint32_t seq_net = htobe32(sequence_number);
    uint64_t sender_ts_net = htobe64(sender_timestamp);
    uint64_t receiver_ts_net = htobe64(receiver_timestamp);
    //copy the byte stream to buffer
    memcpy(buffer.data(), &seq_net, 4);
    memcpy(buffer.data() + 4, &sender_ts_net, 8);
    memcpy(buffer.data() + 12, &receiver_ts_net, 8);
    return buffer;
}
//derialize definiton
TwampLightPacket TwampLightPacket::deserialize(const uint8_t* data, size_t length) {
    uint32_t seq_net;
    uint64_t sender_ts_net, receiver_ts_net;
    memcpy(&seq_net, data, 4);
    memcpy(&sender_ts_net, data + 4, 8);
    memcpy(&receiver_ts_net, data + 12, 8);
    uint32_t seq = ntohl(seq_net);
    uint64_t sender_ts = be64toh(sender_ts_net);
    uint64_t receiver_ts = be64toh(receiver_ts_net);
    return TwampLightPacket(seq, sender_ts, receiver_ts);
}