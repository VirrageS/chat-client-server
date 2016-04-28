#ifndef __CHAT_H__
#define __CHAT_H__

#include <stdbool.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdint.h>

#define debug_print(fmt, ...) \
        do { if (DEBUG) fprintf(stderr, "%s:%d:%s(): " fmt, __FILE__, \
                                __LINE__, __func__, __VA_ARGS__); } while (0)

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#define DEBUG 0

#define PORT 20160
#define MAX_MESSAGE_SIZE 1000
#define BUFFER_SIZE 2000

typedef struct {
    uint16_t msg_length;
    ssize_t in_buffer; // how many bytes is in buffer
    int has_message; // check if message is ready
    char buffer[BUFFER_SIZE]; // buffer
} buffer_t;

void update_buffer_info(buffer_t *buf);
void clean_buffer(buffer_t *buf, bool force);

bool read_from_socket(int fd, buffer_t *buf);
bool write_to_socket(int fd, buffer_t *buf);
void try_sending_message(int fd, buffer_t *buf);

#endif
