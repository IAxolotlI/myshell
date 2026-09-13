#ifndef PARSER_H
#define PARSER_H

typedef enum ErrInput {
    ERR_INPUT_OK   = 1,
    ERR_INPUT_EXIT = 0,
} ErrInput;

typedef enum ErrParse {
    ERR_PARSE_OK,
    ERR_PARSE_QUOTE,
    ERR_PARSE_DQUOTE,
} ErrParse;

typedef enum ParseState {
    PARSE_STATE_CASUAL,
    PARSE_STATE_QUOTE,
    PARSE_STATE_DQUOTE,
} ParseState;

typedef struct {
    char **items;
    size_t count;
    size_t capacity;
    ParseState *states;
    size_t count_states;
    size_t capacity_states;
} Cmd;

void free_tokens(Cmd *cmd);
void free_cmd(Cmd *cmd);
ErrParse process_string(Cmd *cmd, char *str, const char *delim_useless, const char *delim_useful);
ErrInput get_string(char** str);

#endif
