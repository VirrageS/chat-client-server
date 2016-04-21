#define debug_print(fmt, ...) \
        do { if (DEBUG) fprintf(stderr, "%s:%d:%s(): " fmt, __FILE__, \
                                __LINE__, __func__, __VA_ARGS__); } while (0)

#define PORT 20160
#define DEBUG 1
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
