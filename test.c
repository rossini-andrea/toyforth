#include <stdio.h>
#include "abstractions.h"

TypeInfo int_typeinfo = { .size = sizeof(int), .drop = NULL };

int test_Dictionary();
int test_Array();

int main() {
    int errors = 0;
    errors += test_Dictionary();
    errors += test_Array();
    return errors;
}

int test_Array() {
    int errors = 0;
    printf("================================================================\n");
    printf("Testing Array\n");
    Array array;

    if (!Array_init(&array, &int_typeinfo)) {
        printf("INITIALIZATION FAILED\n");
        return 1;
    }

    // Test pushing elements
    int *first = (int*)Array_push(&array);
    if (first == NULL) {
        printf("PUSH FIRST FAILED\n");
        errors++;
        goto cleanup;
    }
    *first = 42;

    int *second = (int*)Array_push(&array);
    if (second == NULL) {
        printf("PUSH SECOND FAILED\n");
        errors++;
        goto cleanup;
    }
    *second = 12;

    // Test getting elements
    int last_val = *(int*)Array_last(&array);
    if (last_val != 12) {
        printf("GET LAST FAILED\n");
        errors++;
        goto cleanup;
    }

    // Test popping elements
    int popped_val;
    if (!Array_pop(&array, &popped_val)) {
        printf("POP FAILED\n");
        errors++;
        goto cleanup;
    }

    if (popped_val != 12) {
        printf("POP VALUE WRONG\n");
        errors++;
        goto cleanup;
    }

    // Test that we can still get the first element
    int remaining_val = *(int*)Array_last(&array);
    if (remaining_val != 42) {
        printf("REMAINING VALUE WRONG\n");
        errors++;
        goto cleanup;
    }

    printf("SUCCESS\n");

cleanup:
    Array_drop(&array);
    return errors;
}

int test_Dictionary() {
    int errors = 0;
    printf("================================================================\n");
    printf("Testing Dictionary\n");
    Dictionary dictionary;

    if (!Dictionary_init(&dictionary, &int_typeinfo)) {
        printf("INITIALIZATION FAILED\n");
        return 1;
    }

    // Test basic insert and get
    *(int*)Dictionary_insert(&dictionary, String_init("quarantadue")) = 42;
    *(int*)Dictionary_insert(&dictionary, String_init("dodici")) = 12;
    *(int*)Dictionary_insert(&dictionary, String_init("uno")) = 1;

    printf("Get quarantadue ");

    if (*(int*)Dictionary_get(&dictionary, "quarantadue") == 42) {
        printf("success\n");
    } else {
        printf("FAIL\n");
        ++errors;
    }

    // Test getting non-existent key
    printf("Get non-existent key ");
    if (Dictionary_get(&dictionary, "nonexistent") == NULL) {
        printf("success\n");
    } else {
        printf("FAIL\n");
        ++errors;
    }

    // Test overwriting existing key
    printf("Overwrite key ");
    int *ptr = (int*)Dictionary_get(&dictionary, "quarantadue");
    if (ptr) {
        *ptr = 99;
    }
    if (*(int*)Dictionary_get(&dictionary, "quarantadue") == 99) {
        printf("success\n");
    } else {
        printf("FAIL\n");
        ++errors;
    }

    Dictionary_drop(&dictionary);

    return errors;
}

