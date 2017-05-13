#include <errno.h>
#include <fcntl.h>
#include <queue>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "Listener.h"

using std::queue;

#define VOID_SOCKET -1

int set_nonblocking(int sock_fd);

ListenerException::ListenerException(int sock_fd, const std::string& message, bool use_errno)
    : std::runtime_error(message + ((use_errno == true) ? strerror(errno) : ""))
    , socket_fd(sock_fd) {
}

const char* ListenerException::what() const throw() {
    std::ostringstream err_msg;

    err_msg << "on socket " << socket_fd << std::runtime_error::what();

    return err_msg.str().c_str();
}

int ListenerException::get_socket() const {
    return socket_fd;
}

Listener::Listener(int efd, int listener_sock_fd)
    : epoll_fd(efd)
    , socket(listener_sock_fd) {
    epoll_event event;

    set_nonblocking(socket);

    event.data.fd = socket;
    event.events = EPOLLIN | EPOLLET;
    int s = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, socket, &event);
    if (s == -1) {
        throw ListenerException(socket, "in Listener::Listener: ", true);
    }

    printf("Accepted connection on descriptor %d\n", socket);
}

Listener::Listener() {}

Listener::Listener(Listener&& other)
    : epoll_fd(other.epoll_fd) {
    socket = other.socket;
    other.socket = VOID_SOCKET;
}

Listener::~Listener() {
    if (socket != VOID_SOCKET) {
        close(socket);
        printf("connection closed on descriptor %d\n", socket);
    }
}

Listener& Listener::operator=(Listener&& other) {
    if (this != &other) {
        epoll_fd = other.epoll_fd;
        socket = other.socket;
        other.socket = VOID_SOCKET;
    }
    return *this;
}

void Listener::put(const char* buf, size_t len) {
    // if the socket was moved from this object
    if (socket == VOID_SOCKET) {
        throw ListenerException(socket, "in Listener::put(): this instance no longer contains");
    }

    // create and initialize write task
    WriteTask t;
    t.buf = new char[len];
    t.size = len;
    t.written = 0;
    strncpy(t.buf, buf, len);

    // push the output string to the waiting queue
    output.push(t);

    // tell epoll to listen for 'socket is writeable' events
    epoll_event event;
    event.data.fd = socket;
    event.events = EPOLLIN | EPOLLOUT;

    int s = epoll_ctl(epoll_fd, EPOLL_CTL_MOD, socket, &event);
    if (s == -1) {
        throw ListenerException(socket, "epoll_ctl in Listener::put()", true);
    }
}

void Listener::flush() {
    // if the socket was moved from this object
    if (socket == VOID_SOCKET) {
        throw ListenerException(socket, "in Listener::put(): the socket was moved");
    }

    WriteTask& t = output.front();
    size_t remain = t.size - t.written;

    int n_written = write(socket, t.buf + t.written, remain);
    if (n_written == -1) {
        throw ListenerException(socket, "write in Listener::flush()", true);
    }
    t.written += (size_t)n_written;

    // check: if this write task is finished or not?
    if (t.written == t.size) { // finished
        output.pop();
        delete[] t.buf;
    }

    // if all messages were sent, disable EPOLLOUT events to save CPU
    if (output.empty()) {
        epoll_event event;
        event.data.fd = socket;
        event.events = EPOLLIN;

        int s = epoll_ctl(epoll_fd, EPOLL_CTL_MOD, socket, &event);
        if (s == -1) {
            throw ListenerException(socket, "epoll_ctl in Listener::flush()", true);
        }
    }
}