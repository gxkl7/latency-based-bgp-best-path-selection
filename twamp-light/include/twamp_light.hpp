#pragma once
#include <iostream>
#include <cstdint>
#include <unistd.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <chrono>
#include <random>
#include <thread>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <ctime>
#include <iomanip>
#include <sstream>

#if (defined(__APPLE__))
    #include <libkern/OSByteOrder.h>
    #define htobe64(x) OSSwapHostToBigInt64(x)
    #define be64toh(x) OSSwapBigToHostInt64(x)
    #define htobe32(x) OSSwapHostToBigInt32(x)
    #define be64t32(x) OSSwapBigToHostInt32(x)
#elif (defined(__linux__))
    #include <endian.h>
#endif

inline uint64_t get_current_time_ns(){
    return std::chrono::duration_cast<std::chrono::nanoseconds>
    (std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

inline std::string get_current_timestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::tm local_tm = *std::localtime(&now_c);
    std::ostringstream oss;
    oss << std::put_time(&local_tm, "%F %T");
    return oss.str();
}

class TwampLightPacket{
    public:
    //packet contents
    uint32_t sequence_number;
    uint64_t sender_timestamp;
    uint64_t receiver_timestamp;
    //function to serialize the packet data into byte stream
    std::vector<uint8_t> serialize() const;
    //function to deserialize
    static TwampLightPacket deserialize(const uint8_t* data, size_t length);
    //constructor
    TwampLightPacket(uint32_t seq = 0, uint64_t sender_ts = 0, uint64_t receiver_ts = 0);
};

class TwampLightReflector{
    public:
    TwampLightReflector(std::string ip, uint16_t port);
    void run();

    private:
    uint16_t listen_port;
    int sockfd;
    std::string ipaddr;
};

class TwampLightSender{
    public:
    TwampLightSender(const std::string& reflector_ip, uint16_t reflector_port);
    std::unordered_map<std::string, double> run(int num_packets, int interval_ms, int timeout_interval_ms);

    private:
    std::string reflector_ip;
    uint16_t reflector_port;
    int sockfd;
};