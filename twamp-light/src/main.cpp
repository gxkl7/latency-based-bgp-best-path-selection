#include "twamp_light.hpp"

void print_usage() {
    std::cout << "\nusage:\n\n";
    std::cout << "\tIPv4 Address (Required)\n\n";
    std::cout << "\tPort Number (Required)\n\n";
    std::cout << "\t-f controller || reflector (Required)\n\n";
    std::cout << "\t-c packet_count (controller mode only)\n\n";
    std::cout << "\t-i interval_ms (controller mode only)\n\n";
    std::cout << "\t-t timeout_ms (controller mode only)\n\n";
    std::cout << "-h, --help Help string\n\n";
    std::cout << "defaults: packet_count=3, interval=10ms, timeout=1000ms\n\n";
}

int main(int argc, char* argv[]) {

    if (argc == 2 && (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help")) {
        print_usage();
        return 0;
    }
    else if (argc < 4) {
        std::cerr << "\nError: Missing arguments\n\n"; print_usage(); return 1;
    }

    std::string mode, ip;
    int port = 0, packet_count = 3, interval_ms = 10, timeout_ms = 1000;
    int i = 1;

    while (i < argc) {
        std::string arg = argv[i];
        if (arg == "-f" && i+1 < argc) mode = argv[++i];
        else if (ip.empty()) ip = arg;
        else if (port == 0) {
            try { port = std::stoi(arg); } 
            catch (...) { std::cerr << "\nError: Invalid port\n\n"; print_usage(); return 1; }
        } else break;
        ++i;
    }

    if (ip.empty() || port == 0 || (mode != "controller" && mode != "reflector")) {
        std::cerr << "\nError: Invalid or missing IP, port, or mode\n\n"; print_usage(); return 1;
    }

    if (mode == "controller") {
        while (i < argc) {
            std::string arg = argv[i++];
            if (arg == "-c" && i < argc) packet_count = std::stoi(argv[i++]);
            else if (arg == "-i" && i < argc) interval_ms = std::stoi(argv[i++]);
            else if (arg == "-t" && i < argc) timeout_ms = std::stoi(argv[i++]);
            else { std::cerr << "\nError: Invalid argument " << arg << "\n\n"; print_usage(); return 1; }
        }
    } 
    else {
        while (i < argc) {
            std::string arg = argv[i++];
            if (arg == "-c" || arg == "-d" || arg == "-t") {
                std::cerr << "\nError: -c, -d, -t flags allowed only with controller mode\n\n";
                print_usage();
                return 1;
            } else {
                std::cerr << "\nError: Unexpected argument " << arg << "\n\n";
                print_usage();
                return 1;
            }
        }
    }
    
    std::cout << "\nMode: " << mode << "\nIPv4 Address: " << ip << "\nPort Number: " << port << "\n" << std::endl;
    if (mode == "controller") {
        std::cout << "Packet count: " << packet_count << "\nInterval (ms): " << interval_ms << "\nTimeout (ms): " << timeout_ms << "\n\n";
    }


    if (mode == "controller"){
        TwampLightSender sender(ip, port);
        std::unordered_map<std::string, double> result = sender.run(packet_count, interval_ms, timeout_ms);
    }
    else {
        TwampLightReflector reflector(ip, port);
        reflector.run();
    }

    return 0;
}
