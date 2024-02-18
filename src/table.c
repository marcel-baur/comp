#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"

#define TABLE_MAX_LOAD 0.75

static Entry* find_entry(Entry* entries, int capacity, ObjString* key) {
    uint32_t index = key->hash % capacity;
    Entry* tombstone = NULL;
    for (;;) {
        Entry* e = &entries[index];
        if (e->key == NULL) {
            if (IS_NIL(e->value)) {
                return tombstone != NULL ? tombstone : e;
            } else {
                if (tombstone == NULL) tombstone = e;
            }
        } else if (e->key == key) {
            return e;
        }
        index = (index + 1) % capacity;
    }
}

static void adjust_capacity(Table* table, int capacity) {
    Entry* entries = ALLOCATE(Entry, capacity);
    for (int i = 0; i < capacity; i++) {
        entries[i].key = NULL;
        entries[i].value = NIL_VAL();
    }

    table->count = 0;
    for (int i = 0; i < table->capacity; i++) {
        Entry* e = &table->entries[i];
        if (e->key == NULL) continue;
        Entry* dest = find_entry(entries, capacity, e->key);
        dest->key = e->key;
        dest->value = e->value;
        table->count++;
    }

    FREE_ARRAY(Entry, table->entries, table->capacity);

    table->entries = entries;
    table->capacity = capacity;
}

void init_table(Table* table) {
    table->count = 0;
    table->capacity = 0;
    table->entries = NULL;
}

void free_table(Table* table) {
    FREE_ARRAY(Entry, table->entries, table->capacity);
    init_table(table);
}

bool table_set(Table* table, ObjString* key, Value value) {
    if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
        int capacity = GROW_CAPACITY(table->capacity);
        adjust_capacity(table, capacity);
    }
    // printf("Still working here\n"); // @Hack: this seems to stop a fatal error?!
    Entry* entry = find_entry(table->entries, table->capacity, key);
    bool isNewKey = entry->key == NULL;
    if (isNewKey && IS_NIL(entry->value)) table->count++;
    entry->key = key;
    entry->value = value;
    return isNewKey;
}

void table_add_all(Table* from, Table* to) {
    for (int i = 0; i < from->capacity; i++) {
        Entry* e = &from->entries[i];
        if (e->key != NULL) {
            table_set(to, e->key, e->value);
        }
    }
}

bool table_get(Table* table, ObjString* key, Value* value) {
    if (table->count == 0) return false;
    Entry* e = find_entry(table->entries, table->capacity, key);
    if (e->key == NULL) return false;
    *value = e->value;
    return true;
}

bool table_delete(Table* table, ObjString* key) {
    if (table->count == 0) return false;
    Entry* e = find_entry(table->entries, table->capacity, key);
    if (e->key == NULL) return false;
    e->key = NULL;
    e->value = BOOL_VAL(true);
    return true;
}

ObjString* table_find_string(Table* table, const char* chars, int length, uint32_t hash) {
    if (table->count == 0) {
        return NULL;
    }
    uint32_t idx = hash % table->capacity;
    for (;;) {
        Entry* e = &table->entries[idx];
        if (e->key == NULL) {
            if (IS_NIL(e->value)) return NULL;
        } else if (
            e->key->length == length 
            && e->key->hash == hash 
            && memcmp(e->key->chars, chars, length) == 0
        ) {
            return e->key;
        }

        idx = (idx + 1) % table->capacity;
    }
}

void mark_table(Table *table) {
    for (int i = 0; i < table->capacity; i++) {
        Entry *entry = &table->entries[i];
        mark_object((Obj*)entry->key);
        mark_value(entry->value);
    }
}


















