#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "abstractions.h"

typedef struct {
    enum {
        number,
        boolean,
        character,
        string,
        plus_op,
        minus_op,
        mul_op,
        div_op,
        eq_op,
        lt_op,
        lte_op,
        gt_op,
        gte_op,
        word,
        command_print
    } type;
    union {
        int numbervalue;
        char charvalue;
        bool booleanvalue;
        String stringvalue;
        String wordname;
    };
} TfElement;

void TfElement_drop(TfElement *self) {
    if (self->type == string && self->stringvalue) {
        String_drop(self->stringvalue);
        self->stringvalue = NULL;
    }

    if (self->type == word && self->wordname) {
        String_drop(self->wordname);
        self->wordname = NULL;
    }
}

/*
 * Checks wether a TfElement contains a true value.
 * The result is stored in result, while a return
 * value of false signals undefined test.
 */
bool TfElement_is_true(TfElement *self, bool *result) {
    switch (self->type) {
        case number: *result = self->numbervalue; return true;
        case boolean: *result = self->booleanvalue; return true;
        case character: *result = self->charvalue; return true;
    }

    return false;
}

TypeInfo tfelement_typeinfo = { .size = sizeof(TfElement), .drop = (DropFunction)TfElement_drop };

typedef struct _TfParser {
    char *token;
    bool (*state_func)(struct _TfParser *);
    Array /* TfElement */ result;
} TfParser;

#ifdef TCO_ACTIVE
    #define STATE_TRANSFER(s) \
        return s(parser);
#else
    #define STATE_TRANSFER(s) \
        parser->state_func = s; \
        return true;
#endif

bool TfParser_state_start(TfParser *parser);
bool TfParser_state_negative_number(TfParser *parser);
bool TfParser_state_number(TfParser *parser);
bool TfParser_state_op(TfParser *parser);
bool TfParser_state_word(TfParser *parser);
bool TfParser_state_minus_or_number(TfParser *parser);
bool TfParser_state_print(TfParser *parser);
bool TfParser_state_equals(TfParser *parser);
bool TfParser_state_lt(TfParser *parser);
bool TfParser_state_lte(TfParser *parser);
bool TfParser_state_gt(TfParser *parser);
bool TfParser_state_gte(TfParser *parser);

bool TfParser_init(TfParser *parser, char *token) {
    parser->token = token;
    parser->state_func = TfParser_state_start;

    return Array_init(&parser->result, &tfelement_typeinfo);
}

void TfParser_drop(TfParser *parser) {
    Array_drop(&parser->result);
}

bool TfParser_state_start(TfParser *parser) {
    for (; *parser->token; ++parser->token) {
        if (isspace(*parser->token)) {
            continue;
        }
        if (isalpha(*parser->token)) {
            STATE_TRANSFER(TfParser_state_word);
        }
        if (isdigit(*parser->token)) {
            STATE_TRANSFER(TfParser_state_number);
        }
        if (*parser->token == '-') {
            STATE_TRANSFER(TfParser_state_minus_or_number);
        }
        if (*parser->token == '.') {
            STATE_TRANSFER(TfParser_state_print);
        }
        if (*parser->token == '=') {
            STATE_TRANSFER(TfParser_state_equals);
        }
        if (*parser->token == '<') {
            STATE_TRANSFER(TfParser_state_lt);
        }
        if (*parser->token == '>') {
            STATE_TRANSFER(TfParser_state_gt);
        }
        if (strchr("+*/", *parser->token)) {
            STATE_TRANSFER(TfParser_state_op);
        }
    }

    parser->state_func = NULL;
    return true;
}

bool TfParser_state_number_impl(TfParser *parser, int factor) {
    int accum = *parser->token - '0';

    for (++parser->token; *parser->token; ++parser->token) {
        /* if space or implicitly, end of parsing, add number to result */
        if (isspace(*parser->token)) {
            break;
        }

        /* if digit accumulate */
        if (isdigit(*parser->token)) {
            accum = accum * 10 + *parser->token - '0';
            continue;
        }

        /* everything else is an error */
        printf("Error at %c.\n", *parser->token);
        return false;
    }

    TfElement *new_number = Array_push(&parser->result);

    if (!new_number) {
        printf("Out of memory.\n");
        return false;
    }

    new_number->type = number;
    new_number->numbervalue = accum * factor;

    STATE_TRANSFER(TfParser_state_start);
}

/*
 * Parses a negative number.
 */
bool TfParser_state_negative_number(TfParser *parser) {
    return TfParser_state_number_impl(parser, -1);
}

/*
 * Parses a number.
 */
bool TfParser_state_number(TfParser *parser) {
    return TfParser_state_number_impl(parser, 1);
}

/*
 * Parses an operator.
 */
bool TfParser_state_op(TfParser *parser) {
    char op = *parser->token;

    for (++parser->token; *parser->token; ++parser->token) {
        /* if space or implicitly, end of parsing, it was an operator */
        if (isspace(*parser->token)) {
            break;
        }

        /* everything else is an error */
        printf("Error at %c.\n", *parser->token);
        return false;
    }

    TfElement *new_op = Array_push(&parser->result);

    if (!new_op) {
        printf("Out of memory.\n");
        return false;
    }

    new_op->type =
        op == '+' ? plus_op :
        op == '*' ? mul_op :
        op == '/' ? div_op : minus_op;

    STATE_TRANSFER(TfParser_state_start);
}

/*
 * Parses the equals sign.
 */
bool TfParser_state_equals(TfParser *parser) {
    for (++parser->token; *parser->token; ++parser->token) {
        /* if space or implicitly, end of parsing, it was equality operator */
        if (isspace(*parser->token)) {
            break;
        }

        /* everything else is an error - for now */
        printf("Error at %c.\n", *parser->token);
        return false;
    }

    TfElement *new_op = Array_push(&parser->result);

    if (!new_op) {
        printf("Out of memory.\n");
        return false;
    }

    new_op->type = eq_op;
    STATE_TRANSFER(TfParser_state_start);
}

/*
 * Parses the "<" sign.
 */
bool TfParser_state_lt(TfParser *parser) {
    for (++parser->token; *parser->token; ++parser->token) {
        /* if space or implicitly, end of parsing, it was less than operator */
        if (isspace(*parser->token)) {
            break;
        }

        if (*parser->token == '=') {
            STATE_TRANSFER(TfParser_state_lte);
            return true;
        }

        /* everything else is an error - for now */
        printf("Error at %c.\n", *parser->token);
        return false;
    }

    TfElement *new_op = Array_push(&parser->result);

    if (!new_op) {
        printf("Out of memory.\n");
        return false;
    }

    new_op->type = lt_op;
    STATE_TRANSFER(TfParser_state_start);
}

/*
 * Parses an "=" found after the "<" sign.
 */
bool TfParser_state_lte(TfParser *parser) {
    for (++parser->token; *parser->token; ++parser->token) {
        /* if space or implicitly, end of parsing, it was less than equal operator */
        if (isspace(*parser->token)) {
            break;
        }

        /* everything else is an error */
        printf("Error at %c.\n", *parser->token);
        return false;
    }

    TfElement *new_op = Array_push(&parser->result);

    if (!new_op) {
        printf("Out of memory.\n");
        return false;
    }

    new_op->type = lte_op;
    STATE_TRANSFER(TfParser_state_start);
}

/*
 * Parses the ">" sign.
 */
bool TfParser_state_gt(TfParser *parser) {
    for (++parser->token; *parser->token; ++parser->token) {
        /* if space or implicitly, end of parsing, it was less than operator */
        if (isspace(*parser->token)) {
            break;
        }

        if (*parser->token == '=') {
            STATE_TRANSFER(TfParser_state_gte);
            return true;
        }

        /* everything else is an error - for now */
        printf("Error at %c.\n", *parser->token);
        return false;
    }

    TfElement *new_op = Array_push(&parser->result);

    if (!new_op) {
        printf("Out of memory.\n");
        return false;
    }

    new_op->type = gt_op;
    STATE_TRANSFER(TfParser_state_start);
}

/*
 * Parses an "=" found after the ">" sign.
 */
bool TfParser_state_gte(TfParser *parser) {
    for (++parser->token; *parser->token; ++parser->token) {
        /* if space or implicitly, end of parsing, it was less than equal operator */
        if (isspace(*parser->token)) {
            break;
        }

        /* everything else is an error */
        printf("Error at %c.\n", *parser->token);
        return false;
    }

    TfElement *new_op = Array_push(&parser->result);

    if (!new_op) {
        printf("Out of memory.\n");
        return false;
    }

    new_op->type = gte_op;
    STATE_TRANSFER(TfParser_state_start);
}
/*
 * Parses a print command or starting with '.'.
 */
bool TfParser_state_print(TfParser *parser) {
    for (++parser->token; *parser->token; ++parser->token) {
        /* if space or implicitly, end of parsing, it was a print */
        if (isspace(*parser->token)) {
            break;
        }

        /* everything else is an error - for now */
        printf("Error at %c.\n", *parser->token);
        return false;
    }

    TfElement *new_print = Array_push(&parser->result);

    if (!new_print) {
        printf("Out of memory.\n");
        return false;
    }

    new_print->type = command_print;
    STATE_TRANSFER(TfParser_state_start);
}

/*
 * Parses a minus op or a negative number.
 */
bool TfParser_state_minus_or_number(TfParser *parser) {
    for (++parser->token; *parser->token; ++parser->token) {
        /* if space, or end of parsing it was a minus sign */
        if (isspace(*parser->token)) {
           break;
        }

        /* if a digit is found, parse as a negative number*/
        if (isdigit(*parser->token)) {
            STATE_TRANSFER(TfParser_state_negative_number);
        }

        /* everything else is an error */
        printf("Error at %c.\n", *parser->token);
        return false;
    }

    TfElement *new_minus = Array_push(&parser->result);

    if (!new_minus) {
        printf("Out of memory.\n");
        return false;
    }

    new_minus->type = minus_op;
    STATE_TRANSFER(TfParser_state_start);
}

/*
 * Parses a word. Appends it to the result array if successful.
 * Returns false on error.
 */
bool TfParser_state_word(TfParser *parser) {
    char *start = parser->token;

    for (++parser->token; *parser->token; ++parser->token) {
        /* if alphanumeric, silently accumulate */
        if (isalnum(*parser->token)) {
            continue;
        }

        /* if space or implicitly, end of parsing, add word to result */
        if (isspace(*parser->token)) {
            break;
        }

        /* everything else is an error */
        printf("Error at %c.\n", *parser->token);
        return false;
    }

    size_t len = parser->token - start;

    TfElement *new_word = Array_push(&parser->result);

    if (!new_word) {
        printf("Out of memory.\n");
        return false;
    }

    new_word->type = word;
    new_word->wordname = String_from_slice(start, len);

    if (!new_word->wordname) {
        printf("Out of memory.\n");
        return false;
    }

    STATE_TRANSFER(TfParser_state_start);
}

bool TfParser_parse(TfParser *parser) {
    while (parser->state_func) {
        if (!parser->state_func(parser)) {
            return false;
        }
    }

    return true;
}

// ===========================================================================

typedef enum {
    NORMAL,
    IF,
    ELSE,
} ScopeType;

typedef struct {
    ScopeType type;
    bool execution_flag;
} TfScope;

TypeInfo tfscope_typeinfo = { .size = sizeof(TfScope), .drop = NULL };

typedef struct {
    Array /* TfElement */ result_stack;
    Array /* TfScope */ scope_stack;
} TfInterpreter;

typedef bool (*WordHandlerFunc)(TfInterpreter *interpreter);

typedef struct {
    bool exec_always;
    WordHandlerFunc func;
} WordHandler;

bool TfInterpreter_init(TfInterpreter *self) {
    if (!Array_init(&self->result_stack, &tfelement_typeinfo)) {
        return false;
    }

    if (!Array_init(&self->scope_stack, &tfscope_typeinfo)) {
        Array_drop(&self->result_stack);
        return false;
    }

    TfScope *scope = Array_push(&self->scope_stack);

    if (!scope) {
        Array_drop(&self->scope_stack);
        Array_drop(&self->result_stack);
        return false;
    }

    scope->type = NORMAL;
    scope->execution_flag = true;

    return true;
}

void TfInterpreter_drop(TfInterpreter *self) {
    Array_drop(&self->result_stack);
    Array_drop(&self->scope_stack);
}

void do_print(TfInterpreter *interpreter) {
    TfElement popped;

    while (Array_pop(&interpreter->result_stack, &popped)) {
        if (popped.type == number) {
            printf("%d", popped.numbervalue);
        }

        if (popped.type == boolean) {
            if (popped.booleanvalue) {
                printf("true");
            } else {
                printf("false");
            }
        }

        if (popped.type == string) {
            printf("%s", popped.stringvalue->c_str);
        }

        TfElement_drop(&popped);
    }
}

Dictionary words;

void run_line(Array *program) {
    TfInterpreter interpreter;

    if (!TfInterpreter_init(&interpreter)) {
        return;
    }

    Array_foreach (program, TfElement, prog_element) {
        if (((TfScope*)Array_last(&interpreter.scope_stack))->execution_flag) {
            TfElement a, b;

            switch (prog_element->type) {
                case number:
                case boolean:
                case character:
                    TfElement *dest = Array_push(&interpreter.result_stack);

                    if (!dest) {
                        printf("Out of memory.\n");
                        goto cleanup;
                    }

                    memcpy(dest, prog_element, sizeof(TfElement));
                    break;
                case string:
                    //TfElement *dest = Array_push(&interpreter.result_stack);

                    //if (!dest) {
                    //    printf("Out of memory.\n");
                    //    goto cleanup;
                    //}

                    //dest->type = prog_element->type;
                    //dest->stringvalue = String_copy(prog_element->stringvalue);

                    break;
                case plus_op:
                case minus_op:
                case mul_op:
                case div_op:

                    // a and b are popped in reverse so operators
                    // can be expressed left to right
                    if (Array_pop(&interpreter.result_stack, &b) && 
                            Array_pop(&interpreter.result_stack, &a)) {

                        if (a.type == number && b.type == number) {
                            int result =
                                prog_element->type == plus_op ?
                                a.numbervalue + b.numbervalue :
                                prog_element->type == minus_op ?
                                a.numbervalue - b.numbervalue :
                                prog_element->type == mul_op ?
                                a.numbervalue * b.numbervalue :
                                a.numbervalue / b.numbervalue;

                            TfElement *dest = Array_push(&interpreter.result_stack);

                            if (!dest) {
                                printf("Out of memory.\n");
                                goto cleanup;
                            }

                            dest->type = number;
                            dest->numbervalue = result;
                        } else {
                            printf("Invalid operands detected.\n");
                            goto cleanup;
                        }
                    } else {
                        printf("Result stack underflow.\n");
                        goto cleanup;
                    }
                    break;
                case eq_op:
                case lt_op:
                case lte_op:
                case gt_op:
                case gte_op:
                    // a and b are popped in reverse so operators
                    // can be expressed left to right
                    if (Array_pop(&interpreter.result_stack, &b) && 
                            Array_pop(&interpreter.result_stack, &a)) {

                        if (a.type == number && b.type == number) {
                            bool result =
                                prog_element->type == eq_op ?
                                a.numbervalue = b.numbervalue :
                                prog_element->type == lt_op ?
                                a.numbervalue < b.numbervalue :
                                prog_element->type == lte_op ?
                                a.numbervalue <= b.numbervalue :
                                prog_element->type == gt_op ?
                                a.numbervalue > b.numbervalue :
                                a.numbervalue >= b.numbervalue;

                            TfElement *dest = Array_push(&interpreter.result_stack);

                            if (!dest) {
                                printf("Out of memory.\n");
                                goto cleanup;
                            }

                            dest->type = boolean;
                            dest->booleanvalue = result;
                        } else {
                            printf("Invalid operands detected.\n");
                            goto cleanup;
                        }
                    } else {
                        printf("Result stack underflow.\n");
                        goto cleanup;
                    }

                    break;
                case word:
                    WordHandler *handler = Dictionary_get(
                        &words,
                        prog_element->wordname->c_str
                    );
                    
                    if (!handler) {
                        printf("Undefined word `%s`.\n", prog_element->wordname->c_str);
                        goto cleanup;
                    }

                    if (!handler->func(&interpreter)) {
                        goto cleanup;
                    }

                    break;
                case command_print:
                    do_print(&interpreter);
                    break;
            }
        } else if(prog_element->type == word) {
            WordHandler *handler = Dictionary_get(
                &words,
                prog_element->wordname->c_str
            );

            if (!handler) {
                printf(
                        "Undefined word `%s`.\n",
                        prog_element->wordname->c_str
                      );
                goto cleanup;
            }

            if (handler->exec_always) {
                if (!handler->func(&interpreter)) {
                    goto cleanup;
                }
            }
        }
    }

cleanup:
    TfInterpreter_drop(&interpreter);
}

bool dup_handler(TfInterpreter *interpreter) {
    TfElement *element = Array_last(&interpreter->result_stack);

    if (!element) {
        printf("Result stack underflow.\n");
        return false;
    }

    TfElement *dup = Array_push(&interpreter->result_stack);

    if (!dup) {
        printf("Out of memory.\n");
        return false;
    }

    memcpy(dup, element, sizeof(TfElement));

    return true;
}

bool if_handler(TfInterpreter *interpreter) {
    TfScope *current_scope = (TfScope*)Array_last(&interpreter->scope_stack);

    // There is always at least one scope
    assert(current_scope != NULL);

    bool is_true = false;

    if (current_scope->execution_flag) {
        TfElement *controlvalue = (TfElement*)Array_last(&interpreter->result_stack);

        if (!controlvalue || !TfElement_is_true(controlvalue, &is_true)) {
            printf("No boolean data on the stack.\n");
            return false;
        }
    }

    // current_scope may become invalid after this point

    TfScope *new_scope = (TfScope*)Array_push(&interpreter->scope_stack);

    if (!new_scope) {
        printf("Out of memory.\n");
        return false;
    }

    new_scope->type = IF;
    new_scope->execution_flag = is_true;

    return true;
}

bool else_handler(TfInterpreter *interpreter) {
    TfScope *current_scope = (TfScope*)Array_last(&interpreter->scope_stack);

    // There is always at least one scope
    assert(current_scope != NULL);

    if (current_scope->type != IF) {
        printf("else without if detected.\n");
        return false;
    }

    current_scope->type = ELSE;
    current_scope->execution_flag = !current_scope->execution_flag;

    return true;
}

bool then_handler(TfInterpreter *interpreter) {
    TfScope current_scope;

    // There is always at least one scope
    assert(Array_pop(&interpreter->scope_stack, &current_scope));

    if (current_scope.type == IF || current_scope.type == ELSE) {
        return true;
    }

    printf("then without if detected.\n");
    return false;
}

bool cr_handler(TfInterpreter *interpreter) {
    printf("\n");
    return true;
}

bool drop_handler(TfInterpreter *interpreter) {
    if (!Array_pop(&interpreter->result_stack, NULL)) {
        printf("Result stack underflow.\n");
        return false;
    }

    return true;
}

TypeInfo wordhandler_typeinfo = { .size = sizeof(WordHandler), .drop = NULL };

int main(int argc, char *argv[]) {
    if (!Dictionary_init(&words, &wordhandler_typeinfo)) {
        fprintf(stderr, "Catastrophic error.\n");
        return -1;
    }

    WordHandler *h = Dictionary_insert(&words, String_init("if"));
        h->exec_always = true;
        h->func = if_handler;
    h = Dictionary_insert(&words, String_init("else"));
        h->exec_always = true;
        h->func = else_handler;
    h = Dictionary_insert(&words, String_init("then"));
        h->exec_always = true;
        h->func = then_handler;
    h = Dictionary_insert(&words, String_init("cr"));
        h->exec_always = false;
        h->func = cr_handler;
    h = Dictionary_insert(&words, String_init("dup"));
        h->exec_always = false;
        h->func = dup_handler;
    h = Dictionary_insert(&words, String_init("drop"));
        h->exec_always = false;
        h->func = drop_handler;

    while (1) {
        char *line = NULL;
        size_t linelen;
        printf("tf> ");
        fflush(stdout);
        getline(&line, &linelen, stdin);

        TfParser line_parser;
        TfParser_init(&line_parser, line);
        TfParser_parse(&line_parser);

        run_line(&line_parser.result);

        TfParser_drop(&line_parser);
        free(line);
    }
}
