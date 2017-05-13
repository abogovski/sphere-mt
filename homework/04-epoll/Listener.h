#ifndef CLIENT_H_INCLUDED
#define CLIENT_H_INCLUDED

#include <exception>
#include <queue>
#include <sstream>
#include <stddef.h>
#include <stdexcept>
#include <string>
#include <sys/epoll.h>

class ListenerException : public std::runtime_error {
public:
    ListenerException(int sock_fd, const std::string& message, bool use_errno = false);

    virtual const char* what() const throw();

    int get_socket() const;

private:
    int socket_fd;
};

class Listener {
public:
    Listener(int efd, int listener_sock_fd);

    Listener();
    Listener(const Listener&) = delete;
    Listener(Listener&&);
    ~Listener();

    Listener& operator=(const Listener&) = delete;
    Listener& operator=(Listener&&);

    void put(const char* buf, size_t len);

    void flush();

private:
    struct WriteTask {
        char* buf;
        size_t size;
        size_t written;
    };

    int epoll_fd;
    int socket;
    std::queue<WriteTask> output;
};

#endif // LISTENER_H_INCLUDED