//
// Created by Theo Lincke on 4/1/24.
//

#include "c_json.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

static void sj_error(const char *msg_fmt, ...) {
    va_list args;
    va_start(args, msg_fmt);
    printf("c_dynamic_string error: ");
    vprintf(msg_fmt, args);
}

enum sj_state {
    TEMP
};

int is_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

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
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            return 3;
        case '\"':
            return 4;
        case 'n':
            return 5;
        default:
            return -1;
    }
}

int fgetc_ignore_ws(FILE *fp) {
    int c;
    while ((c = fgetc(fp)) != EOF && is_ws((char) c));
    return c;
}

int parse_bool(FILE *fp, char first) {
    if (first == 't') {
        // Ensure the full data is "true"
        char rest[3];
        size_t bytes_read = fread(rest, 1, 3, fp);
        if (bytes_read != 3 || strncmp(rest, "rue", 3) != 0)
            return -1;
        return 1;
    } else if (first == 'f') {
        // Ensure the full data is "true"
        char rest[4];
        size_t bytes_read = fread(rest, 1, 4, fp);
        if (bytes_read != 3 || strncmp(rest, "alse", 3) != 0)
            return -1;
        return 0;
    }
    return -1;
}

int parse_object(FILE *fp) {
    int c;
    while ((c = fgetc(fp)) != EOF && c != '}');
    if (c == EOF)
        return 0;
    return 1;
}

int parse_array(FILE *fp) {
    int c;
    while ((c = fgetc(fp)) != EOF && c != ']');
    if (c == EOF)
        return 0;
    return 1;
}

int is_digit(char c) {
    return c >= '0' && c <= '9';
}

int is_decimal(char c) {
    return c == '.';
}

int parse_number(FILE *fp, struct sj_number *num, char first) {
    num->type = 0;
    num->data.int_val = 0;
    int ret = (int) (first - '0');
    int next = 0;
    while ((next = fgetc(fp)) != EOF) {
        if (is_digit((char) next)) {
            ret *= 10;
            ret += (next - '0');
        }
        if (is_decimal((char) next)) {
            num->type = 1;
            num->data.float_val = (float) num->data.int_val;
        }
    }

    if(num->type == 1) {
        while ((next = fgetc(fp)) != EOF) {
            if (is_digit((char) next)) {
                ret *= 10;
                ret += (next - '0');
            }
            if (is_decimal((char) next)) {
                num->type = 1;
                num->data.float_val = (float) num->data.int_val;
            }
        }
    }
    for (int i = 0; str[i] != '\0'; ++i)
        res = res * 10 + str[i] - '0';
}

int parse_simple_json_from_fp(FILE *fp) {
    int c = fgetc_ignore_ws(fp);
    if (c < 0)
        return c;

    printf("%c\n", c);
    switch (next_value_starting_type((char) c)) {
        case 0: {
            int b = parse_bool(fp, (char) c);
            if (b == 1) {
                printf("Parsed bool: true\n");
            } else if (b == 0) {
                printf("Parsed bool: false\n");
            } else {
                printf("Parsed bool: error\n");
            }
            break;
        }
        case 1: {
            if (parse_object(fp)) {
                printf("Parsed object\n");
            } else {
                printf("Failed to parse object\n");
            }
        }
            break;
        case 2: {
            if (parse_array(fp)) {
                printf("Parsed array\n");
            } else {
                printf("Failed to parse array\n");
            }
        }
        case 3: {
            struct sj_number num;
            if (parse_number(fp, &num)) {
                if (num.type == 0) {
                    printf("Parsed number: %f\n", num.data.float_val);
                } else {
                    printf("Parsed number: %f\n", num.data.int_val);
                }
            } else {
                printf("Failed to parse number\n");
            }
        }
        case 4:
        case 5:
            return c;
    }

    return c;
}

struct simple_json *parse_simple_json(char *filename) {
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        printf("Invalid JSON file: %s\n", filename);
        return NULL;
    }
    struct simple_json *ret = malloc(sizeof(struct simple_json));

    int c;

    // Read the file character by character
    while (1) {
        c = parse_simple_json_from_fp(fp);
        if (c == EOF) {
            break;
        }
    }
    return NULL;
}

