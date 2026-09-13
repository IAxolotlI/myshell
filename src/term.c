#include <ctype.h>
#include <termios.h>
#include <sys/select.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/utsname.h>
#include "trie.h"
#include "task.h"

#define COLOR_ERROR        "\33[38;5;160m"
#define COLOR_DEFAULT      "\33[38;5;231m"
#define COLOR_PROMPT       "\33[38;5;26m"
#define COLOR_MAGENTA      "\33[38;5;200m"
#define COLOR_GREEN        "\33[38;5;2m"
#define COLOR_PURPLE       "\33[38;5;129m"
#define COLOR_RESET        "\33[0m"
#define COLOR_SUGGEST      "\33[38;5;245m"
#define COLOR_COMMAND      "\33[38;5;40m"
#define CONTROL_ERASE      "\33[K"
#define CONTROL_ERASE_LINE "\33[0G\33[2K"

#define DIR_SIZE      256
#define KEY_ENTER     10
#define KEY_RETURN    13
#define KEY_TAB       9
#define KEY_BACKSPACE 127
#define KEY_ESC       27
#define KEY_EXIT      4
#define KEY_RIGHT     1001 
#define KEY_UP        1002
#define KEY_DOWN      1003
#define KEY_LEFT      1004

void append_system_commands(TrieRoot *root) {
    char *path_env = getenv("PATH");
    if (!path_env) return;

    char *path_copy = strdup(path_env);
    char *dir_path = strtok(path_copy, ":");

    while (dir_path != NULL) {
        DIR *d = opendir(dir_path);
        if (d) {
            struct dirent *dir;
            while ((dir = readdir(d)) != NULL) {
                if (dir->d_name[0] == '.') continue;
                trie_append(root, dir->d_name);
            }
            closedir(d);
        }
        dir_path = strtok(NULL, ":");
    }
    free(path_copy);
    trie_append(root, "cd");
    trie_append(root, "export");
    trie_append(root, "unsetenv");
    trie_append(root, "env");
    trie_append(root, "exit");
}

void generate_prompt() {
    char *prompt = (char*)calloc(DIR_SIZE, sizeof(char));
    char cwd[DIR_SIZE];
    getcwd(cwd, sizeof(cwd));
    memcpy(prompt, cwd, sizeof(cwd));
    struct utsname sys_info;
    char *host = NULL;
    if (uname(&sys_info) == 0) {
        host = sys_info.nodename;
    }

    char *user = getenv("USER");
    char *pos = strrchr(prompt, '/');
    if (pos) {
        if (prompt < pos) {
            pos--;
            while (*pos != '/') {
                if (prompt < pos)  {
                    if (*(pos - 1) == '/') break;
                    pos--;
                }
                else break;
            }
        }
        printf("%s%s@%s%s %s%s ❯%s ", COLOR_GREEN, user, COLOR_PURPLE, host, COLOR_MAGENTA, pos, COLOR_RESET);
    }
    else printf("%s%s@%s%s %s%s ❯%s ", COLOR_GREEN, user, COLOR_PURPLE, host, COLOR_MAGENTA, prompt, COLOR_RESET);
    fflush(stdout);
    free(prompt);
}

struct termios orig_termios;

void disable_raw_mode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    printf("\33[0 q");
}

void enable_raw_mode() {    
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disable_raw_mode);
    struct termios raw = orig_termios;
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    printf("\33[5 q");
    fflush(stdout);
}

void refresh_screen(TrieRoot *trie, char *buffer) {
    printf("\r\33[K");
    generate_prompt();
    char *suffix = NULL;
    bool expecting_cmd = true;
    size_t len = strlen(buffer);
    size_t i = 0;
    char *full_cmd = NULL;
    char *found_word = NULL;
    trie_find(trie, buffer, &full_cmd);
    size_t word_len = 0;    
    while (i < len) {
        if (buffer[i] == '|') {
            expecting_cmd = true;
            printf("%s%c%s", COLOR_DEFAULT, buffer[i++], COLOR_RESET);
            continue;
        }
    
        if (buffer[i] == ' ') {
            if (!expecting_cmd) {
                expecting_cmd = false; 
            }
            printf("%c", buffer[i++]);
            continue;
        }
    
        if (expecting_cmd) {
            size_t start = i;
            while (i < len && buffer[i] != ' ' && buffer[i] != '|') {
                i++;
            }
            word_len = i - start;
            char *word = (char*)calloc(word_len + 1, sizeof(char));
            memcpy(word, buffer + start, word_len);
            word[word_len] = '\0';
            
            ErrFind flag = trie_find(trie, word, &found_word);
            if (flag == ERR_TRIE_FOUND_ENTIRELY) printf("%s%s%s", COLOR_COMMAND, word, COLOR_RESET);
            else printf("%s%s%s", COLOR_ERROR, word, COLOR_RESET);
            if (i != len) {
                free(found_word);
                found_word = NULL;
            }
            free(word);
            expecting_cmd = false; 
            
        }
        else {
            printf("%s%c%s", COLOR_DEFAULT, buffer[i++], COLOR_RESET);
        }
    }
    if (full_cmd) {
        suffix = full_cmd + len;
    }
    else if (found_word) {
        suffix = found_word + word_len;
    }
    
    size_t suffix_len = suffix ? strlen(suffix) : 0;
    if (suffix_len > 0) {
        printf("%s%s%s", COLOR_SUGGEST, suffix, COLOR_RESET);
        printf("\33[%dD", (int)suffix_len);
    }
    free(full_cmd);
    free(found_word);
    fflush(stdout);
}

int parse_escape_sequence() {
    char seq1 = '0';
    char seq2 = '0';
    async_read_byte(&seq1);
    async_read_byte(&seq2);
    if (seq1 == '[') {
        switch (seq2) {
            case 'C': return KEY_RIGHT;
            case 'A': return KEY_UP;
            case 'B': return KEY_DOWN;
            case 'D': return KEY_LEFT;
        }
    }
    return KEY_ESC;
}

int read_line_interactive(TrieRoot *trie, char *buffer, size_t max_len) {
    size_t len = 0;
    memset(buffer, 0, max_len);
    
    printf("\r");
    generate_prompt();
    int key = KEY_EXIT;
    char c = '0';
    while (len < max_len - 1) {
        async_read_byte(&c);
        key = (int)c;
        if (key == KEY_ESC) {
            key = parse_escape_sequence();
        }
        if (key == KEY_ENTER || key == KEY_RETURN) {
            buffer[len] = '\0';
            printf("%s", CONTROL_ERASE);
            printf("\r\n");
            break;
        }
        if (key == 4) {
            printf("EXIT\n");
            return 1;
        }
        
        else if (key == KEY_BACKSPACE) {
            if (len > 0) {
                len--;
                buffer[len] = '\0';

                if (len == 0) {
                    printf("\r\33[K");
                    generate_prompt();
                    continue;
                }
                refresh_screen(trie, buffer);
            }
        }

        else if (key == KEY_RIGHT) {
            char *found_word = NULL;
            ErrFind flag = trie_find(trie, buffer, &found_word);
            if (found_word == NULL) {
                char *pos = buffer + len;
                while (pos > buffer && *(pos - 1) != ' ') {
                    pos--;
                }
                flag = trie_find(trie, pos, &found_word);
                if ((flag == ERR_TRIE_FOUND_PARTLY || flag == ERR_TRIE_FOUND_ENTIRELY) && found_word != NULL) {
                    char *suffix = found_word + strlen(pos);
                    strcat(buffer, suffix);
                    len = strlen(buffer);
                    refresh_screen(trie, buffer);
                }
            }
            else {
                if ((flag == ERR_TRIE_FOUND_PARTLY || flag == ERR_TRIE_FOUND_ENTIRELY) && found_word != NULL) {
                    char *suffix = found_word + strlen(buffer);
                    strcat(buffer, suffix);
                    len = strlen(buffer);
                    refresh_screen(trie, buffer);
                }
            }
            free(found_word);
        }
        else if (key == ' ') {
            buffer[len++] = ' ';
            buffer[len] = '\0';
            refresh_screen(trie, buffer);
        }


        else if (key >= 32 && key < 127) {
            buffer[len++] = (char)key;
            buffer[len] = '\0';
            refresh_screen(trie, buffer);
        }
    }
    return 0;
}
