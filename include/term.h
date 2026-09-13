#ifndef TERM_H
#define TERM_H

void generate_prompt();
void refresh_prompt(char c);
void enable_raw_mode();
void disable_raw_mode();
int read_line_interactive(TrieRoot *trie, char *buffer, size_t max_len);
void append_system_commands(TrieRoot *root);

#endif
