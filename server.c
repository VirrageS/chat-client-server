#include <stdio.h>
#include <stdlib.h>
#include <string.h> /* memset */
#include <unistd.h> /* close */
#include <stdbool.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>

#include "err.h"
#include "chat.h"

#define MAX_CLIENTS 20
#define BUFFER_SIZE 20000

typedef struct {
    ssize_t in_buffer; // how many bytes is in buffer
    int has_message; // check if message is read
    char buffer[BUFFER_SIZE]; // buffer
} buffer_t;

typedef struct pollfd connection_t;

connection_t connections[MAX_CLIENTS + 1];
nfds_t connections_len = 1, current_conn_len = 0;
int listen_socket = -1;

// buffer_t read_buffer[MAX_CLIENTS];

void close_connections()
{
    debug_print("%s\n", "[server] closing connection");

    for (int k = 1; k < connections_len; ++k) {
        shutdown(connections[k].fd, 2);
    }

    shutdown(listen_socket, 2);
}


void broadcast_message(buffer_t *buf, connection_t conn)
{
    int err;

    for (int k = 0; k < connections_len; ++k) {
        if ((connections[k].fd <= 0) || (conn.fd == connections[k].fd) || (listen_socket == connections[k].fd))
            continue;

        debug_print("broadcasting (from: %d; message: [%s] (bytes %zd); to: %d)\n", conn.fd, buf->buffer, buf->in_buffer, connections[k].fd);
        err = send(connections[k].fd, buf->buffer, buf->in_buffer, 0);
        if (err < 0) {
            syserr("send() failed");
            break;
        }
    }

    // clear buffer
    buf->has_message = false;
    buf->in_buffer = 0;
    memset(buf->buffer, 0, sizeof(buf->buffer));
}

void compress_connections()
{
    for (int i = 0; i < connections_len; i++) {
        if (connections[i].fd == -1) {
            for (int j = i; j < connections_len; j++)
                connections[j].fd = connections[j + 1].fd;

            connections_len--;
        }
    }
}

void set_listening_socket(uint16_t port)
{
    int err = 0;
    struct sockaddr_in server_address;

    // creating IPv4 TCP socket
    listen_socket = socket(PF_INET, SOCK_STREAM, 0);
    if (listen_socket < 0) {
        syserr("socket() failed");
    }

    // set listening socket to be nonblocking
    // socket with incoming connections will inherit nonblocking state
    err = fcntl(listen_socket, F_SETFL, fcntl(listen_socket, F_GETFL, 0) | O_NONBLOCK);
    if (err < 0) {
        syserr("ioctl() failed");
    }

    // binding the listening socket
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons((port > 0 ? port : PORT));
    err = bind(listen_socket, (struct sockaddr *) &server_address, sizeof(server_address));
    if (err < 0) {
        syserr("bind() failed");
    }

    // setting listening and max clients queue
    err = listen(listen_socket, MAX_CLIENTS);
    if (err < 0) {
        syserr("listen() failed");
    }
}

int main(int argc, char *argv[])
{
    signal(SIGINT, close_connections);
    signal(SIGKILL, close_connections);

    // INITIAL VALUES
    int err = 0;
    uint16_t port = 0;

    bool end_server = false; // checks if we should end server
    bool close_connection = false; // checks if we should close connection
    buffer_t read_buffer[MAX_CLIENTS];


    // INITIAL FUNCTIONS
    if (argc > 2) {
        fatal("Usage: %s [port]\n", argv[0]);
    }

    // check if port has been set
    if (argc == 2) {
        long int tmp_port = strtol(argv[1], NULL, 10);

        if (tmp_port > 0L)
            port = (uint16_t)tmp_port;
    }

    // setting up listening socket (socket, ioctl, bind, listen)
    set_listening_socket(port);

    // init listening connection
    memset(connections, 0, sizeof(connections));
    connections[0].fd = listen_socket;
    connections[0].events = POLLIN | POLLHUP;

    // SERVER
    while (!end_server) {
        debug_print("%s\n", "waiting on poll() ...");

        // call poll() and wait 3 minutes for it to complete
        err = poll(connections, connections_len, 3 * 60 * 1000);
        if (err < 0) {
            syserr("poll() failed");
        }

        if (err == 0) {
            debug_print("%s\n", "poll() timed out. exiting...");
            break;
        }

        current_conn_len = connections_len;
        for (int i = 0; i < current_conn_len; i++) {
            // ommit connections which are not active
            if (connections[i].revents == 0)
                continue;

            if (!(connections[i].revents & (POLLIN | POLLHUP))) {
                debug_print("[ERROR] revents = %d\n", connections[i].revents);
                end_server = true;
                break;
            }

            if (connections[i].fd == listen_socket) {
                // listening socket is readable
                debug_print("%s\n", "listening socket is readable");

                int client_socket = -1;
                do {
                    client_socket = accept(listen_socket, NULL, NULL);
                    if (client_socket < 0) {
                        if (errno != EWOULDBLOCK) {
                            syserr("accept() failed");
                        }

                        break;
                    }

                    debug_print("new incoming connection - %d\n", client_socket);
                    connections[connections_len].fd = client_socket;
                    connections[connections_len].events = POLLIN;
                    connections_len++;
                } while (client_socket != -1);
            } else {
                debug_print("descriptor %d is readable\n", connections[i].fd);

                buffer_t *current_buffer = &read_buffer[i];
                current_buffer->in_buffer = 0;
                close_connection = false;
                do {
                    ssize_t bytes_received = recv(connections[i].fd, &(current_buffer->buffer[current_buffer->in_buffer]), sizeof(char) * (BUFFER_SIZE - current_buffer->in_buffer - 1), 0);
                    if (bytes_received < 0) {
                        if (errno != EWOULDBLOCK)
                            syserr("recv() failed");

                        break;
                    }

                    // check if connection has been closed by client
                    if (bytes_received == 0) {
                        debug_print("connection %d closed\n", connections[i].fd);
                        close_connection = true;
                        break;
                    }

                    current_buffer->in_buffer += bytes_received;
                    current_buffer->has_message = true;

                    debug_print("message: [%s] (bytes %zd)\n", current_buffer->buffer, current_buffer->in_buffer);
                } while (true);

                // broadcast message
                if (current_buffer->has_message) {
                    broadcast_message(current_buffer, connections[i]);
                }
            }

            if (close_connection) {
                err = close(connections[i].fd);
                if (err < 0) {
                    syserr("close() failed");
                }

                connections[i].fd = -1;
            }
        }

        // remove connections which are not used anymore
        compress_connections();
    }

    // close all connections which left opened
    close_connections();
    return 0;
}
