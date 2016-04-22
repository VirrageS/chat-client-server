#include "chat.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "err.h"

void update_buffer_info(buffer_t *buf)
{
    if (buf->in_buffer < 2)
        return;

    if (buf->msg_length > 0) {
        if (buf->in_buffer >= buf->msg_length)
            buf->has_message = true;

        return;
    }

    debug_print("msg_length: %u; in_buffer: %zd; msg: [%s]\n", buf->msg_length, buf->in_buffer, buf->buffer);

    buf->msg_length = ntohs(buf->buffer[0] | (buf->buffer[1] << 8));

    memmove(&buf->buffer[0], &buf->buffer[2], sizeof(char) * (buf->in_buffer - 2));
    buf->in_buffer -= 2;

    debug_print("msg_length: %u; in_buffer: %zd; msg: [%s]\n", buf->msg_length, buf->in_buffer, buf->buffer);

    if (buf->in_buffer >= buf->msg_length)
        buf->has_message = true;
}

void clean_buffer(buffer_t *buf, bool force)
{
    if (!force) {
        memmove(&buf->buffer[0], &buf->buffer[buf->msg_length], sizeof(char) * (buf->in_buffer - buf->msg_length));
        buf->in_buffer -= buf->msg_length;
        buf->msg_length = 0;
        buf->has_message = false;
    } else {
        buf->msg_length = 0;
        buf->in_buffer = 0;
        buf->has_message = false;
        memset(buf->buffer, 0, sizeof(buf->buffer));
    }

    debug_print("[clean_buffer] in_buffer: %zd; msg_length: %u;\n", buf->in_buffer, buf->msg_length);
}

bool read_from_socket(int fd, buffer_t *buf)
{
    bool close_connection = false;

    do {
        ssize_t bytes_received = recv(fd, &buf->buffer[buf->in_buffer], sizeof(char) * (BUFFER_SIZE - buf->in_buffer), 0);
        if (bytes_received < 0) {
            if (errno != EWOULDBLOCK)
                syserr("recv() failed");

            break;
        } else if (bytes_received == 0) {
            // check if connection has been closed by client
            debug_print("connection %d closed\n", fd);
            close_connection = true;
            break;
        }

        buf->in_buffer += bytes_received;
        update_buffer_info(buf);

        // check if message has exceeded 1000 bytes
        if (buf->msg_length > MAX_MESSAGE_SIZE) {
            debug_print("%s\n", "message has exceeded allowed length");
            close_connection = true;
            break;
        }


        try_sending_message(fd, buf);
        // debug_print("message: [%s] (bytes %zd)\n", buf->buffer, buf->in_buffer);
    } while (true);

    return close_connection;
}