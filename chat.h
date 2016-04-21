#define debug_print(fmt, ...) \
        do { if (DEBUG) fprintf(stderr, "%s:%d:%s(): " fmt, __FILE__, \
                                __LINE__, __func__, __VA_ARGS__); } while (0)

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#define DEBUG 1

#define PORT 20160
#define MAX_MESSAGE_SIZE 1000
// #define MESSAGE_SIZE 2000

// typedef struct {
//     unsigned short len;
//     char message[BUFFER_SIZE];
// } message_t;
