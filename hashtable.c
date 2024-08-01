// Amrita Pasupuleti
// CSE 130, asgn 4: multi-threaded http server
// hashtable.c
// defines a hashtable for storing reader and write locks

#include "hashtable.h"

int hash_function(const char *key, int size) {
    int hash;
    for (int i = 0; key[i] != '\0'; i++) {
        hash = (30 * hash + key[i]) % size;
    }
    return hash;
}

hash_table_t *hash_table_create(int size) {
    hash_table_t *ht = (hash_table_t *) malloc(sizeof(hash_table_t));
    ht->size = size;
    ht->table = (entry_t **) calloc(size, sizeof(entry_t *));
    return ht;
}

void hash_table_free(hash_table_t *ht) {
    for (int i = 0; i < ht->size; i++) {
        for (entry_t *entry = ht->table[i]; entry != NULL;) {
            entry_t *next = entry->next;
            free(entry->key);
            free(entry);
            entry = next;
        }
    }
    free(ht->table);
    free(ht);
}

void hash_table_insert(hash_table_t *ht, const char *key, rwlock_t *rwlock) {
    int index = hash_function(key, ht->size);
    entry_t *new = (entry_t *) malloc(sizeof(entry_t));
    new->key = strdup(key);
    new->rwlock = rwlock;
    new->next = ht->table[index];
    ht->table[index] = new;
}

rwlock_t *get_hash(hash_table_t *ht, const char *key) {
    int i = hash_function(key, ht->size);
    for (entry_t *entry = ht->table[i]; entry != NULL; entry = entry->next) {
        if (strcmp(entry->key, key) == 0) {
            return entry->rwlock;
        }
    }
    return NULL;
}
