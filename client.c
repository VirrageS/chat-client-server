#include <sys/types.h>
#include <sys/socket.h>
#include <sys/poll.h>
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

typedef struct pollfd connection_t;

int client_socket = -1;
connection_t descriptors[2];
nfds_t descriptors_len = 2;
buffer_t read_buffer, send_buffer;


/**
    Closes client connection to server.
    **/
void close_connections()
{
    debug_print("%s\n", "[client] closing connection");
    shutdown(client_socket, 2);
}


/**
    Writes all messages recieved from server to stdout.

    @param fd File descriptor from which message has arrived.
    @param buf Buffer which contains all messages.
    **/
void try_sending_message(int fd, buffer_t *buf)
{
    debug_print("%s\n", "client write message to output");

    while (buf->has_message) {
        buffer_t tmp_buf;
        memcpy(&tmp_buf, buf, sizeof(*buf));
        tmp_buf.buffer[tmp_buf.msg_length] = '\0';

        fprintf(stdout, "%s\n", tmp_buf.buffer);
        fflush(stdout);

        // remove only message
        clean_buffer(buf, false);

        // refresh info
        update_buffer_info(buf);
    }
}

bool read_from_input()
{
    bool end_client = false;

    ssize_t bytes_received = read(STDIN_FILENO, &send_buffer.buffer, BUFFER_SIZE);
    if (bytes_received < 0) {
        if (errno != EWOULDBLOCK) {
            syserr("read() failed");
        }
    } else if (bytes_received == 0) {
        // we will no longer read anything from read socket
        end_client = true;
    } else {
        bytes_received--; // remove new line char

        if ((bytes_received > MAX_MESSAGE_SIZE) || (bytes_received == 0)) {
            debug_print("%s\n", "message too long... not sending to server");
            return end_client;
        }

        // debug_print("sending [%s] message to server", send_buffer.buffer);

        send_buffer.in_buffer = bytes_received;
        send_buffer.msg_length = bytes_received;
        bool close_connection = write_to_socket(client_socket, &send_buffer);
        if (close_connection) {
            end_client = true;
        }
    }

    return end_client;
}

void set_client_socket(char *host, char *port)
{
    int err = 0;
    struct addrinfo addr_hints;
    struct addrinfo *addr_result;

    // 'converting' host/port in string to struct addrinfo
    memset(&addr_hints, 0, sizeof(struct addrinfo));
    addr_hints.ai_family = AF_INET; // IPv4
    addr_hints.ai_socktype = SOCK_STREAM;
    addr_hints.ai_protocol = IPPROTO_TCP;
    err = getaddrinfo(host, port, &addr_hints, &addr_result);
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

    // make client socket nonblocking
    err = fcntl(client_socket, F_SETFL, fcntl(client_socket, F_GETFL, 0) | O_NONBLOCK);
    if (err < 0) {
        syserr("fcntl() failed");
    }
}

int main(int argc, char *argv[])
{
    signal(SIGINT, close_connections);
    signal(SIGKILL, close_connections);

    // INITAL VALUES
    int err;
    bool end_client = false;

    // INITIAL FUNCTIONS
    if (argc > 3) {
        fatal("Usage: %s host [port]\n", argv[0]);
    }

    // set default port
    if (argc == 2) {
        argv[2] = STR(PORT);
    }

    // setting up client socket
    set_client_socket(argv[1], argv[2]);

    // make standard input nonblocking
    err = fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL, 0) | O_NONBLOCK);
    if (err < 0) {
        syserr("fcntl() failed");
    }

    // init descriptors for poll
    memset(descriptors, 0, sizeof(descriptors));
    descriptors[0].fd = client_socket;
    descriptors[0].events = POLLIN | POLLHUP;

    descriptors[1].fd = STDIN_FILENO;
    descriptors[1].events = POLLIN | POLLHUP;

    // clear buffers
    clean_buffer(&read_buffer, true);
    clean_buffer(&send_buffer, true);

    // CLIENT
    while (!end_client) {
        // call poll() and wait 3 minutes for it to complete
        err = poll(descriptors, descriptors_len, 3 * 60 * 1000);
        if (err < 0) {
            syserr("poll() failed");
        }

        if (err == 0) {
            debug_print("%s\n", "poll() timed out. exiting...");
            break;
        }

        for (int i = 0; i < descriptors_len; ++i) {
            if (descriptors[i].revents == 0)
                continue;

            if (!(descriptors[i].revents & (POLLIN | POLLHUP))) {
                debug_print("[ERROR] revents = %d\n", descriptors[i].revents);
                end_client = true;
                break;
            }

            if (descriptors[i].fd == STDIN_FILENO) {
                end_client = read_from_input();
            } else if (descriptors[i].fd == client_socket) {
                bool close_connection = read_from_socket(client_socket, &read_buffer);

                if (close_connection)
                    return 100;
            } else {
                debug_print("%s\n", "default descriptor?!");
            }
        }
    }

    close_connections();
    return 0;
}
