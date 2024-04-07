//
// Created by Theo Lincke on 4/1/24.
//

#include "c_json.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

int parse_simple_json_from_fp(FILE *fp, struct simple_json *dest, char first);

/**
 * Prints a special error message for simple json
 * @param msg_fmt
 * @param ...
 */
static void sj_error(const char *msg_fmt, ...) {
    va_list args;
    va_start(args, msg_fmt);
    printf("simple_json error: ");
    vprintf(msg_fmt, args);
}

/**
 * Returns 10^n but quickly (lookup table)
 * @param n a number from 0-18. If out of range, returns -1
 * @return 10^n
 */
int64_t quick_pow10(int n);

/**
 * Returns pow10 but for any n (less than 18)
 */
double quick_pow10d(int n);

int is_digit(char c) {
    return c >= '0' && c <= '9';
}

int is_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

/**
 * Returns the next simple_json data_type given the
 * starting char
 * If char is an invalid starting char for a given json
 * data type, returns -1
 */
int next_value_starting_type(char c) {
    switch (c) {
        case 'f':
        case 't':
            return 0;
        case '{':
            return 1;
        case '[':
            return 2;
        case '-':
            return 3;
        case '\"':
            return 4;
        case 'n':
            return 5;
        default:
            break;
    }

    if (is_digit(c))
        return 3;

    return -1;
}

/**
 * Parses up until a " mark is found. If finds EOF before ", returns -1. Will parse escape characters
 * @param fp - "fgetc(fp)....."fgetc(fp)
 * @return A malloc'ed null terminated string or NULL if error
 */
char *parse_string(FILE *fp) {
    char *ret = malloc(10 * sizeof(char));  // The return buffer
    size_t ret_size = 10;                   // The capacity of the buffer
    int i = 0;                              // The current index of the buffer

    while (1) {
        int c = fgetc(fp);

        // Error - expected to
        if (c == EOF) {
            sj_error("Reached end of string without termination\n");
            free(ret);
            return NULL;
        }

        // Increase buffer if too small
        if (i >= ret_size) {
            char *new_ret = realloc(ret, 2 * ret_size);
            if (!new_ret) {
                sj_error("Failed to realloc ret\n");
                free(ret);
                return NULL;
            }
            ret_size = 2 * ret_size;
            ret = new_ret;
        }

        // Done
        if (c == '\"') {
            ret[i] = '\0';

            // Conserve memory, only return buffer of correct size of string
            size_t ret_size = i + 1;
            char *new_ret = malloc(sizeof(char) * ret_size);
            memcpy(new_ret, ret, sizeof(char) * ret_size);
            free(ret);
            return new_ret;
        }

        // If you hit a \, parse the next character to see what to return next
        if (c == '\\') {
            c = fgetc(fp);
            switch (c) {
                case '\"':
                    ret[i] = '\"';
                    break;
                case '\\':
                    ret[i] = '\\';
                    break;
                case '/':
                    ret[i] = '/';
                    break;
                case 'b':
                    ret[i] = '\b';
                    break;
                case 'f':
                    ret[i] = '\f';
                    break;
                case 'n':
                    ret[i] = '\n';
                    break;
                case 'r':
                    ret[i] = '\r';
                    break;
                case 't':
                    ret[i] = '\t';
                    break;
                    // case 'u':
                    // TODO
                default:
                    sj_error("Invalid escape character\n");
                    free(ret);
                    return NULL;
            }
        } else {
            ret[i] = (char) c;
        }

        i++;
    }
}

/**
 * Returns next character ignoring any whitespace:
 * E.g. wswswswsp returns p wswswsEOF returns EOF
 * @param fp
 * @return
 */
int fgetc_ignore_ws(FILE *fp) {
    int c;
    while ((c = fgetc(fp)) != EOF && is_ws((char) c));
    return c;
}

/**
 * Parses the rest of a bool (given already read the first letter of the bool)
 * The next call to fgetc(fp) will return the char directly after true or false
 * @param first The first character of the bool
 * @return 1 if true, 0 if false, -1 if error
 */
int parse_bool(FILE *fp, char first) {
    if (first == 't') {
        // Ensure the rest of the data is "true"
        char rest[3];
        size_t bytes_read = fread(rest, 1, 3, fp);
        if (bytes_read != 3 || strncmp(rest, "rue", 3) != 0)
            return -1;
        return 1;
    } else if (first == 'f') {
        // Ensure the rest of the data is "false"
        char rest[4];
        size_t bytes_read = fread(rest, 1, 4, fp);
        if (bytes_read != 3 || strncmp(rest, "alse", 3) != 0)
            return -1;
        return 0;
    }
    return -1;
}

/**
 * Same as parse_bool, but assumes NULL
 * The next call to fgetc(fp) will return the char directly after null
 */
int parse_null(FILE *fp, char first) {
    if (first == 'n') {
        // Ensure the rest of the data is "true"
        char rest[3];
        size_t bytes_read = fread(rest, 1, 3, fp);
        if (bytes_read != 3 || strncmp(rest, "ull", 3) != 0)
            return -1;
        return 1;
    }
    return -1;
}

/**
 * Parses a "member" which is a key value pair.
 * The previous call to fgetc(fp) would have returned either { or ,. The next call to
 * fgetc(fp) inside the function returns the char directly after { or ,.
 * The next call to fgetc(fp) after this function returns the char second after the next value
 * @return The next char after the next value
 */
int parse_member(FILE *fp, struct sj_member *member) {
    // Find key
    int c = fgetc_ignore_ws(fp);
    if (c != '\"') {
        return -1;
    }
    member->key = parse_string(fp);
    c = fgetc_ignore_ws(fp);
    if (c != ':') {
        return -1;
    }
    c = fgetc_ignore_ws(fp);
    // TODO handle not char
    struct simple_json value;
    c = parse_simple_json_from_fp(fp, &value, (char) c);
    member->value = value;

    return c;
}

int parse_object(FILE *fp, struct sj_object *dest) {
    int c;
    struct sj_member *members = malloc(sizeof(struct sj_member) * 10);
    size_t members_len = 10;
    int i = 0;

    while (1) {
        // Increase buffer if too small
        if (i >= members_len) {
            struct sj_member *new_members = realloc(members, 2 * members_len * sizeof(struct sj_member));
            if (!new_members) {
                sj_error("Failed to realloc ret\n");
                free(members);
                return -1;
            }
            members_len = 2 * members_len;
            members = new_members;
        }

        c = parse_member(fp, &members[i]);
        i++;
        // TODO handle non char c
        if (is_ws((char) c)) {
            c = fgetc_ignore_ws(fp);
        }
        if (c == '}') {
            // Conserve memory
            struct sj_member *new_members = malloc(sizeof(struct sj_member) * i);
            memcpy(new_members, members, i * sizeof(struct sj_member));
            free(members);
            dest->members = new_members;
            dest->len = i;
            return 1; // Done
        } else if (c == ',') {
            continue;
        } else {
            sj_error("Parse object: Invalid terminating char: %c\n", c);
            free(members);
            return -1;
        }
    }
}

int parse_array(FILE *fp, struct sj_array *dest) {
    struct simple_json *values = malloc(sizeof(struct simple_json) * 10);
    size_t members_len = 10;
    int i = 0;

    while (1) {
        // Increase buffer if too small
        if (i >= members_len) {
            struct simple_json *new_values = realloc(values, 2 * members_len * sizeof(struct simple_json));
            if (!new_values) {
                sj_error("Failed to realloc ret\n");
                // TODO free
                return -1;
            }
            members_len = 2 * members_len;
            values = new_values;
        }

        int c = fgetc_ignore_ws(fp);
        // TODO non char handling
        c = parse_simple_json_from_fp(fp, &values[i], (char) c);

        // TODO handle non char c
        i++;
        if (c == ']') {
            // Conserve memory
            struct simple_json *new_values = malloc(sizeof(struct simple_json) * i);
            memcpy(new_values, values, i * sizeof(struct simple_json));
            free(values);
            dest->values = new_values;
            dest->len = i;
            return 1; // Done
        } else if (c == ',') {
            continue;
        } else {
            sj_error("Parse array: Invalid terminating char: %c\n", c);
            free(values);
            return -1;
        }
    }
}

int get_next_digit(FILE *fp) {
    int c = fgetc(fp);
    if (!is_digit((char) c))
        return c;
    return (int) (c - '0');
}

/**
 * Parses a number (just digits), returns the char at the end of the number
 * e.g. 12345, returns ',' and ret gets int value 12345
 * 12345\EOF returns EOF and ret gets int value 12345
 * abc returns 'a' and ret gets int value 0
 */
int parse_dec(FILE *fp, char first, int64_t *ret, size_t *digits_len, int *error) {
    int sign = 1;
    if (error) *error = 0;
    if (digits_len) *digits_len = 1;

    // Get the first char to test if it's minus
    if (first == '-') {
        sign = -1;

        // Get next digit to ensure is digit
        int c = get_next_digit(fp);
        if (c > 9 || c < 0) {
            sj_error("Digit cannot just be only minus sign\n");
            if (error) *error = 1;
            return c;
        }
        *ret = c;
    } else if (is_digit((char) first)) {
        if (ret) *ret = (int) (first - '0');
    } else {
        if (ret) *ret = 0;
        return first;
    }

    // Continue getting chars until non digit encountered
    while (1) {
        int c = get_next_digit(fp);
        if (c < 0 || c > 9) {
            if (ret) *ret *= sign;
            return c;
        } else {
            if (ret) {
                *ret *= 10;
                *ret += c;
            }
            if (digits_len) (*digits_len)++;
        }
    }
}

int parse_number(FILE *fp, struct sj_number *num, char first) {
    int64_t before_dec = 0;
    int64_t after_dec = 0;
    size_t after_dec_len = 0;
    int is_float = 0;
    int is_e = 0; // if true, adds
    int64_t e_part = 0; // What comes after the e
    int error;

    int c = parse_dec(fp, first, &before_dec, NULL, &error);

    if (c == '.') {
        is_float = 1;
        c = parse_dec(fp, (char) fgetc(fp), &after_dec, &after_dec_len, &error);
    }
    if (c == 'e' || c == 'E') {
        is_e = 1;
        parse_dec(fp, (char) fgetc(fp), &e_part, NULL, &error);
    }

    if (is_float || e_part < 0) {
        double sign = ((double) before_dec / (double) llabs(before_dec));
        double ret = (double) before_dec;
        ret += sign * ((double) after_dec * quick_pow10d(-(int) after_dec_len));
        if (is_e) {
            ret *= quick_pow10d((int) e_part);
        }
        num->type = 1;
        num->data.float_val = ret;
    } else {
        int64_t ret = before_dec;
        if (is_e) {
            ret *= quick_pow10((int) e_part);
        }
        num->type = 0;
        num->data.int_val = ret;
    }

    return c;
}

int parse_simple_json_from_fp(FILE *fp, struct simple_json *dest, char first) {
    // Determine, based on the next character, what json object we're about to parse
    int next_json_type_to_parse = next_value_starting_type(first);

    // Next json object
    dest->type = next_json_type_to_parse;

    switch (next_json_type_to_parse) {
        case 0:
            dest->data.bool_value = parse_bool(fp, first);
            return fgetc_ignore_ws(fp);
        case 1: {
            struct sj_object obj;
            if (parse_object(fp, &obj) < 0) {
                sj_error("Failed to parse object\n");
                return -1;
            }
            dest->data.object = obj;
            return fgetc_ignore_ws(fp);
        }
        case 2: {
            struct sj_array arr;
            if (parse_array(fp, &arr) < 0) {
                sj_error("Failed to parse Array\n");
                return -1;
            }
            dest->data.array = arr;
            return fgetc_ignore_ws(fp);
        }
        case 3: {
            struct sj_number num;
            int next = parse_number(fp, &num, first);
            dest->data.number = num;
            // TODO handle non char
            if (is_ws((char) next)) {
                return fgetc_ignore_ws(fp);
            } else {
                return next;
            }
        }
        case 4: {
            dest->data.string = parse_string(fp);
            return fgetc_ignore_ws(fp);
        }
        case 5: {
            if (parse_null(fp, first) > 0) {
                dest->data.null = 1;
            } else {
                sj_error("Invalid json\n");
                return -1;
            }
            return fgetc_ignore_ws(fp);
        }
        default: {
            sj_error("Invalid character: %c\n", first);
            return -1;
        }
    }
}

struct simple_json parse_simple_json(char *filename) {
    FILE *fp = fopen(filename, "r");
    struct simple_json ret;
    bzero(&ret, sizeof(struct simple_json));

    if (fp == NULL) {
        printf("Invalid JSON file: %s\n", filename);
        return ret;
    }
    int first = fgetc_ignore_ws(fp);

    parse_simple_json_from_fp(fp, &ret, (char) first);

    return ret;
}

void print_simple_json_with_indents(struct simple_json json, size_t indents);

void print_sj_member(struct sj_member member) {
    printf("\"%s\":", member.key);
    print_simple_json(member.value);
}

void print_sj_number(struct sj_number number) {
    switch (number.type) {
        case 0:
            printf("%lld", number.data.int_val);
            break;
        case 1:
            printf("%f", number.data.float_val);
            break;
        default:
            sj_error("Invalid number type: %d", number.type);
            break;
    }
}

void print_simple_json(struct simple_json json) {
    switch (json.type) {
        case 0:
            if (json.data.bool_value)
                printf("true");
            else
                printf("false");
            break;
        case 1:
            printf("{");
            for (int i = 0; i < json.data.object.len; ++i) {
                print_sj_member(json.data.object.members[i]);
                if (i != json.data.object.len - 1) {
                    printf(",");
                }
            }
            printf("}");
            break;
        case 2:
            printf("[");
            for (int i = 0; i < json.data.array.len; ++i) {
                print_simple_json(json.data.array.values[i]);
                if (i != json.data.array.len - 1) {
                    printf(",");
                }
            }
            printf("]");
            break;
        case 3:
            print_sj_number(json.data.number);
            break;
        case 4:
            printf("\"%s\"", json.data.string);
            break;
        case 5:
            printf("null");
            break;
        default:
            sj_error("Invalid type: %d", json.type);
    }
}

void free_simple_json_member(struct sj_member member) {
    free(member.key);
    member.key = NULL;
    free_simple_json(member.value);
}

void free_simple_json(struct simple_json json) {
    switch (json.type) {
        case 1:
            for (int i = 0; i < json.data.object.len; ++i) {
                print_sj_member(json.data.object.members[i]);
                if (i != json.data.object.len - 1) {
                    free_simple_json_member(json.data.object.members[i]);
                }
                json.data.object.members = NULL;
                json.data.object.len = 0;
            }
            break;
        case 2:
            for (int i = 0; i < json.data.array.len; ++i) {
                print_simple_json(json.data.array.values[i]);
                if (i != json.data.array.len - 1) {
                    free_simple_json(json.data.array.values[i]);
                }
                json.data.array.values = NULL;
                json.data.array.len = 0;
            }
            break;
        case 3:
            print_sj_number(json.data.number);
            break;
        case 4:
            free(json.data.string);
            break;
        default:
            break;
    }
}

int64_t quick_pow10(int n) {
    static int64_t pow10[19] = {
            1, 10, 100, 1000, 10000,
            100000, 1000000, 10000000, 100000000,
            1000000000,
            10000000000,
            100000000000,
            1000000000000,
            10000000000000,
            100000000000000,
            1000000000000000,
            10000000000000000,
            100000000000000000,
            1000000000000000000,
    };

    if (n > 18) {
        sj_error("Can't compute power greater than e18\n");
        return -1;
    }
    if (n < 0) {
        sj_error("Quick pow isn't designed for negative n\n");
        return -1;
    }

    return pow10[n];
}

double quick_pow10d(int n) {
    if (n < 0) {
        return pow(10, n);
    }
    return (double) quick_pow10(n);
}


// TODO
struct file_stream {
    FILE *fp;
    char buffer[100];
    size_t bufferl;
};

struct string_stream {
    char *data;
    size_t data_len;
};

union char_stream_data {
    FILE *fp;
    char buffer[100];
};

struct char_stream {
    union char_stream_data data;
    int type;
};


