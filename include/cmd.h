#ifndef CMD_H
#define CMD_H
#include <stddef.h>
#include "parser.h"
#define INVALID_FD (-1)
#define INVALID_PROC (-1)

typedef enum {
    ERR_PROC_OK,
    ERR_PROC_EXIT,
    ERR_PROC_WAIT_FAIL,
    ERR_PROC_INVALID,
    ERR_PROC_TERMINATED,
} ErrProc;

typedef struct {
    int *items;
    size_t count;
    size_t capacity;
} Procs;

ErrProc execute_commands(Cmd *cmd);
int cmd_get_shell_state();
Procs cmd_get_procs();
void free_procs();
void free_jobs();

#endif
