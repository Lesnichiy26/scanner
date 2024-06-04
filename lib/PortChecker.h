#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <sys/epoll.h>

class PortChecker {
public:
    PortChecker(const std::string& ip) : ip_(ip) {}

    ~PortChecker() = default;

    std::vector<int> checkPortRange(int start, int end, int num_threads = 10);

private:
    std::string ip_;

    void checkPortSubset(int start, int end, std::vector<int>& open, std::mutex& mtx);

    bool createSocket(int port, int& sock);

    bool initEpoll(int sock, int& epoll_fd);

    bool isPortOpen(int port);
};