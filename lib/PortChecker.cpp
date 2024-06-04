#include "PortChecker.h"


std::vector<int> PortChecker::checkPortRange(int start, int end, int num_threads) {
    std::vector<std::thread> threads;
    std::vector<int> open;
    std::mutex mtx;
    int range_per_thread = (end - start + 1) / num_threads;
    for (int i = 0; i < num_threads; ++i) {
        int range_start = start + i * range_per_thread;
        int range_end;
        if (i == num_threads - 1) {
            range_end = end;
        } else {
            range_end = range_start + range_per_thread - 1;
        }
        std::thread th(&PortChecker::checkPortSubset, this, range_start, range_end, std::ref(open), std::ref(mtx));
        threads.emplace_back(std::move(th));
    }

    for (auto& thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    return open;
}


void PortChecker::checkPortSubset(int start, int end, std::vector<int>& open, std::mutex& mtx) {
    for (int port = start; port <= end; ++port) {
        if (isPortOpen(port)) {
            std::lock_guard<std::mutex> lock(mtx);
            open.push_back(port);
        }
    }
}

bool PortChecker::createSocket(int port, int& sock) {
    sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (sock < 0) {
        std::cerr << "Error : socket creation error: " << strerror(errno) << '\n';
        return false;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip_.c_str(), &server_addr.sin_addr) <= 0) {
        std::cerr << "Error : invalid address or address not supported: " << strerror(errno) << '\n';
        return false;
    }

    int result = connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if (result < 0 && errno != EINPROGRESS) {
        if (errno != ECONNREFUSED) {
            std::cerr << "Connection error : " << strerror(errno) << '\n';
        }
        return false;
    }

    return true;
}

bool PortChecker::initEpoll(int sock, int& epoll_fd) {
    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        std::cerr << "epoll_create1 error : " << strerror(errno) << '\n';
        return false;
    }

    struct epoll_event ev;
    ev.events = EPOLLOUT | EPOLLERR;
    ev.data.fd = sock;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock, &ev) == -1) {
        std::cerr << "epoll_ctl error : " << strerror(errno) << '\n';
        return false;
    }

    return true;
}

bool PortChecker::isPortOpen(int port)  {
    int sock = -1;
    int epoll_fd = -1;
    bool portOpen = false;

    if (createSocket(port, sock) && initEpoll(sock, epoll_fd)) {
        struct epoll_event events[1];
        int nfds = epoll_wait(epoll_fd, events, 1, 5000);  // 5-second timeout
        if (nfds == -1) {
            std::cerr << "epoll_wait error : " << strerror(errno) << '\n';
        } else {
            for (int i = 0; i < nfds; ++i) {
                if (events[i].events & EPOLLOUT) {
                    portOpen = true;
                } else if (events[i].events & EPOLLERR) {
                    int error = 0;
                    socklen_t len = sizeof(error);
                    if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
                        std::cerr << "getsockopt error : " << strerror(errno) << '\n';
                    } else {
                        if (error != ECONNREFUSED) {
                            std::cerr << "Connection error : " << strerror(error) << '\n';
                        }
                    }
                }
            }
        }
    }

    if (sock != -1) {
        close(sock);
    }
    if (epoll_fd != -1) {
        close(epoll_fd);
    }

    return portOpen;
}