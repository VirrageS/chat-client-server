#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdbool.h>
#include <signal.h>

#include "err.h"
#include "chat.h"

#define BUFFER_SIZE 10000
#define STDIN 0

int client_socket = -1;

void close_connection()
{
    debug_print("%s\n", "[client] closing connection");
    close(client_socket);
}

int main(int argc, char *argv[])
{
    signal(SIGINT, close_connection);
    signal(SIGKILL, close_connection);

    int err;
    struct addrinfo addr_hints;
    struct addrinfo *addr_result;
    char read_buffer[BUFFER_SIZE];
    char send_buffer[BUFFER_SIZE];
    ssize_t rcv_len;

    if (argc > 3) {
        fatal("Usage: %s host [port]\n", argv[0]);
    }

    // set default port
    if (argc == 2) {
        argv[2] = STR(PORT);
    }

    // 'converting' host/port in string to struct addrinfo
    memset(&addr_hints, 0, sizeof(struct addrinfo));
    addr_hints.ai_family = AF_INET; // IPv4
    addr_hints.ai_socktype = SOCK_STREAM;
    addr_hints.ai_protocol = IPPROTO_TCP;
    err = getaddrinfo(argv[1], argv[2], &addr_hints, &addr_result);
    if (err == EAI_SYSTEM) { // system error
        syserr("getaddrinfo: %s", gai_strerror(err));
    } else if (err != 0) { // other error (host not found, etc.)
        fatal("getaddrinfo: %s", gai_strerror(err));
    }

    // initialize socket according to getaddrinfo results
    client_socket = socket(addr_result->ai_family, addr_result->ai_socktype, addr_result->ai_protocol);
    if (client_socket < 0)
        syserr("socket");

    // connect socket to the server
    err = connect(client_socket, addr_result->ai_addr, addr_result->ai_addrlen);
    if (err < 0)
        syserr("connect() failed");

    freeaddrinfo(addr_result);

    // make standard input nonblocking
    err = fcntl(STDIN, F_SETFL, fcntl(STDIN, F_GETFL, 0) | O_NONBLOCK);
    if (err < 0) {
        syserr("fcntl() failed");
    }

    // make client socket nonblocking
    err = fcntl(client_socket, F_SETFL, fcntl(client_socket, F_GETFL, 0) | O_NONBLOCK);
    if (err < 0) {
        syserr("fcntl() failed");
    }

    while (true) {
        int bytes_received = read(STDIN, &send_buffer, 1000);
        if (bytes_received < 0) {
            if (errno != EWOULDBLOCK) {
                syserr("read() failed");
            }
        }

        if (bytes_received > 0) {
            debug_print("%s\n", "sending message to server");

            bytes_received--; // remove new line
            if (write(client_socket, send_buffer, bytes_received) != bytes_received) {
                syserr("write() partial / failed");
            }
        }

        memset(read_buffer, 0, sizeof(read_buffer));
        rcv_len = read(client_socket, read_buffer, sizeof(read_buffer) - 1);
        if (rcv_len < 0) {
            if (errno != EWOULDBLOCK) {
                syserr("read() failed");
            }
        }

        if (rcv_len == 0) {
            printf("%s\n", "Something wrong happened. Connection has been closed");
            close_connection();
            return 100;
        }

        if (rcv_len > 0) {
            debug_print("read message from server [%s] (%zd bytes)\n", read_buffer, rcv_len);
        }

        // clear buffer
        memset(read_buffer, 0, sizeof(read_buffer));
        memset(send_buffer, 0, sizeof(send_buffer));

        // sleep(1);
    }

    err = close(client_socket);
    if (err < 0) {
        syserr("close() failed");
    }

    return 0;
}
