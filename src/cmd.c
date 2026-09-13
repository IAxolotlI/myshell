#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include "parser.h"
#include "logger.h"
#include "task.h"
#include "cmd.h"
#define STB_DA_IMPLEMENTATION
#include "macro.h"

#define return_defer(value) do { result = (value); goto defer; } while(0)

static Procs procs = {0}; // running pids array
void free_procs() {
    free(procs.items);
}
Procs cmd_get_procs() {
    return procs;
}

static Procs jobs = {0};  // suspended pids array
void free_jobs() {
    free(jobs.items);
}

static int shell_state = 0;
int cmd_get_shell_state() {
    return shell_state;
}

int cmd_start_proc(Cmd cmd, int fdin, int fdout, int fderr) {
    pid_t cpid = fork();
    if (cpid < 0) {
        LOG_ERROR("Couldn't fork child process: %s", strerror(errno));
        return INVALID_PROC;
    }
    if (cpid == 0) {
        if (fdin > 2) {
            if (dup2(fdin, STDIN_FILENO) < 0) {
                LOG_ERROR("Couldn't setup stdin for child process: %s", strerror(errno));
                exit(1);
            }
            close(fdin);
        }

        if (fdout > 2) {
            if (dup2(fdout, STDOUT_FILENO) < 0) {
                LOG_ERROR("Could not setup stdout for child process: %s", strerror(errno));
                exit(1);
            }
            close(fdout);
        }
        if (fderr > 2) {
            if (dup2(fderr, STDERR_FILENO) < 0) {
                LOG_ERROR("Could not setup stderr for child process: %s", strerror(errno));
                exit(1);
            }
            close(fderr);
        }
        sigset_t mask;
        sigemptyset(&mask);
        sigprocmask(SIG_SETMASK, &mask, NULL);
        da_append(&cmd, (char*)NULL);
        if (execvp(cmd.items[0], (char * const*) cmd.items) < 0) {
            if (errno == ENOENT) {
                LOG_INFO("Command not found: %s", cmd.items[0]);
                exit(0);
            }
            else {
                LOG_ERROR("Could not exec child process for %s: %s", cmd.items[0], strerror(errno));
                exit(1);
            }
        }
        assert("Child reached place after exec, something went horribly wrong");
    }
    return cpid;
}

typedef struct {
    int read;
    int write;
} Pipe;

bool pipe_create(Pipe *pip) {
    int pipefd[2];
    if (pipe(pipefd) < 0) {
        LOG_ERROR("Could not create pipe: %s", strerror(errno));
        return false;
    }
    pip->read = pipefd[0];
    pip->write = pipefd[1];
    return true;
}

#ifndef BUF_SIZE
#define BUF_SIZE 64
#endif

void change_dir(const char *path) {
    if (path == NULL) {
        path = getenv("HOME");
        if (path == NULL) {
            LOG_ERROR("cd: $HOME hasn't been init");
            return;
        }
    }

    if (chdir(path) < 0) {
        LOG_ERROR("cd: %s: %s", path, strerror(errno));
        return;
    }

    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd))) {
        setenv("PWD", cwd, 1);
    }
}

extern char **environ;

void get_env(int fdout) {
    if (fdout > 2) {
        for (char **env = environ; *env != NULL; env++) {
            write(fdout, *env, strlen(*env));
            write(fdout, "\n", 1);
        }
    }
    else {
        for (char **env = environ; *env != NULL; env++) {
            printf("%s\n", *env);
        }
    }
    
}

bool check_cmd(Cmd *cmd, int fdout) {
    if (cmd->count == 0) {
        return false;
    }

    // if cmd == cd
    else if (strcmp(cmd->items[0], "cd") == 0) {
        if (cmd->count == 1) {
            change_dir(NULL);
        }
        else if (cmd->count == 2) {
            change_dir(cmd->items[1]);
        }
        return false;
    }
    
    // if cmd == env
    else if (strcmp(cmd->items[0], "env") == 0) {
        get_env(fdout);
        return false;
    }
    
    // if cmd == exit
    else if (strcmp(cmd->items[0], "exit") == 0) {
        exit(0);
    }
    
    // if cmd == export (parsed: "export" "name" "=" "value")
    else if (strcmp(cmd->items[0], "export") == 0) {
        if (cmd->count != 4) {
            LOG_ERROR("Syntax error in export command");
            LOG_INFO("Usage: export [KEY]=[VALUE]");
            return false;
        }
        if (setenv(cmd->items[1], cmd->items[3], 1) < 0) {
            LOG_ERROR("export: %s", strerror(errno));
            return false;
        }
        return false;
    }

    // if cmd == unsetenv (parsed: "unsetenv" "name")
    else if (strcmp(cmd->items[0], "unsetenv") == 0) {
        if (cmd->count != 2) {
            LOG_ERROR("Syntax error in unsetenv command");
            LOG_INFO("Usage: unsetenv [KEY]");
            return false;
        }
        if (unsetenv(cmd->items[1]) < 0) {
            LOG_ERROR("unsetenv: %s: %s", cmd->items[1], strerror(errno));
            return false;
        }

        return false;
    }
    
    // if cmd == jobs
    else if (strcmp(cmd->items[0], "jobs") == 0) {
        for (size_t i = 0; i < jobs.count; i++) {
            printf("%d\n", jobs.items[i]);
        }
        return false;
    }

    // if cmd == fg
    else if (strcmp(cmd->items[0], "fg") == 0) {
        if (jobs.count != 0) {
            kill(jobs.items[jobs.count - 1], SIGCONT);
        
            siginfo_t *info = (siginfo_t*)calloc(1, sizeof(siginfo_t));
            if (async_prep_waitid(jobs.items[jobs.count - 1], info) == -1) {
                LOG_ERROR("prep_waitid in fg: %s", strerror(errno));
                return false;
            }
            if (async_submit_waitids(1) == -1) {
                LOG_ERROR("submit_waitids in fg: %s", strerror(errno));
                return false;
            }
            free(info);
        }
        return false;
    }
    return true;
}

void close_fd(int fd) {
    if (fd > 2) {
        close(fd);
    }
}

ErrProc procs_flush(Procs *procs) {
    siginfo_t *infos = (siginfo_t*)calloc(procs->count, sizeof(siginfo_t));
    for (size_t i = 0; i < procs->count; i++) {
        if (async_prep_waitid(procs->items[i], &infos[i]) == -1) {
            return ERR_PROC_WAIT_FAIL;
        }
    }
    if (async_submit_waitids(procs->count) == -1) {
        return ERR_PROC_WAIT_FAIL;
    }

    for (size_t i = 0; i < procs->count; i++) {
        if (infos[i].si_code == CLD_STOPPED) {
            LOG_INFO("Proccess stopped");
            da_append(&jobs, procs->items[i]);
        }
        if (infos[i].si_code == CLD_KILLED) LOG_INFO("Killed");
    }
    free(infos);
    procs->count = 0;
    return ERR_PROC_OK;
}

bool cmd_run_async(Cmd *cmd, Procs *procs, int fdin, int fdout, int fderr) {
    if (!check_cmd(cmd, fdout)) return true;
    int p = cmd_start_proc(*cmd, fdin, fdout, fderr);
    if (p == INVALID_PROC) return false;
    
    da_append(procs, p);
    free_tokens(cmd);
    close_fd(fdin);
    close_fd(fdout);
    close_fd(fderr);
    return true;
}

ErrProc execute_commands(Cmd *cmd) {
    shell_state = 1;
    ErrProc result = ERR_PROC_OK;
    
    Cmd cmd_tmp = {0};
    int read_fd = STDIN_FILENO;
    int write_fd = STDOUT_FILENO;
    
    for (size_t i = 0; i < cmd->count; i++) {
        // if symbol |
        if (*cmd->items[i] == '|' && cmd->states[i] == PARSE_STATE_CASUAL) {
            if (i + 1 >= cmd->count) {
                LOG_ERROR("Syntax error near \'|\'");
                return_defer(ERR_PROC_OK);
            }
            
            Pipe pip;
            pipe_create(&pip);
            
            if (write_fd == STDOUT_FILENO) write_fd = pip.write;

            if (!cmd_run_async(&cmd_tmp, &procs, read_fd, write_fd, STDERR_FILENO)) {
                return_defer(ERR_PROC_OK);                
            }
            if (write_fd == pip.write) read_fd = pip.read;
            else read_fd = STDIN_FILENO;
            
            write_fd = STDOUT_FILENO;
        }
        // if symbol >
        else if (*cmd->items[i] == '>' && cmd->states[i] == PARSE_STATE_CASUAL) {
            i++;
            if (i >= cmd->count) {
                LOG_ERROR("Syntax error near \'>\'");
                return_defer(ERR_PROC_OK);
            }

            if (*(cmd->items[i]) == '>') {
                i++;
                if (i >= cmd->count) {
                    LOG_ERROR("Syntax error near \'>\'");
                    return_defer(ERR_PROC_OK);
                }
                write_fd = open(cmd->items[i], O_CREAT | O_WRONLY | O_APPEND, 0644);
            }
            else {
                write_fd = open(cmd->items[i], O_CREAT | O_WRONLY | O_TRUNC, 0644);
            }
            if (write_fd < 0) {
                LOG_ERROR("Couldn't open file %s: %s", cmd->items[i], strerror(errno));
                return_defer(ERR_PROC_OK);
            }

        }

        // if symbol <
        else if (*cmd->items[i] == '<' && cmd->states[i] == PARSE_STATE_CASUAL) {
            i++;
            if (i >= cmd->count) {
                LOG_ERROR("Syntax error near \'<\'");
                return_defer(ERR_PROC_OK);
            }

            read_fd = open(cmd->items[i], O_RDONLY, 0644);
            
            if (read_fd < 0) {
                LOG_ERROR("Couldn't open file %s: %s", cmd->items[i], strerror(errno));
                return_defer(ERR_PROC_OK);
            }
        }

        
        else {
            char *str = strdup(cmd->items[i]);
            da_append(&cmd_tmp, str);
        }
        
    }
    
    if (cmd_tmp.count > 0) {
        if (!cmd_run_async(&cmd_tmp, &procs, read_fd, write_fd, STDERR_FILENO)) {
            return_defer(ERR_PROC_OK);
        }
    }
defer:
    if (cmd_tmp.count > 0) free_tokens(&cmd_tmp);
    free_cmd(&cmd_tmp);
    free_tokens(cmd);
    result = procs_flush(&procs);
    shell_state = 0;
    return result;
}

#undef STB_DA_IMPLEMENTATION
