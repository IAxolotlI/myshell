#ifndef TRIE_H
#define TRIE_H
#include <stdbool.h>
typedef struct TrieRoot TrieRoot;
typedef struct TrieNode TrieNode;
typedef enum {
    ERR_TRIE_NOT_FOUND,
    ERR_TRIE_FOUND_PARTLY,
    ERR_TRIE_FOUND_ENTIRELY,
} ErrFind;

TrieRoot* trie_create();
void trie_append(TrieRoot *root, char *str);
void trie_free(TrieRoot *root);
ErrFind trie_find(TrieRoot *root, char *substr, char **res);

#endif
