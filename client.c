#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "err.h"

#define PORT "20160"
#define BUFFER_SIZE 10000

int main(int argc, char *argv[])
{
    int client_socket, err;
    struct addrinfo addr_hints;
    struct addrinfo *addr_result;
    char buffer[BUFFER_SIZE];
    ssize_t len, rcv_len;

    if (argc > 3) {
        fatal("Usage: %s host [port]\n", argv[0]);
    }

    // set default port
    if (argc == 2) {
        argv[2] = PORT;
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


    while (read(0, &buffer, 1000) > 0) {
        // first read "number" and convert to some sort of int
        // then read bytes which make
        len = strnlen(buffer, BUFFER_SIZE);
        if (len == BUFFER_SIZE) {
            continue;
        }

        printf("writing to socket: %s\n", buffer);
        if (write(client_socket, buffer, len) != len) {
            syserr("partial / failed write");
        }

        memset(buffer, 0, sizeof(buffer));
        rcv_len = read(client_socket, buffer, sizeof(buffer) - 1);
        if (rcv_len < 0) {
            syserr("read() failed");
        }

        printf("read from socket: %zd bytes: %s\n", rcv_len, buffer);
    }

    err = close(client_socket);
    if (err < 0) {
        syserr("close() failed");
    }

    return 0;
}
