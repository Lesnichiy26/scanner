#include "PortChecker.h"

std::vector<int> PortChecker::checkPortRange(int start, int end, int num_threads) {
    std::vector<std::thread> threads;
    std::vector<std::vector<int>> open_per_thread(num_threads);
    int range_per_thread = (end - start + 1) / num_threads;

    for (int i = 0; i < num_threads; ++i) {
        int range_start = start + i * range_per_thread;
        int range_end;
        if (i == num_threads - 1) {
            range_end = end;
        } else {
            range_end = range_start + range_per_thread - 1;
        }
        threads.emplace_back(&PortChecker::checkPortSubset, this, range_start, range_end, std::ref(open_per_thread[i]));
    }

    for (auto& thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    std::vector<int> open;
    for (const auto& vec : open_per_thread) {
        open.insert(open.end(), vec.begin(), vec.end());
    }

    return open;
}

void PortChecker::checkPortSubset(int start, int end, std::vector<int>& open) {
    std::queue<int> socket_queue;
    std::queue<int> epoll_fd_queue;
    int max_sockets = 10;

    for (int i = 0; i < max_sockets; ++i) {
        int sock, epoll_fd;
        if (createSocket(start, sock) && initEpoll(sock, epoll_fd)) {
            socket_queue.push(sock);
            epoll_fd_queue.push(epoll_fd);
        }
    }

    for (int port = start; port <= end; ++port) {
        if (socket_queue.empty()) {
            std::lock_guard<std::mutex> lock(cerr_mutex_);
            std::cerr << "Not enough sockets created.\n";
            break;
        }

        int sock = socket_queue.front();
        int epoll_fd = epoll_fd_queue.front();
        socket_queue.pop();
        epoll_fd_queue.pop();

        if (createSocket(port, sock) && initEpoll(sock, epoll_fd)) {
            if (isPortOpen(port, sock, epoll_fd)) {
                open.push_back(port);
            }
        }

        socket_queue.push(sock);
        epoll_fd_queue.push(epoll_fd);
    }

    while (!socket_queue.empty()) {
        close(socket_queue.front());
        socket_queue.pop();
    }

    while (!epoll_fd_queue.empty()) {
        close(epoll_fd_queue.front());
        epoll_fd_queue.pop();
    }
}

bool PortChecker::createSocket(int port, int& sock) {
    sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (sock < 0) {
        std::lock_guard<std::mutex> lock(cerr_mutex_);
        std::cerr << "Error : socket creation error: " << strerror(errno) << '\n';
        return false;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip_.c_str(), &server_addr.sin_addr) <= 0) {
        std::lock_guard<std::mutex> lock(cerr_mutex_);
        std::cerr << "Error : invalid address or address not supported: " << strerror(errno) << '\n';
        return false;
    }

    int result = connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if (result < 0 && errno != EINPROGRESS) {
        if (errno != ECONNREFUSED) {
            std::lock_guard<std::mutex> lock(cerr_mutex_);
            std::cerr << "Connection error : " << strerror(errno) << '\n';
        }
        return false;
    }

    return true;
}

bool PortChecker::initEpoll(int sock, int& epoll_fd) {
    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        std::lock_guard<std::mutex> lock(cerr_mutex_);
        std::cerr << "epoll_create1 error : " << strerror(errno) << '\n';
        return false;
    }

    struct epoll_event ev;
    ev.events = EPOLLOUT | EPOLLERR | EPOLLHUP;
    ev.data.fd = sock;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock, &ev) == -1) {
        std::lock_guard<std::mutex> lock(cerr_mutex_);
        std::cerr << "epoll_ctl error : " << strerror(errno) << '\n';
        return false;
    }

    return true;
}

bool PortChecker::isPortOpen(int port, int sock, int epoll_fd) {
    bool portOpen = false;

    struct epoll_event events[1];
    int nfds = epoll_wait(epoll_fd, events, 1, 5000);  // 5-second timeout
    if (nfds == -1) {
        std::lock_guard<std::mutex> lock(cerr_mutex_);
        std::cerr << "epoll_wait error : " << strerror(errno) << '\n';
    } else {
        for (int i = 0; i < nfds; ++i) {
            if (events[i].events & (EPOLLERR | EPOLLHUP)) {
                int error = 0;
                socklen_t len = sizeof(error);
                if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
                    std::lock_guard<std::mutex> lock(cerr_mutex_);
                    std::cerr << "getsockopt error : " << strerror(errno) << '\n';
                } else {
                    if (error != ECONNREFUSED) {
                        std::lock_guard<std::mutex> lock(cerr_mutex_);
                        std::cerr << "Connection error : " << strerror(error) << '\n';
                    }
                }
            } else if (events[i].events & EPOLLOUT) {
                portOpen = true;
            }
        }
    }

    return portOpen;
}
