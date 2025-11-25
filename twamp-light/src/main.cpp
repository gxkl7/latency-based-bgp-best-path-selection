#include "twamp_light.hpp"
using namespace std;

//flags to manage the receiver and sender threads
atomic<bool> running {true};
atomic<bool> peers_updated {false};

//signal handler
void signal_handler(int signal) {
    if (signal == SIGTERM || signal == SIGINT) {
        cout << "\nReceived shutdown signal" << endl;
        running = false;
    }
    else {
        std::cout << "\nReceived signal: " << signal << std::endl;
        running = false;
    }

}

//probe parameter configs
struct probe_config_struct {
    int packet_count = 3;
    int interval_ms = 10;
    int timeout_ms = 100;
    int probe_cycle_sec = 60;
    int port = 862;
};

//map and struct to store peer IPs and their latency data
struct latency_data{
    uint64_t latency {0};
    bool spike {false};
};
unordered_map<string,latency_data> latency_db;

//protect latency_db
mutex latency_db_mutex; 
//condition variable to track when peers are added to an empty vector
condition_variable latency_db_cv;

//add peers to the shared vector
void add_peer(const string &peer_ip_add) {
    unique_lock<mutex> lock(latency_db_mutex);
    latency_data data;
    latency_db[peer_ip_add] = data;
    peers_updated = true;
    latency_db_cv.notify_one();
}

//delete peers from the shared vector
void del_peer(const string &peer_ip_del) {
    unique_lock<mutex> lock(latency_db_mutex);
    latency_db.erase(peer_ip_del);
    peers_updated = true;
    latency_db_cv.notify_one();
}

//Reflector function
void reflector_main(const int &port){
    //initialziing the reflector to start responding to the peer on port 862
    TwampLightReflector reflector("0.0.0.0", port);
    reflector.run();
    cout << "Reflector thread exiting" << endl;
}

//Sender function
void sender_main(const probe_config_struct &probe_config){ 
    // Local Cache 
    unordered_map<string,latency_data> local_latency_db;
    while (running){
        {
            unique_lock<mutex> lock(latency_db_mutex);
            // wait while empty and running
            while (latency_db.size() == 0 && running){
                cout << "No peers to probe, waiting..." << endl;
                latency_db_cv.wait_for(lock, chrono::seconds(1), 
                    [&]{ return latency_db.size() > 0 || !running;});
            }
            if (!running) break;
            if (latency_db.size() > 0) cout << "Peers available, starting probes..." << endl;
        }
        if (peers_updated){
                peers_updated = false;
                local_latency_db = latency_db;
                cout << "Updated the peer table" << endl;
        }
        if (local_latency_db.size() > 0){
            if (!running) break;
            //run the probes and write it
            for (const auto &items: local_latency_db)
            {
                //TODO
            }
            this_thread::sleep_for(chrono::seconds(probe_config.probe_cycle_sec));
        }
    }
    cout << "Sender thread exiting" << endl;
}

int main(int argc, char* argv[]) {
    
    //registering signal handler
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);

    //probe parameter configs
    probe_config_struct probe_config;
    
    //parsing the cli input
    if (argc > 1) {
        int i = 1;
        while (i < argc) {
                std::string arg = argv[i++];
                if (arg == "-c" && i < argc) probe_config.packet_count = std::stoi(argv[i++]);
                else if (arg == "-i" && i < argc) probe_config.interval_ms = std::stoi(argv[i++]);
                else if (arg == "-t" && i < argc) probe_config.timeout_ms = std::stoi(argv[i++]);
                else if (arg == "-p" && i < argc) probe_config.port = std::stoi(argv[i++]);
                else if (arg == "-f" && i < argc) probe_config.probe_cycle_sec = std::stoi(argv[i++]);
        }
    }

    cout << "Starting the TWAMP-Light Agent..." << endl;
    cout<<"starting the reflector thread" <<endl;
    // To start the reflector in a separate thread
    thread reflector_thread(reflector_main, probe_config.port);
    reflector_thread.detach();
    cout<<"starting the sended thread" <<endl;
    // To start the controller in a separate thread
    thread sender_thread(sender_main, probe_config);
    sender_thread.detach();

    //holding the main thread
    while (running) {
        this_thread::sleep_for(chrono::seconds(1));
    }
    cout << "Shutting down..." << endl;
    // Wake up sender if waiting
    latency_db_cv.notify_all();
    this_thread::sleep_for(chrono::seconds(5));

    return 0;
}
