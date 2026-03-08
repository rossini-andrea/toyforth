#include <stdio.h>
#include "abstractions.h"

TypeInfo int_typeinfo = { .size = sizeof(int), .drop = NULL };

int test_Dictionary();

int main() {
    int errors = 0;
    errors += test_Dictionary();
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

    printf("setting values\n");
    *(int*)Dictionary_insert(&dictionary, String_init("quarantadue")) = 42;
    *(int*)Dictionary_insert(&dictionary, String_init("dodici")) = 12;
    *(int*)Dictionary_insert(&dictionary, String_init("uno")) = 1;

    printf("get quarantadue ");

    if (*(int*)Dictionary_get(&dictionary, "quarantadue") == 42) {
        printf("success\n");
    } else {
        printf("FAIL\n");
        ++errors;
    }

    Dictionary_drop(&dictionary);

    return errors;
}

