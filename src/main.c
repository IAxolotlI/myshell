#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <liburing.h>
#include <sys/signalfd.h>
#include <signal.h>
#include "parser.h"
#include "logger.h"
#include "cmd.h"
#include "trie.h"
#include "term.h"
#include "task.h"

#define DELIM_USEFUL  "<|>=" // DON'T PUT QUOTES ANYWHERE
#define DELIM_USELESS " \t"

void task_shell(void *arg) {
    TrieRoot *trie = (TrieRoot*)arg;
    while (runtime_check()) {
        char *buffer = (char*)calloc(512, sizeof(char));
        int stat = read_line_interactive(trie, buffer, 512);
        if (stat == 1) {
            free(buffer);
            runtime_stop();
            break;
        }
        
        if (strlen(buffer) > 0) {
            Cmd cmd = {0};
            ErrParse stat_parse = process_string(&cmd, buffer, DELIM_USELESS, DELIM_USEFUL);
            trie_append(trie, buffer);
            if (stat_parse == ERR_PARSE_OK) {
                disable_raw_mode();
                execute_commands(&cmd);                
                enable_raw_mode();
            }
            free_tokens(&cmd);
            free_cmd(&cmd);
        }
        free(buffer);
        buffer = NULL;
    }
}

int main() {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGQUIT);
    sigaddset(&mask, SIGTTOU);
    sigaddset(&mask, SIGTTIN);
    sigaddset(&mask, SIGTSTP);
    sigprocmask(SIG_BLOCK, &mask, NULL);

    enable_raw_mode();
    TrieRoot *trie = trie_create();
    append_system_commands(trie);
    runtime_init();
    task_init(task_shell, trie);

    runtime_run();

    free_procs();
    free_jobs();
    trie_free(trie);
//    close(sfd);
    disable_raw_mode();
    return 0;
}
