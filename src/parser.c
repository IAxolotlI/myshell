#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include "parser.h"
#include "logger.h"
#include "trie.h"
#define STB_DA_IMPLEMENTATION
#include "macro.h"

#define BUF_SIZE    64
#define DIR_SIZE    128

bool char_in_string(char chr, const char *str) {
    const char *p_delim = str;
    while (*p_delim) {
        if (chr == *p_delim) {
            return true;
        }
        p_delim++;
    }
    return false;
}

void free_tokens(Cmd *cmd) {
    for (size_t i = 0; i < cmd->count; i++) {
        free(cmd->items[i]);
        cmd->items[i] = NULL;
    }
    cmd->count = 0;
    cmd->count_states = 0;
}

void free_cmd(Cmd *cmd) {
    if (cmd->items) free(cmd->items);
    if (cmd->states) free(cmd->states);
}


void alloc_word(char **word, char *start_word, char *end_word) {
    size_t word_len = end_word - start_word;
    if (*word == NULL) {
        *word = (char*)calloc(word_len + 1, sizeof(char));
        if (end_word - start_word > 0) memcpy(*word, start_word, word_len);
    }
    else {
        size_t old_len = strlen(*word);
        *word = (char*)realloc(*word, (old_len + word_len + 1) * sizeof(char));
        if (end_word - start_word > 0) memcpy(*word + old_len, start_word, word_len);
        (*word)[old_len + word_len] = '\0';
    }  
}

ErrParse process_string(Cmd *cmd, char *str, const char *delim_useless, const char *delim_useful) {
    char *tmp_str          = strdup(str);
    char *start_word       = tmp_str;
    char *end_word         = tmp_str;
    bool is_single_quotes  = false;
    bool is_double_quotes  = false;
    ParseState parse_state = PARSE_STATE_CASUAL;
    bool flag = true;
    char *word = NULL;
    while (flag) {

        if (*end_word == '\0') {
            if (end_word - start_word > 0 && !is_single_quotes && !is_double_quotes) {
                if (!char_in_string(*start_word, delim_useless)) {
                    alloc_word(&word, start_word, end_word);
                    
                    da_append(cmd, word);
                    da_append_state(cmd, parse_state);
                    
                    word = NULL;
                    start_word = end_word;
                }
            }

            flag = false;
            continue;
        }

        if (*end_word == '\'' && !is_single_quotes && !is_double_quotes) {
            if (end_word != tmp_str) {
                if (!char_in_string(*(end_word - 1), delim_useless)) {
                    if (end_word - start_word > 0) {
                        alloc_word(&word, start_word, end_word);
                    }
                }                
            }
            is_single_quotes = !is_single_quotes;
            parse_state = PARSE_STATE_QUOTE;
            start_word = end_word + 1;
            end_word++;
            continue;
        }

        if (*end_word == '\"' && !is_single_quotes && !is_double_quotes) {
            if (end_word != tmp_str) {
                if (!char_in_string(*(end_word - 1), delim_useless)) {
                    if (end_word - start_word > 0) {
                        alloc_word(&word, start_word, end_word);
                    }
                }                
            }
            is_double_quotes = !is_double_quotes;
            parse_state = PARSE_STATE_DQUOTE;
            start_word = end_word + 1;
            end_word++;
            continue;
        }
        
        if (*end_word == '\'' && is_single_quotes && !is_double_quotes) {
            alloc_word(&word, start_word, end_word);
            da_append(cmd, word);
            da_append_state(cmd, parse_state);
            word = NULL;
            is_single_quotes = !is_single_quotes;
            parse_state = PARSE_STATE_CASUAL;
            start_word = end_word + 1;
            end_word++;
            continue;
        }

        if (*end_word == '\"' && !is_single_quotes && is_double_quotes) {
            alloc_word(&word, start_word, end_word);
            da_append(cmd, word);
            da_append_state(cmd, parse_state);
            word = NULL;
            is_double_quotes = !is_double_quotes;
            parse_state = PARSE_STATE_CASUAL;
            start_word = end_word + 1;
            end_word++;
            continue;
        }

        
        if (char_in_string(*end_word, delim_useless) && !is_single_quotes && !is_double_quotes) {
            if (!char_in_string(*start_word, delim_useless)) {
                alloc_word(&word, start_word, end_word);
                da_append(cmd, word);
                da_append_state(cmd, parse_state);
                word = NULL;
                start_word = end_word;
            }
        }

        if (char_in_string(*end_word, delim_useful) && !is_single_quotes && !is_double_quotes) {
            if ((!char_in_string(*start_word, delim_useful) && !char_in_string(*start_word, delim_useless))
                              || (char_in_string(*start_word, delim_useful) && end_word != start_word)) {
                alloc_word(&word, start_word, end_word);
                da_append(cmd, word);
                da_append_state(cmd, parse_state);
                word = NULL;
                start_word = end_word;
            }
        }
        
        if (!char_in_string(*end_word, delim_useless) && !is_single_quotes && !is_double_quotes) {
            if (char_in_string(*start_word, delim_useless)) {
                start_word = end_word;
            }
        }

        if (!char_in_string(*end_word, delim_useful) && !is_single_quotes && !is_double_quotes) {
            if (char_in_string(*start_word, delim_useful)) {
                alloc_word(&word, start_word, end_word);
                da_append(cmd, word);
                da_append_state(cmd, parse_state);
                word = NULL;
                start_word = end_word;
            }
        }
        
        end_word++;
    }
    if (word) free(word);
    free(tmp_str);
    
    if (is_single_quotes) {
        LOG_ERROR("Parser: Missing quote");
        return ERR_PARSE_QUOTE;
    }
    else if (is_double_quotes) {
        LOG_ERROR("Parser: Missing dquote");
        return ERR_PARSE_DQUOTE;
    }
    return ERR_PARSE_OK;
}
#undef STB_DA_IMPLEMENTATION
