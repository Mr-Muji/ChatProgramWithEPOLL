#ifndef SERVER_H
#define SERVER_H

#include <string>
#include <map>
#include <queue>

#define MAX_EVENTS 10

class Server
{
private:
    std::map<int, std::string> nicknames;
    std::map<int, std::queue<std::string>> send_queues;
    void broadcasting(int client_sock, int epoll_fd, const char *msg);

public:
    Server() {};
    ~Server() {};
    void run();
};

#endif // SERVER_H