//
// Created by Theo Lincke on 4/1/24.
//

#ifndef C_JSON_C_JSON_H
#define C_JSON_C_JSON_H

#include <stdlib.h>
#include <stdio.h>

enum json_type {
  BOOL = 0,
  OBJECT = 1,
  ARRAY = 2,
  NUMBER = 3,
  STRING = 4,
  JSON_NULL = 5,
  UNKNOWN = 6,
};

// List of members
struct json_object {
  struct json_member *members;
  size_t len;
};

// List of jsons
struct json_array {
  struct json *values;
  size_t len;
};

// Number data type
union json_number_dt {
  int64_t int_val;
  double float_val;
};

enum number_type {
  DEC,
  FLOAT
};

// Number
struct json_number {
  union json_number_dt data;
  enum number_type type;
};

union json_value_dt {
  int bool_value;
  struct json_object object;
  struct json_array array;
  struct json_number number;
  char *string;
  int null;
};

// Simple Json Root
struct json {
  union json_value_dt data;
  enum json_type type;
};

// Key : json
struct json_member {
  char *key;
  struct json value;
};

int fparse_json(const char *filename, struct json* dest);

int str_parse_json(const char *data, struct json* dest);

void free_json(struct json json);

void print_json(struct json json);

#endif //C_JSON_C_JSON_H
