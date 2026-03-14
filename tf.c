#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
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

/*
 * Frees a TfElement's owned resources.
 * Drops String and word fields if present.
 */
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
 * Checks whether a TfElement contains a true value.
 * The result is stored in result, while a return
 * value of false signals undefined test.
 * The result is stored in result, while a return
 * value of false signals undefined test.
 */
bool TfElement_is_true(TfElement *self, bool *result) {
    switch (self->type) {
        case number: *result = self->numbervalue; return true;
        case boolean: *result = self->booleanvalue; return true;
        case character: *result = self->charvalue; return true;
        default: return false;
    }
}

TypeInfo tfelement_typeinfo = { .size = sizeof(TfElement), .drop = (DropFunction)TfElement_drop };

// ============================================================================
// PARSER DEFINITION
// ============================================================================

/*
 * Holds the parser status.
 */
typedef struct _TfParser {
    char *token;
    char *end;
#ifndef STATE_MACHINE_AS_JUMP_LABELS
    bool (*state_func)(struct _TfParser *);
#endif
    Array /* TfElement */ result;
    union { // this union holds cross state accumulators
        int number_sign;
        Array string_accumulator;
    };
} TfParser;

/*
 * Build the state machine DSL based one the
 * desired implementation options.
 */
#ifdef STATE_MACHINE_AS_JUMP_LABELS
#   pragma message("Implementing parser as goto labels.")
#   define STATE(s)             s:
#   define STATE_TRANSFER(s)    goto s;
#else
#   pragma message("Implementing parser as dispatcher loop.")
#   define STATE(s)             bool s(TfParser *parser)
#   define STATE_TRANSFER(s)    parser->state_func = s; \
                                return true;
#endif

#ifndef STATE_MACHINE_AS_JUMP_LABELS
bool TfParser_state_start(TfParser *parser);
bool TfParser_state_number(TfParser *parser);
bool TfParser_state_string(TfParser *parser);
bool TfParser_state_string_escape(TfParser *parser);
bool TfParser_state_op(TfParser *parser);
bool TfParser_state_word(TfParser *parser);
bool TfParser_state_minus_or_number(TfParser *parser);
bool TfParser_state_print(TfParser *parser);
bool TfParser_state_equals(TfParser *parser);
bool TfParser_state_lt(TfParser *parser);
bool TfParser_state_lte(TfParser *parser);
bool TfParser_state_gt(TfParser *parser);
bool TfParser_state_gte(TfParser *parser);
#endif

/*
 * Initializes the parser.
 * Parameters:
 * * parser: the parser to initialize.
 * * token: the starting position of a string to parse.
 * Returns true on success, false on failure.
 */
bool TfParser_init(TfParser *parser, char *token, size_t linelen) {
    // Begin before the buffer. It selects a garbage
    // memory location, but safety is guaranteed by the parser loop
    // always fetching the next byte - expecially start state.
    parser->token = token - 1;
    parser->end = token + linelen;
#ifndef STATE_MACHINE_AS_JUMP_LABELS
    parser->state_func = TfParser_state_start;
#endif

    return Array_init(&parser->result, &tfelement_typeinfo);
}

/*
 * Frees a TfParser and its result array.
 */
void TfParser_drop(TfParser *parser) {
    Array_drop(&parser->result);
    /* The accumulator union may hold dynamically allocated arrays.
     * We cannot safely drop them without knowing which field is active.
     * Parser actions explicitly drop these when needed. */
}

// ============================================================================
// PARSER ACTIONS
// ============================================================================

/*
 * Pushes a character in the currently active string accumulator.
 */
bool TfParser_action_string_accumulate(TfParser *parser, char c) {
    char *dest = Array_push(&parser->string_accumulator);

    if (!dest) {
        Array_drop(&parser->string_accumulator);
        return false;
    }

    *dest = c;
    return true;
}

/*
 * Saves the currently accumulated string in the result program.
 */
bool TfParser_action_string_save(TfParser *parser) {
    // End of accumulation action
    String s = String_from_array(&parser->string_accumulator);

    if (!s) {
        // accumulator is consumed on string creation failure
        // and success. No need to drop it.
        return false;
    }

    TfElement *new_string = Array_push(&parser->result);

    if (!new_string) {
        String_drop(s);
        printf("Out of memory.\n");
        return false;
    }

    new_string->type = string;
    new_string->stringvalue = s;

    return true;
}

// ============================================================================
// PARSER IMPLEMENTATION
// ============================================================================

/*
 * Parses the entire input string.
 * Continues in state machine mode until all tokens are parsed.
 */
#ifdef STATE_MACHINE_AS_JUMP_LABELS
bool TfParser_parse(TfParser *parser) { // Open bracket without closing to surround all states
    // Jump to first state explicitly.
    STATE_TRANSFER(TfParser_state_start);
#else
    bool TfParser_parse(TfParser *parser) { // Dispatcher loop implementation
        while (parser->state_func) {
            if (!parser->state_func(parser)) {
                return false;
            }
        }

        return true;
    }
#endif

    /*
     * The start state of the parser.
     * Dispatches to appropriate state based on the first character.
     */
    STATE(TfParser_state_start) {
        for (++parser->token;
                parser->token != parser->end &&
                *parser->token;
                ++parser->token) {
            if (isspace(*parser->token)) {
                continue;
            }
            if (isalpha(*parser->token)) {
                STATE_TRANSFER(TfParser_state_word);
            }
            if (isdigit(*parser->token)) {
                // Action: accumulate positive sign and transfer state
                parser->number_sign = 1;
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
            if (*parser->token == '"') {
                // Action: start accumulator
                if (!Array_init(&parser->string_accumulator, &char_typeinfo)) {
                    return false;
                }

                STATE_TRANSFER(TfParser_state_string);
            }
            if (strchr("+*/", *parser->token)) {
                STATE_TRANSFER(TfParser_state_op);
            }
        }

#ifndef STATE_MACHINE_AS_JUMP_LABELS
        parser->state_func = NULL;
#endif
        return true;
    }

    /*
     * Parses a number.
     */
    STATE(TfParser_state_number) {
        int accum = *parser->token - '0';

        for (++parser->token;
                parser->token != parser->end &&
                *parser->token;
                ++parser->token) {
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
        new_number->numbervalue = accum * parser->number_sign;

        STATE_TRANSFER(TfParser_state_start);
    }

    /*
     * Parses an operator.
     */
    STATE(TfParser_state_op) {
        char op = *parser->token;

        for (++parser->token;
                parser->token != parser->end &&
                *parser->token;
                ++parser->token) {
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
     * Parses a string.
     */
    STATE(TfParser_state_string) {
        for (++parser->token;
                parser->token != parser->end &&
                *parser->token;
                ++parser->token) {
            char tok = *parser->token;

            if (tok == '\\') {
                STATE_TRANSFER(TfParser_state_string_escape);
            }

            if (tok == '"') {
                if (!TfParser_action_string_save(parser)) {
                    return false;
                }

                STATE_TRANSFER(TfParser_state_start);
            }

            if (!TfParser_action_string_accumulate(parser, tok)) {
                return false;
            }
        }

        printf("Unexpected end of line.\n");
        Array_drop(&parser->string_accumulator);
        return false;
    }

    /*
     * Parses a string escape sequence found in a string
     */
    STATE(TfParser_state_string_escape) {
        for (++parser->token;
                parser->token != parser->end &&
                *parser->token;
                ++parser->token) {
            char tok = *parser->token;
            char out_char;

            switch (tok) {
                case '\\':
                case '\"':
                    out_char = tok;
                    break;
                default:
                    printf("Unexpected token %c.\n", tok);
                    Array_drop(&parser->string_accumulator);
                    return false;
            }

            if (!TfParser_action_string_accumulate(parser, out_char)) {
                return false;
            }
             
            STATE_TRANSFER(TfParser_state_string);
        }

        printf("Unexpected end of line.\n");
        Array_drop(&parser->string_accumulator);
        return false;
    }

    /*
     * Parses the equals sign.
     */
    STATE(TfParser_state_equals) {
        for (++parser->token;
                parser->token != parser->end &&
                *parser->token;
                ++parser->token) {
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
    STATE(TfParser_state_lt) {
        for (++parser->token;
                parser->token != parser->end &&
                *parser->token;
                ++parser->token) {
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
    STATE(TfParser_state_lte) {
        for (++parser->token;
                parser->token != parser->end &&
                *parser->token;
                ++parser->token) {
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
    STATE(TfParser_state_gt) {
        for (++parser->token;
                parser->token != parser->end &&
                *parser->token;
                ++parser->token) {
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
    STATE(TfParser_state_gte) {
        for (++parser->token;
                parser->token != parser->end &&
                *parser->token;
                ++parser->token) {
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
    STATE(TfParser_state_print) {
        for (++parser->token;
                parser->token != parser->end &&
                *parser->token;
                ++parser->token) {
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
    STATE(TfParser_state_minus_or_number) {
        for (++parser->token;
                parser->token != parser->end &&
                *parser->token;
                ++parser->token) {
            /* if space, or end of parsing it was a minus sign */
            if (isspace(*parser->token)) {
                break;
            }

            /* if a digit is found, parse as a negative number*/
            if (isdigit(*parser->token)) {
                // Accumulate negative sign and transfer state
                parser->number_sign = -1;
                STATE_TRANSFER(TfParser_state_number);
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
    STATE(TfParser_state_word) {
        char *start = parser->token;

        for (++parser->token;
                parser->token != parser->end &&
                *parser->token;
                ++parser->token) {
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

        String s = String_from_slice(start, len);

        if (!s) {
            Array_pop(&parser->result, NULL);
            printf("Out of memory.\n");
            return false;
        }

        new_word->type = word;
        new_word->wordname = s;

        STATE_TRANSFER(TfParser_state_start);
    }

#ifdef STATE_MACHINE_AS_JUMP_LABELS
} // End of state machine
#endif
// ===========================================================================

/*
 * Represents the type of a scope (normal, or a control structure).
 * Used for resolving labels in Forth.
 */
typedef enum {
    NORMAL,
    IF,
    ELSE,
    DO
} ScopeType;

typedef struct {
    ScopeType type;
    bool execution_flag;

    union {
        struct {
            size_t do_label;
            int index;
            int limit;
        } loop_control;
    };
} TfScope;

TypeInfo tfscope_typeinfo = { .size = sizeof(TfScope), .drop = NULL };

typedef struct {
    Array /* TfElement */ result_stack;
    Array /* TfScope */ scope_stack;
    size_t program_counter;
} TfInterpreter;

typedef bool (*WordHandlerFunc)(TfInterpreter *interpreter);

typedef struct {
    bool exec_always;
    WordHandlerFunc func;
} WordHandler;

/*
 * Initializes a TfInterpreter.
 * Sets up the result stack, scope stack, and initial scope.
 * Returns true on success, false on failure.
 */
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
    self->program_counter = 0;

    return true;
}

/*
 * Frees a TfInterpreter and its stacks.
 */
void TfInterpreter_drop(TfInterpreter *self) {
    Array_drop(&self->result_stack);
    Array_drop(&self->scope_stack);
}

/*
 * Pops all elements from the result stack and prints them.
 */
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
        printf(" ");
    }
}

Dictionary words;

/*
 * Executes a parsed program.
 * Initializes an interpreter and runs each element in the program.
 * Handles numbers, operators, and Forth words.
 */
void run_line(Array *program) {
    TfInterpreter interpreter;

    if (!TfInterpreter_init(&interpreter)) {
        return;
    }

    while (true) {
        TfElement *prog_element = Array_at(program, interpreter.program_counter);

        if (!prog_element) {
            break;
        }

        ++interpreter.program_counter;

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
                    TfElement *dest_s = Array_push(&interpreter.result_stack);

                    if (!dest_s) {
                        printf("Out of memory.\n");
                        goto cleanup;
                    }

                    String copy = String_copy(prog_element->stringvalue);

                    if (!copy) {
                        goto cleanup;
                    }

                    dest_s->type = prog_element->type;
                    dest_s->stringvalue = copy;

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
                                (a.numbervalue = b.numbervalue) :
                                prog_element->type == lt_op ?
                                (a.numbervalue < b.numbervalue) :
                                prog_element->type == lte_op ?
                                (a.numbervalue <= b.numbervalue) :
                                prog_element->type == gt_op ?
                                (a.numbervalue > b.numbervalue) :
                                (a.numbervalue >= b.numbervalue);

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

/*
 * Duplicates the top element on the result stack.
 * Returns false if the stack is empty.
 */
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

/*
 * Pushes a new scope on the scope stack for conditional execution.
 * If the top of the stack is true, the new scope will execute.
 * Otherwise, it will not execute.
 * Returns false if no boolean value is on the stack.
 */
bool if_handler(TfInterpreter *interpreter) {
    TfScope *current_scope = Array_last(&interpreter->scope_stack);

    if (!current_scope) {
        printf("Scope stack corrupted.\n");
        return false;
    }

    bool is_true = false;

    if (current_scope->execution_flag) {
        TfElement controlvalue;
        bool pop = Array_pop(&interpreter->result_stack, &controlvalue);

        if (!pop || !TfElement_is_true(&controlvalue, &is_true)) {
            TfElement_drop(&controlvalue);
            printf("No boolean data on the stack.\n");
            return false;
        }

        TfElement_drop(&controlvalue);
    }

    // Don't use current_scope variable after this point
    // since the array may relocate.

    TfScope *new_scope = (TfScope*)Array_push(&interpreter->scope_stack);

    if (!new_scope) {
        printf("Out of memory.\n");
        return false;
    }

    new_scope->type = IF;
    new_scope->execution_flag = is_true;

    return true;
}

/*
 * Marks the current if-block as else.
 * Toggles the execution flag of the current scope.
 * Returns false if there is no if-block to toggle.
 */
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

/*
 * Pops the scope stack and checks that it was an if or else block.
 * Returns true if valid, false if there was a mismatch.
 */
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

/*
 * Prints a newline character.
 */
bool cr_handler(TfInterpreter *interpreter) {
    printf("\n");
    return true;
}

/*
 * Pops the top element from the result stack.
 * Drops the element if dest is NULL.
 * Returns false if the stack is empty.
 */
bool drop_handler(TfInterpreter *interpreter) {
    if (!Array_pop(&interpreter->result_stack, NULL)) {
        printf("Result stack underflow.\n");
        return false;
    }

    return true;
}

/*
 * When execution is enabled, checks for the presence of 2 numeric elements on
 * the result stack.
 * If everything succeeds places a DO scope on the execution stack.
 */
bool do_handler(TfInterpreter *interpreter) {
    TfScope *current_scope = Array_last(&interpreter->scope_stack);

    if (!current_scope) {
        printf("Scope stack corrupted.\n");
        return false;
    }

    bool execution_flag = current_scope->execution_flag;
    int i = 0;
    int l = 0;

    if (execution_flag) {
        // Fetch the control variables.
        TfElement index;
        TfElement limit;

        if (
            !Array_pop(&interpreter->result_stack, &index) ||
            !Array_pop(&interpreter->result_stack, &limit)
        ) {
            printf("Result stack underflow.\n");
            return false;
        }

        // TODO: expand the numeric types allowed
        if (index.type != limit.type || !(index.type == number || index.type == character)) {
            printf("No numeric control variables on the stack.\n");
            return false;
        }

        i = index.type == number ? index.numbervalue : index.charvalue;
        l = limit.type == number ? limit.numbervalue : limit.charvalue;
    }

    // Don't use current_scope variable after this point
    // since the array may relocate.

    TfScope *new_scope = (TfScope*)Array_push(&interpreter->scope_stack);

    if (!new_scope) {
        printf("Out of memory.\n");
        return false;
    }

    new_scope->type = DO;
    new_scope->execution_flag = execution_flag;
    new_scope->loop_control.do_label = interpreter->program_counter;
    new_scope->loop_control.index = i;
    new_scope->loop_control.limit = l;

    return true;
}

/*
 * Checks if the current scope is a DO.
 * When execution is enabled, updates the loop control variable.
 */
bool loop_handler(TfInterpreter *interpreter) {
    TfScope *current_scope = Array_last(&interpreter->scope_stack);

    if (!current_scope) {
        printf("Scope stack corrupted.\n");
        return false;
    }

    if (current_scope->type != DO) {
        printf("LOOP without DO detected.\n");
        return false;
    }

    // If execution is off, just pop to NULL, we are fine
    // with lexer validation.
    if (!current_scope->execution_flag) {
        Array_pop(&interpreter->scope_stack, NULL);
        return true;
    }

    if (++current_scope->loop_control.index >= current_scope->loop_control.limit) {
        Array_pop(&interpreter->scope_stack, NULL);
        return true;
    }

    interpreter->program_counter = current_scope->loop_control.do_label;
    return true;
}

bool leave_handler(TfInterpreter *interpreter) {
    printf("not implemented\n"); return false;
}
bool i_handler(TfInterpreter *interpreter) {
    printf("not implemented\n"); return false;
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
    h = Dictionary_insert(&words, String_init("do"));
    h->exec_always = true;
    h->func = do_handler;
    h = Dictionary_insert(&words, String_init("loop"));
    h->exec_always = true;
    h->func = loop_handler;
    h = Dictionary_insert(&words, String_init("leave"));
    h->exec_always = false;
    h->func = leave_handler;
    h = Dictionary_insert(&words, String_init("i"));
    h->exec_always = false;
    h->func = i_handler;

    bool istty = isatty(fileno(stdin));

    while (1) {
        char *line = NULL;
        size_t linelen, linesize;

        if (istty) {
            printf("tf> ");
        }

        fflush(stdout);

        if ((linelen = getline(&line, &linesize, stdin) - 1) < 0) {
            break;
        }

        TfParser line_parser;
        TfParser_init(&line_parser, line, linelen);
        TfParser_parse(&line_parser);

        run_line(&line_parser.result);

        TfParser_drop(&line_parser);
        free(line);
    }
}
