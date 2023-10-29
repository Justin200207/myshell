#include <signal.h>

typedef struct bgnode {
    pid_t id;
    char name[64];
    struct bgnode *next;
    int done;
} bgnode;