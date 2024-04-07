//
// Created by Theo Lincke on 4/1/24.
//

#ifndef C_JSON_C_JSON_H
#define C_JSON_C_JSON_H

#include <stdlib.h>
#include <stdio.h>

/**
 * JSON_TEXT -> *ws value *ws
 * ws -> Space | Horizontal Tab | Line feed | New Line | Carriage Return
 * value -> false | null | true | object | array | number | string
 * object -> begin-object member *(value-separator member) end-object
 * member -> string name-separator value
 * array -> begin-array value *(value-separator value) end-array
 *
 * number -> [ minus ] int [ frac ] [ exp ]
 * minus -> -
 * int -> zero | ( digit1-9 *DIGIT )
 * digit1-9 -> 1-9
 * zero -> 0
 * frac -> decimal-point 1*DIGIT
 * decimal-point -> .
 * exp -> e [ minus | plus ] 1*DIGIT
 * e -> e | E
 * plus -> +
 *
 * string -> " *char "
 * char = unescaped | escape ( " | \ | / | b | f | n | r | t | hexdig )
 * hexdig -> uXXXX
 * escape -> \
 * unescaped -> %x20-21 / %x23-5B / %x5D-10FFFF
 */

// List of members
struct sj_object {
    struct sj_member *members;
    size_t len;
};

// List of simple_jsons
struct sj_array {
    struct simple_json *values;
    size_t len;
};


// Number data type
union sj_number_dt {
    int64_t int_val;
    double float_val;
};

// Number
struct sj_number {
    union sj_number_dt data;
    int type;
};

union sj_value_dt {
    int bool_value;
    struct sj_object object;
    struct sj_array array;
    struct sj_number number;
    char *string;
    int null;
};

// Simple Json Root
struct simple_json {
    union sj_value_dt data;
    int type;
};

// Key : simple_json
struct sj_member {
    char *key;
    struct simple_json value;
};

// Value data type

struct simple_json parse_simple_json(char *filename);

void free_simple_json(struct simple_json json);

void print_simple_json(struct simple_json json);

#endif //C_JSON_C_JSON_H
