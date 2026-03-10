#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "abstractions.h"

TypeInfo hash_typeinfo = { .size = sizeof(Hash), .drop = NULL };
TypeInfo entry_typeinfo = { .size = sizeof(Entry), .drop = NULL };
TypeInfo array_typeinfo = { .size = sizeof(Array), .drop = (DropFunction)Array_drop };

/*
 * malloc wrapper which prevents COW and fragmentation from calling realloc.
 */
void *remalloc(void *p, size_t current_count, size_t new_count, size_t size) {
    if (size == 0 || new_count == 0 || new_count > SIZE_MAX / size) {
        return NULL;
    }

    void *new_p = malloc(new_count * size);

    if (new_p) {
        if (current_count > 0 && p != NULL) {
            memcpy(new_p, p, current_count * size);
        }

        free(p);
    }
    
    return new_p;
}

String String_init(char *c_str) {
    size_t len = strlen(c_str);
    String self = malloc(sizeof(struct String_s) + len + 1);

    if (self == NULL) {
        return NULL;
    }
    
    strcpy(self->c_str, c_str);
    self->len = len;

    return self;
}

/*
 * Copies a String. The source string is borrowed.
 * Thus it is still owned by the caller.
 */
String String_copy(String str) {
    size_t size = sizeof(struct String_s) + str->len + 1;
    String self = malloc(size);

    if (self == NULL) {
        return NULL;
    }
    
    memcpy(self, str, size);

    return self;
}

String String_from_slice(char *c_str, size_t len) {
    String self = malloc(sizeof(struct String_s) + len + 1);

    if (self == NULL) {
        return NULL;
    }
    
    memcpy(self->c_str, c_str, len);
    self->c_str[len] = '\0';
    self->len = len;

    return self;
}

void String_drop(String self) {
    free(self);
}

/*
 * Computes a string hash.
 * Copied from somewhere on stackoverflow.
 */
uint32_t String_hash(char *c_str) {
    uint32_t hash = 0;

    if (!c_str) {
        return 0;
    }

    for (char *c = c_str; *c; ++c) {
        hash = hash + *c;
        hash += hash << 10;
        hash ^= hash >> 6;
    }

    hash += hash << 3;
    hash ^= hash >> 11;
    hash += hash << 15;

    return hash;
}

bool Dictionary_init(Dictionary *self, TypeInfo *typeinfo) {
    self->typeinfo = typeinfo;
    
    // entry typeinfo is passed around as reference so we can't allow it to move
    // if the dictionary is moved, in this case it's better to allocate and
    // never touch it again
    if (!(self->entry_typeinfo = malloc(sizeof(TypeInfo)))) {
        printf("Out of memory.\n");
        return false;
    }

    // Entry does not need a drop, as it is handled by the dictionary
    // itself
    self->entry_typeinfo->size = typeinfo->size + sizeof(Entry);
    self->entry_typeinfo->drop = NULL;

    if (!Array_init(&self->hashes, &hash_typeinfo)) {
        free(self->entry_typeinfo);
        return false;
    }

    if (!Array_init(&self->entries, &array_typeinfo)) {
        Array_drop(&self->hashes);
        free(self->entry_typeinfo);
        return false;
    }

    return true;
}

Array *Dictionary_find_hash_slot(Dictionary *self, uint32_t hash) {
    Array_foreach((&(self->hashes)), Hash, hash_element) {
        if (hash_element->hash == hash) {
            return ((Array*)self->entries.data) + hash_element->index;
        }
    }

    return NULL;
}

void* Dictionary_insert(Dictionary *self, String key) {
    uint32_t key_hash = String_hash(key->c_str);
    Array *destination_slot = Dictionary_find_hash_slot(self, key_hash);

    if (destination_slot) {
        for (
            void *entryptr = destination_slot->data;
            entryptr < destination_slot->data + destination_slot->len * self->entry_typeinfo->size;
            entryptr += self->entry_typeinfo->size
        ) {
            Entry *entry = entryptr;

            if (
                entry->key->len == key->len &&
                strcmp(entry->key->c_str, key->c_str) == 0
            ) {
                goto failure;
            }
        }
    }

    if (!destination_slot) {
        destination_slot = Array_push(&self->entries);

        if (!destination_slot) {
            goto failure;
        }

        if (!Array_init(destination_slot, self->entry_typeinfo)) {
            Array_pop(&self->entries, NULL);
            goto failure;
        }

        Hash *new_hash = Array_push(&self->hashes);

        if (!new_hash) {
            Array_pop(&self->entries, NULL);
            goto failure;
        }

        size_t index = destination_slot - (Array*)self->entries.data;
        new_hash->hash = key_hash;
        new_hash->index = index;
    }

    Entry *new_entry = Array_push(destination_slot);

    if (!new_entry) {
        goto failure;
    }

    // String is an owned type, so it can be moved
    new_entry->key = key;
    return &new_entry->value;
failure:
    // String is an owned type, and it's my responsability to drop it
    String_drop(key);
    return NULL;
}

/* Gets an element by key.
 * If the element does not exist return NULL.
 */
void* Dictionary_get(Dictionary *self, char *key) {
    uint32_t key_hash = String_hash(key);
    Array *slot = Dictionary_find_hash_slot(self, key_hash);

    if (!slot) {
        return NULL;
    }

    for (
        void *entryptr = slot->data;
        entryptr < slot->data + slot->len * self->entry_typeinfo->size;
        entryptr += self->entry_typeinfo->size
    ) {
        Entry *entry = entryptr;

        if (
            strcmp(entry->key->c_str, key) == 0
        ) {
            return &entry->value;
        }
    }

    return NULL;
}

/*
 * Drops the dictionary.
 *
 */ 
void Dictionary_drop(Dictionary *self) {
    if (!self->entry_typeinfo || !self->entries.data) {
        return;
    }

    // Entries are custom dropped by the dictionary,
    // as it has knowledge of the underlying type and
    // Entry_drop is not implemented on purpose.
    Array_foreach(&self->entries, Array, slot) {
        for (
            void *entryptr = slot->data;
            entryptr < slot->data + slot->len * self->entry_typeinfo->size;
            entryptr += self->entry_typeinfo->size
        ) {
            Entry *entry = (Entry*)entryptr;
            String_drop(entry->key);

            if (self->typeinfo->drop) {
                self->typeinfo->drop(&entry->value);
            }
        }
    }

    Array_drop(&self->entries);
    Array_drop(&self->hashes);
    free(self->entry_typeinfo);
    self->entry_typeinfo = NULL;
}

/*
 * Initializes a new array object.
 * Parameters:
 * self: reference to array to initialize
 * element_size: sizeof(T), the type of elements to hold.
 * Returns true on success.
 */
bool Array_init(Array *self, TypeInfo *typeinfo) {
    self->typeinfo = typeinfo;
    self->capacity = 8;
    self->len = 0;
    self->data = NULL;
    void *data = malloc(self->capacity * typeinfo->size);

    if (!data) {
        return false;
    }

    self->data = data;
    return true;
}

/*
 * Detach allocated data from the Array and
 * surrenders ownership of memory to the caller.
 * But why would you want anyway?
 */
void* Array_detach(Array *self) {
    void* data = self->data;
    self->data = NULL;
    self->typeinfo = NULL;
    self->capacity = 0;
    self->len = 0;
    return data;
}

/*
 * Drops an Array and frees allocated memory.
 */
void Array_drop(Array *self) {
    if (self->data == NULL) {
        return;
    }

    // drop all elements
    if (self->typeinfo->drop) {
        for (void *p = self->data; p < self->data + self->len * self->typeinfo->size; p += self->typeinfo->size) {
            self->typeinfo->drop(p);
        }
    }

    free(self->data);
    self->data = NULL;
    self->typeinfo = NULL;
    self->capacity = 0;
    self->len = 0;
}

/*
 * Adds a new element at the end of the array.
 * Paramenters:
 * self: the array to add the element to.
 * Returns a reference to the newly added element.
 * It is up to the caller to further initialize the new element.
 */
void* Array_push(Array *self) {
    size_t capacity = self->capacity;
    size_t len = self->len;

    if (capacity <= len) {
        capacity *= 2;

        void *new_data = remalloc(self->data, self->capacity, capacity, self->typeinfo->size);

        if (!new_data) {
            return NULL;
        }

        self->data = new_data;
        self->capacity = capacity;
    }

    self->len++;
    return self->data + (self->len - 1) * self->typeinfo->size;
}

/*
 * Removes the last element of the array.
 * Parameters:
 * self: the array
 * dest: area of memory to copy the deleted element.
 * Returns true on success, false on empty array.
 * If dest is NULL and the underlying type supports it,
 * the item will be dropped.
 */
bool Array_pop(Array *self, void *dest) {
    if (self->len <= 0) {
        return false;
    }

    --self->len;

    if (dest) {
        memcpy(dest, self->data + self->len * self->typeinfo->size, self->typeinfo->size);
    } else if (self->typeinfo->drop) {
        self->typeinfo->drop(self->data + self->len * self->typeinfo->size);
    }

    return true;
}

/*
 * Retrieves a reference to the last element
 * of the array, or NULL if empty.
 */
void* Array_last(Array *self) {
    if (self->len <= 0) {
        return NULL;
    }

    return self->data + (self->len - 1) * self->typeinfo->size;
}

