// This solution is based on the following code:
// https://banu.com/blog/2/how-to-use-epoll-a-complete-example-in-c/

// Client application is not provided (however, it can be easily implemented);
// please, use Telnet instead.
//
// Example:
// server:   ./chat-server 12345
// client:   telnet localhost 12345

// EPOLLIN  - new data in socket to be read
// EPOLLOUT - socket is ready to continue data receiving
// EPOLLERR - error in socket (file descriptor)
// EPOLLHUP - socket was closed

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <map>
#include <vector>

#include "Listener.h"

#define MAXEVENTS 64
#define MAX_MSG_CHUNK 512

int set_nonblocking(int sock_fd);
int create_and_bind_socket(char* port_str);

// creates socket on port specified in port_str
// returns: socket descriptor or -1 if an error occured
int create_and_bind_socket(char* port_str) {
    int sock_fd;
    struct sockaddr_in sock_addr;
    uint32_t port;

    port = strtol(port_str, nullptr, 10);

    bzero(&sock_addr, sizeof(sock_addr));
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_port = htons(port);
    sock_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    sock_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    int s = bind(sock_fd, (struct sockaddr*)&sock_addr, sizeof(sock_addr));
    if (s == -1) {
        perror("bind");
        return -1;
    }

    s = set_nonblocking(sock_fd);
    if (s == -1) {
        perror("set_nonblocking");
        return -1;
    }

    return sock_fd;
}

int set_nonblocking(int sock_fd) {
    int flags = fcntl(sock_fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl");
        return -1;
    }

    flags |= O_NONBLOCK;

    int s = fcntl(sock_fd, F_SETFL, flags);
    if (s == -1) {
        perror("fcntl");
        return -1;
    }
    return 0;
}

bool sigint = false;

void sigint_handler(int signo) {
    if (signo == SIGINT) {
        sigint = true;
    }
}

int main(int argc, char** argv) {
    signal(SIGINT, sigint_handler);

    int master_socket;
    std::map<int, Listener> listeners; // map: socket_fd -> Listener object

    if (argc != 2) {
        fprintf(stderr, "Usage: %s [port]\n", argv[0]);
        return 1;
    }

    master_socket = create_and_bind_socket(argv[1]);
    if (master_socket == -1) {
        return 1;
    }

    // listen port
    int s = listen(master_socket, SOMAXCONN);
    if (s == -1) {
        perror("listen");
        return 1;
    }

    // data for epoll
    int efd = epoll_create1(0);
    struct epoll_event event;
    struct epoll_event* events;

    event.data.fd = master_socket;
    event.events = EPOLLIN | EPOLLET;
    s = epoll_ctl(efd, EPOLL_CTL_ADD, master_socket, &event);
    if (s == -1) {
        perror("epoll_ctl");
        return 1;
    }
    events = new epoll_event[MAXEVENTS];
    printf("Epoll started successfully\n");

    while (!sigint) {
        int n = epoll_wait(efd, events, MAXEVENTS, -1);
        for (int i = 0; i < n; i++) {
            try {
                int fd = events[i].data.fd;
                // if (error or closed socket)
                if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) || (!(events[i].events & (EPOLLIN | EPOLLOUT)))) {
                    printf("epoll error at descriptor %d\n", fd);
                    listeners.erase(fd);
                    continue;
                }

                // send next data chunk to fd
                if ((events[i].events & EPOLLOUT) && (fd != master_socket)) {
                    listeners[fd].flush(); // can throw an exception
                    continue;
                }

                // new connection
                if (fd == master_socket) {
                    while (true) {
                        struct sockaddr in_addr;
                        socklen_t in_len = sizeof(in_addr);
                        char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
                        int infd = accept(master_socket, &in_addr, &in_len);

                        if (infd == -1) {
                            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                                perror("accept\n"); // unexpected failure reason
                            }
                            break;
                        }

                        listeners[infd] = Listener(efd, infd);
                        const char* msg = "Welcome to Epoll-powered chat!\n";
                        listeners[infd].put(msg, strlen(msg));
                    }
                    continue;
                }
                // new message was received
                {
                    int done = 0;
                    // read input message chunk-by-chunk
                    while (true) {
                        ssize_t count;
                        char buf[MAX_MSG_CHUNK];
                        count = read(fd, buf, sizeof(buf));
                        if (count == -1) {
                            if (errno != EAGAIN) {
                                perror("read");
                                done = 1;
                            }
                            break;
                        }
                        char* eot = (char*)memchr(buf, 4, count);
                        // EOF or EOT; connection closed
                        if (count == 0 || eot == buf) {
                            done = 1;
                            if (eot) {
                                printf("Connection on %d descriptor received EOT\n", fd);
                                sprintf(buf, "Ctrl+D Received. You've left this chat.\n", fd);
                                listeners[fd].put(buf, strlen(buf));
                                listeners[fd].flush();
                            }
                            break;
                        }

                        for (std::pair<const int, Listener>& l : listeners) {
                            l.second.put(buf, count);
                        }
                    }
                    if (done) {
                        listeners.erase(fd);
                    }
                }
            } catch (ListenerException& ex) {
                printf("Error: %s\n", ex.what());
                close(ex.get_socket());
            }
        }
    }

    puts("Shutting down server");
    delete[] events;
    close(master_socket);
}