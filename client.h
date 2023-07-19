#ifndef CLIENT_H
#define CLIENT_H

#include <string>

class Client {
private:
    std::string server_ip;
    int server_port;
public:
    Client(std::string ip, int port) : server_ip(ip), server_port(port) {};
    ~Client() {};
    int connect_to_server();
    //void receive_data(int sockfd);
};

#endif // CLIENT_H
