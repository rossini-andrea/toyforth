#pragma once

#include <stdint.h>
#include <stddef.h>

typedef void (*DefaultInitFunction)(void*);
typedef void (*DropFunction)(void*);

typedef struct {
    size_t size;
    DefaultInitFunction default_init;
    DropFunction drop;
} TypeInfo;

// Provide TypeInfo for common types
extern TypeInfo array_typeinfo;
extern TypeInfo char_typeinfo;

typedef struct String_s {
    size_t len;
    char c_str[];
} *String;

/* A dynamic array for untyped data */
typedef struct {
    TypeInfo *typeinfo;
    size_t capacity;
    size_t len;
    void *data;
} Array;

typedef struct {
    uint32_t hash;
    size_t index;
} Hash;

typedef struct {
    String key;
    char value[];
} Entry;

/* A dictionary */
typedef struct {
    TypeInfo *typeinfo;
    TypeInfo *entry_typeinfo;
    size_t bucket_mask;
    Array /* of Array of Entry */ buckets;
} Dictionary;

/* Forces typed loop on an Array
 * Parameters:
 * a: a reference to an Array object
 * t: the type to cast the elements
 * x: iteration control variable
 */
#define Array_foreach(a, t, x) for (t *x = (a)->data; x < (t*)((a)->data) + (a)->len; ++x)

String String_init(char *c_str);
String String_copy(String str);
String String_from_slice(char *c_str, size_t len);
String String_from_array(Array *array);
void String_drop(String self);
uint32_t String_hash(char *c_str);

bool Dictionary_init(Dictionary *self, TypeInfo *typeinfo);
void* Dictionary_insert(Dictionary *self, String key);
void* Dictionary_get(Dictionary *self, char *key);
void Dictionary_drop(Dictionary *self);

bool Array_init(Array *self, TypeInfo *typeinfo);
bool Array_init_with_capacity(Array *self, TypeInfo *typeinfo, size_t size);
void* Array_detach(Array *self);
void Array_drop(Array *self);
void* Array_push(Array *self);
bool Array_pop(Array *self, void *dest);
void* Array_last(Array *self);

