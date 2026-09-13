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

/*void task_sighandler(void *arg) {
    int *sfd = (int*)arg;
    uint64_t offset = 0;
    while (runtime_check()) {
        struct signalfd_siginfo fdsi;
        ssize_t bytes_read = async_read_file(*sfd, &fdsi, sizeof(struct signalfd_siginfo), offset);
        if (bytes_read == 0) {
            LOG_BUG("0 bytes read from signalfd. Either library or system messed up, \
            or most likely the program in a busy-wait loop, rather than sleeping on read signals");
            continue;
        }
        else if (bytes_read < 0) {
            LOG_ERROR("Broken read performed from signalfd, if error produced by system/library, it's because: %s", strerror(errno));
            break;
        }
        else if (bytes_read != sizeof(struct signalfd_siginfo)) {
            LOG_BUG("Either library or system failed to read, either assumption, \
            that read from signalfd must be sizeof(struct signalfd_siginfo), was violated");
            break;
        }
        
        offset += bytes_read;
    }
}*/

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
//    int sfd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);

    enable_raw_mode();
    TrieRoot *trie = trie_create();
    append_system_commands(trie);
    runtime_init();
    task_init(task_shell, trie);
//    task_init(task_sighandler, &sfd);

    runtime_run();

    free_procs();
    free_jobs();
    trie_free(trie);
//    close(sfd);
    disable_raw_mode();
    return 0;
}
