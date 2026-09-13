#define STB_DA_IMPLEMENTATION
#include "macro.h"

#include "trie.h"
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>

struct TrieNode {
    size_t command_id;
    int max_sub_id_idx;
    size_t count;
    size_t capacity;
    struct TrieNode **items;
    struct TrieNode *parent;
    char *key;
};

struct TrieRoot {
    TrieNode* items[256];
};


static size_t static_id = 1;

TrieRoot* trie_create() {
    TrieRoot *root = (TrieRoot*)calloc(1, sizeof(TrieRoot));
    assert(root != NULL && "Out of RAM");
    return root;
}

TrieNode* node_create() {
    TrieNode *node = (TrieNode*)calloc(1, sizeof(TrieNode));
    assert(node != NULL && "Out of RAM");
    return node;
}

void node_free_recursive(TrieNode *node) {
    if (!node) return;
    
    for (size_t i = 0; i < node->count; i++) {
        node_free_recursive(node->items[i]);
    }
    if (node->key) {
        free(node->key);
    }
    if (node->items) {
        free(node->items);
    }
    free(node);
}

void trie_free(TrieRoot *root) {
    if (!root) return;
    for (int i = 0; i < 256; i++) {
        if (root->items[i] != NULL) {
            node_free_recursive(root->items[i]);
            root->items[i] = NULL;
        }
    }
    free(root);
}

TrieNode* get_child(TrieNode *node, char *str, char **same, char **diff_key, char **diff_str) {
    size_t key_len = strlen(node->key);
    size_t str_len = strlen(str);
    size_t i = 0;
    size_t len = str_len;
    if (key_len > str_len) {
        len = key_len;
    }
    if (key_len == str_len) {
        (*same) = (char*)calloc(1, sizeof(char));
        (*diff_str) = (char*)calloc(1, sizeof(char));
        (*diff_key) = (char*)calloc(1, sizeof(char));
    }
    
    for (i = 0; i < len; i++) {
        if (str[i] != node->key[i]) {
            if (*same) free(*same);
            (*same) = strdup(str); (*same)[i] = '\0';
            if (*diff_str) free(*diff_str);
            (*diff_str) = strdup(str + i);
            if (*diff_key) free(*diff_key);
            (*diff_key) = strdup(node->key + i);
            break;
        }
    }
    TrieNode *next_node = node;
    size_t j = 0;
    for (j = 0; j < node->count; j++) {
        if (*(next_node->items[j]->key) == str[i]) {
            next_node = next_node->items[j];
            break;
        }
    }
    if (next_node != node) {
        node->max_sub_id_idx = j;
        free(*same); *same = NULL;
        free(*diff_str); *diff_str = NULL;
        free(*diff_key); *diff_key = NULL;
        return get_child(next_node, str + i, same, diff_key, diff_str);
    }
    else return node;
}


void trie_append(TrieRoot *root, char *str) {
    TrieNode *node = root->items[(unsigned char)*str];
    if (node != NULL) {
        char *diff_str = NULL;
        char *diff_key = NULL;
        char *same = NULL;
        TrieNode *child = get_child(node, str, &same, &diff_key, &diff_str);

        if (strlen(diff_key) == 0 && strlen(diff_str) == 0) {
            free(diff_key);
            free(diff_str);
            free(same);
            child->command_id = static_id++;
            child->max_sub_id_idx = -1;
        }
        else if (strlen(diff_key) == 0) {
            free(diff_key);
            free(same);
            TrieNode *new_node = node_create();
            new_node->parent = child;
            new_node->key = diff_str;
            new_node->command_id = static_id++;
            new_node->max_sub_id_idx = -1;
            da_append(child, new_node);
            child->max_sub_id_idx = child->count - 1;
        }
        else if (strlen(diff_str) == 0) {
            free(diff_str);
            TrieNode *new_node = node_create();
            new_node->parent = child;
            new_node->key = diff_key;
            new_node->max_sub_id_idx = child->max_sub_id_idx;
            new_node->command_id = child->command_id;
            new_node->count = child->count;
            new_node->capacity = child->capacity;
            new_node->items = child->items;

            if (child->key) free(child->key);
            child->key = same;
            child->max_sub_id_idx = -1;
            child->command_id = static_id++;
            child->count = 0;
            child->capacity = 0;
            child->items = NULL;
            da_append(child, new_node);
        }
        else {
            TrieNode *left = node_create();
            TrieNode *right = node_create();
            left->command_id = child->command_id;
            left->max_sub_id_idx = child->max_sub_id_idx;
            left->parent = child;
            left->key = diff_key;
            left->items = child->items;
            left->count = child->count;
            left->capacity = child->capacity;
            right->command_id = static_id++;
            right->max_sub_id_idx = -1;
            right->parent = child;
            right->key = diff_str;
            if (child->key) free(child->key);
            child->key = same;
            child->items = NULL;
            child->count = 0;
            child->capacity = 0;
            child->max_sub_id_idx = 1;
            child->command_id = 0;
            da_append(child, left);
            da_append(child, right);
        }
        
        
    }
    else {
        root->items[(unsigned char)*str] = node_create();
        root->items[(unsigned char)*str]->key = strdup(str);
        root->items[(unsigned char)*str]->command_id = static_id++;
        root->items[(unsigned char)*str]->max_sub_id_idx = -1;
    }
    
}

ErrFind node_find_last(TrieNode* node, char **res, char *str) {
    ErrFind err = ERR_TRIE_FOUND_PARTLY;
    while (node->max_sub_id_idx != -1) {
        if (node->max_sub_id_idx < (int)node->count) {
            if (*res == NULL) {
                *res = strdup(node->key);
            }
            else {
                *res = realloc(*res, strlen(*res) + strlen(node->key) + 1);
                strcat(*res, node->key);
            }
            if (strcmp(str, *res) == 0 && node->command_id != 0) {
                err = ERR_TRIE_FOUND_ENTIRELY;
            }
            
            node = node->items[node->max_sub_id_idx];
        }
        else {
            assert("You found bug: max_sub_id_idx invalid");
            exit(1);
        }
        
    }
    if (*res == NULL) {
        *res = strdup(node->key);
    }
    else {
        *res = realloc(*res, strlen(*res) + strlen(node->key) + 1);
        strcat(*res, node->key);
    }
    if (strcmp(str, *res) == 0 && node->command_id != 0) {
        err = ERR_TRIE_FOUND_ENTIRELY;
    }
    return err;
}



ErrFind trie_find(TrieRoot *root, char *substr, char **res) {
    if (strlen(substr) == 0) {
        return false;
    }
    if (!root->items[(unsigned char)*substr]) {
        return false;
    }
    if (*(root->items[(unsigned char)*substr]->key) != substr[0]) {
        return false;
    }
    TrieNode *node = root->items[(unsigned char)*substr];
    size_t j = 0;
    size_t i = 0;
    for (i = 0; i < strlen(substr); i++) {
        if (substr[i] != node->key[j]) {
            if (*res == NULL) {
                *res = strdup(node->key);
            }
            else {
                *res = realloc(*res, strlen(*res) + strlen(node->key) + 1);
                strcat(*res, node->key);
            }

            bool flag = false;
            for (size_t k = 0; k < node->count; k++) {
                if (*node->items[k]->key == substr[i]) {
                    node = node->items[k];
                    flag = true;
                    break;
                }
            }
            if (!flag) {
                free(*res);
                *res = NULL;
                return ERR_TRIE_NOT_FOUND;
            }
            j = 0;
            i--;
            continue;
        }
        j++;
    }
    ErrFind err = node_find_last(node, res, substr);
    if (strlen(substr) == strlen(root->items[(unsigned char)*substr]->key) && root->items[(unsigned char)*substr]->command_id != 0) {
        err = ERR_TRIE_FOUND_ENTIRELY;
    }
    return err;
}
#undef STB_DA_IMPLENETATION
