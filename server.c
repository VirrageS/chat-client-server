#include <stdio.h>
#include <stdlib.h>
#include <string.h> /* memset */
#include <unistd.h> /* close */
#include <stdbool.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>

#include "err.h"

#define PORT 20160
#define MAX_CLIENTS 20
#define BUFFER_SIZE 20000

int main (int argc, char *argv[])
{
    // INITIAL VALUES
    int on = 1; // WTF?!
    int client_socket = -1;

    int listen_socket = -1, err = 0, len = 0;
    uint16_t port = 0;

    struct sockaddr_in server_address;

    struct pollfd connections[MAX_CLIENTS + 10];
    nfds_t connections_len = 1, current_conn_len = 0;

    bool end_server = false; // checks if we should end server
    bool close_connection = false; // checks if we should close connection

    char buffer[BUFFER_SIZE];


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

    // creating IPv4 TCP socket
    listen_socket = socket(PF_INET, SOCK_STREAM, 0);
    if (listen_socket < 0) {
        syserr("socket() failed");
    }

    // set listening socket to be nonblocking
    // socket with incoming connections will inherit nonblocking state
    err = ioctl(listen_socket, FIONBIO, (char *)&on);
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


    //
    memset(connections, 0, sizeof(connections));
    connections[0].fd = listen_socket;
    connections[0].events = POLLIN;


    // SERVER
    while (!end_server) {
        printf("waiting on poll() ...\n");

        // call poll() and wait 3 minutes for it to complete
        err = poll(connections, connections_len, 3 * 60 * 1000);
        if (err < 0) {
            syserr("poll() failed");
        }

        if (err == 0) {
            printf("poll() timed out. exiting...");
            break;
        }

        current_conn_len = connections_len;
        for (int i = 0; i < current_conn_len; i++) {
            // ommit connections which are not active
            if (connections[i].revents == 0)
                continue;

            if (connections[i].revents != POLLIN) {
                printf("[ERROR] revents = %d\n", connections[i].revents);
                end_server = true;
                break;
            }

            if (connections[i].fd == listen_socket) {
                // listening socket is readable
                printf("listening socket is readable\n");

                do {
                    client_socket = accept(listen_socket, NULL, NULL);
                    if (client_socket < 0) {
                        if (errno != EWOULDBLOCK) {
                            syserr("accept() failed");
                        }

                        break;
                    }

                    printf("new incoming connection - %d", client_socket);
                    connections[connections_len].fd = client_socket;
                    connections[connections_len].events = POLLIN;
                    connections_len++;
                } while (client_socket != -1);
            } else {
                printf("descriptor %d is readable\n", connections[i].fd);

                close_connection = false;
                do {
                    // TODO: change `err` to `bytes_received` or something else
                    err = recv(connections[i].fd, buffer, sizeof(buffer), 0);
                    if (err < 0) {
                        if (errno != EWOULDBLOCK) {
                            syserr("recv() failed");
                        }

                        break;
                    }

                    // check if connection has been closed by client
                    if (err == 0) {
                        printf("connection closed\n");
                        close_connection = true;
                        break;
                    }

                    len = err;
                    printf("%d bytes received\n", len);

                    // echo message back
                    err = send(connections[i].fd, buffer, len, 0);
                    if (err < 0) {
                        syserr("send() failed");
                        close_connection = true;
                        break;
                    }
                } while (true);
            }

            if (close_connection) {
                err = close(connections[i].fd);
                if (err < 0) {
                    syserr("close() failed");
                }

                connections[i].fd = -1;
            }
        }

        for (int i = 0; i < connections_len; i++) {
            if (connections[i].fd == -1) {
                for (int j = i; j < connections_len; j++)
                    connections[j].fd = connections[j + 1].fd;

                connections_len--;
            }
        }
    }

    // close all connections which left opened
    for (int i = 0; i < connections_len; i++) {
        if (connections[i].fd >= 0) {
            err = close(connections[i].fd);
            if (err) {
                syserr("close() failed");
            }
        }
    }

    return 0;
}
