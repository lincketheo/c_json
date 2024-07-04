//
// Created by Theo Lincke on 4/1/24.
//

#include "c_json.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <assert.h>

#define BOLD_RED     "\033[1;31m"
#define RESET   "\033[0m"

#define unwrap(expr, fail_expr) \
({                            \
  if ((expr) == NULL)         \
    (fail_expr);              \
  expr;                       \
})

#define printf_err(fmt, ...) \
fprintf(stderr, BOLD_RED "json error [%s]: " RESET fmt, __func__, ##__VA_ARGS__)

#define malloc_err(arg, name, expr) ({\
                                     unwrap(malloc(arg), ({\
                                     printf_err("Failed to allocate memory for %s\n", name); \
                                     expr;\
                                     })); \
})

#define realloc_err(arg, len, expr) ({\
  void* ret = unwrap(realloc(arg, len), ({\
    printf_err("Failed to reallocate memory\n"); \
    expr;\
  }));\
  ret;\
})


int parse_json_from_fp(FILE *fp, struct json *dest, char first);

// Abstract string and file next char result
//  - if(!ok) handle error
//  - if(ok) Then check eof
//  - Finally check result
struct cstream {
  void (*advance)(struct cstream*);
  int ok;
  int eof;
  char head;
  size_t index;
  void* private_data;
};

#define assert_stream_ready(stream) assert(stream); assert(stream->ok); assert(!stream->eof);

int parse_json(struct cstream* stream, struct json *dest);

// 0 <= n <= 18
int64_t quick_pow10(uint8_t n);

// -1 <= n <= 18
double quick_pow10d(int n);

#define is_digit(c) ((c) >= '0' && (c) <= '9')
#define is_ws(c) ((c) == ' ' || (c) == '\t' || (c) == '\n' || (c) == '\r')

/**
 * Returns the next json data_type given the
 * starting char
 * If char is an invalid starting char for a given json
 * data type, returns -1
 */
enum json_type next_value_starting_type(char c) {
  switch (c) {
    case 'f':
    case 't':
      return BOOL;
    case '{':
      return OBJECT;
    case '[':
      return ARRAY;
    case '-':
      return NUMBER;
    case '\"':
      return STRING;
    case 'n':
      return JSON_NULL;
    default:
      break;
  }

  if (is_digit(c))
    return NUMBER;

  printf_err("Unrecognized char: %c\n", c);
  return UNKNOWN;
}

int parse_special_character(struct cstream* stream, char* dest) {
  assert(dest);
  assert(stream->head == '\\');

  stream->advance(stream);
  if(stream->eof || !stream->ok)
    return 1;

  switch (stream->head) {
    case '\"':
      *dest = '\"';
      return 0;
      break;
    case '\\':
      *dest = '\\';
      return 0;
      break;
    case '/':
      *dest = '/';
      return 0;
      break;
    case 'b':
      *dest = '\b';
      return 0;
      break;
    case 'f':
      *dest = '\f';
      return 0;
      break;
    case 'n':
      *dest = '\n';
      return 0;
      break;
    case 'r':
      *dest = '\r';
      return 0;
      break;
    case 't':
      *dest = '\t';
      return 0;
      break;
    // case 'u':
    // TODO
    default:
      printf_err("Invalid special escape character: %c\n", stream->head);
      return 1;
  }
}

int double_buffer_if_exceeded(void** ptr, size_t* capacity, size_t cur_len) {
  assert(ptr);
  assert(capacity);
  assert(*capacity != 0);

  if(cur_len >= *capacity) {
    size_t new_capacity = *capacity * 2;
    void* new_ptr = realloc(ptr, new_capacity);
    if(new_ptr == NULL) {
      printf_err("Failed to reallocate memory when trying to increase buffer size from %zu to %zu bytes\n", *capacity, new_capacity); \
      return -1;
    }
    *ptr = new_ptr;
    *capacity = new_capacity;
  }
  return 0;
}


char *parse_string(struct cstream* stream) __attribute__((malloc));
char *parse_string(struct cstream* stream) {
  assert_stream_ready(stream);
  assert(stream->head == '\"');

  char* ret = NULL;
  size_t ret_size = 10;
  size_t start_i = stream->index;

  ret = malloc_err(ret_size * sizeof(char), "string result", ({ goto FAILED; }));

  while (1) {
    stream->advance(stream);
    if(!stream->ok || stream->eof) goto FAILED;

    size_t len = stream->index - start_i;
    size_t i = len - 1;

    if(double_buffer_if_exceeded((void**)&ret, &ret_size, len)) goto FAILED;

    if (stream->head == '\"') {
      ret[i] = '\0';
      return realloc_err(ret, len, ({ goto FAILED; }));
    }

    // If you hit a \, parse the next character to see what to return next
    if (stream->head == '\\') {
      if(parse_special_character(stream, &ret[i])) goto FAILED;
    } else {
      ret[i] = (char) stream->head;
      i++;
    }
  }

FAILED:
  free(ret);
  printf_err("Failed to parse string\n");
  return NULL;
}


/**
 * Returns next character ignoring any whitespace:
 * E.g. wswswswsp returns p wswswsEOF returns EOF
 * @param fp
 * @return
 */
int advance_ignore_ws(struct cstream* stream) {
  do {
    stream->advance(stream);
  } while (stream->ok && !stream->eof && is_ws(stream->head));
  return !stream->ok;
}

int advance_ignore_ws_if_ws(struct cstream* stream) {
  while(stream->ok && !stream->eof && is_ws(stream->head)) {
    stream->advance(stream);
  }
  return !stream->ok;
}

int matches(struct cstream* stream, const char* data, size_t dl) {
  assert_stream_ready(stream);
  assert(data);

  for(int i = 0; i < dl; ++i) {
    if(!stream->ok || stream->eof || stream->head != data[i])
      return 0;
    stream->advance(stream);
  }

  return 1;
}

int parse_true(struct cstream* stream) {
  assert_stream_ready(stream);
  assert(stream->head == 't');
  return !matches(stream, "true", 4) || !stream->ok;
}

int parse_false(struct cstream* stream) {
  assert_stream_ready(stream);
  assert(stream->head == 'f');
  return !matches(stream, "false", 5) || !stream->ok;
}

int parse_null(struct cstream* stream) {
  assert_stream_ready(stream);
  assert(stream->head == 'n');
  return !matches(stream, "null", 4) || !stream->ok;
}

int parse_member(struct cstream* stream, struct json_member *member) {
  assert_stream_ready(stream);
  assert(member);

  member->key = NULL;
  member->value = (struct json){ 0 };

  if(advance_ignore_ws(stream)) goto FAILED;

  if (stream->eof || stream->head != '\"') goto FAILED;

  member->key = unwrap(parse_string(stream), ({goto FAILED;}));

  if(advance_ignore_ws(stream)) goto FAILED;

  if (stream->eof || stream->head != ':') goto FAILED;

  if(advance_ignore_ws(stream)) goto FAILED;

  struct json value;
  if(stream->eof || parse_json(stream, &value)) goto FAILED;

  member->value = value;

  return 0 || !stream->ok;

FAILED:
  return -1;
}

int parse_object(struct cstream* stream, struct json_object *dest) {
  assert_stream_ready(stream);
  assert(dest);
  assert(stream->head == '{');
  struct json_member *members = NULL;

  size_t capacity = 10;
  dest->len = 0;
  members = malloc_err(sizeof(struct json_member) * capacity, "json members", ({ goto FAILED; }));

  while (1) {
    if(double_buffer_if_exceeded((void**)&members, &capacity, dest->len)) goto FAILED;

    if(parse_member(stream, &members[dest->len++])) goto FAILED;

    if(advance_ignore_ws_if_ws(stream)) goto FAILED;

    if(stream->eof) goto FAILED;

    if (stream->head == '}') {
      dest->members = realloc_err(ret, dest->len, ({ goto FAILED; }));
      return 0 || !stream->ok;
    } else if (stream->head == ',') {
      continue;
    } else {
      printf_err("Parse object: Invalid terminating char: %c\n", stream->head);
      goto FAILED;
    }
  }

FAILED:
  free(members);
  printf_err("Failed to parse json object\n");
  return -1;
}

int parse_array(struct cstream *stream, struct json_array *dest) {
  assert_stream_ready(stream);
  assert(dest);
  assert(stream->head == '[');
  struct json* values = NULL;

  size_t capacity = 10;
  values = malloc_err(sizeof(struct json) * capacity, "json values", ({ goto FAILED; }));
  dest->len = 0;

  while (1) {
    // Increase buffer if too small
    if(double_buffer_if_exceeded((void**)&values, &capacity, dest->len)) goto FAILED;

    if(parse_json(stream, &values[dest->len++])) goto FAILED;

    if(advance_ignore_ws_if_ws(stream)) goto FAILED;

    if(stream->eof) goto FAILED;

    if (stream->head == ']') {
      dest->values = realloc_err(values, dest->len * sizeof(struct json), ({ goto FAILED; }));
      return 0;
    } else if (stream->head == ',') {
      continue;
    } else {
      printf_err("Parse array: Invalid terminating char: %c\n", stream->head);
      goto FAILED;
    }
  }

FAILED:
  free(values);
  printf_err("Failed to parse json array\n");
  return -1;
}

int parse_dec(struct cstream* stream, int64_t* dest, size_t* dig_len) {
  assert_stream_ready(stream);
  assert(stream->head == '-' || is_digit(stream->head));

  int sign = 1;
  *dest = 0;
  if(dig_len) *dig_len = 0;

  if(stream->head == '-') {
    sign = -1;
    stream->advance(stream);
    if(!stream->ok || stream->eof || !is_digit(stream->head)) goto FAILED;
  }

  // Continue getting chars until non digit encountered
  while (1) {
    *dest *= 10;
    *dest += stream->head - '0';
    if(dig_len) (*dig_len)++;

    stream->advance(stream);

    if(!stream->ok) goto FAILED;

    if(stream->eof || !is_digit(stream->head)){
      *dest *= sign;
      return 0 || !stream->ok;
    }
  }

FAILED:
  printf_err("Failed to parse decimal\n");
  return -1;
}

int parse_number(struct cstream* stream, struct json_number *num) {
  assert_stream_ready(stream);
  assert(stream->head == '-' || is_digit(stream->head));

  int64_t before_dec;
  int64_t after_dec;
  size_t after_dec_len;
  int is_float = 0;
  int is_e = 0; // if true, adds
  int64_t e_part; // What comes after the e

  if(parse_dec(stream, &before_dec, NULL)) goto FAILED;

  if ((is_float = (stream->head == '.'))) {
    stream->advance(stream);
    if(!stream->ok || stream->eof || !is_digit(stream->head) || parse_dec(stream, &after_dec, &after_dec_len))
      goto FAILED;
  }

  if((is_e = (stream->head == 'e' || stream->head == 'E'))){
    stream->advance(stream);
    if(!stream->ok || stream->eof || !is_digit(stream->head) || stream->head != '-' || parse_dec(stream, &e_part, NULL))
      goto FAILED;
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

  return 0 || !stream->ok;

FAILED:
  printf_err("Failed to parse number\n");
  return 1;
}

int parse_json(struct cstream * stream, struct json *dest) {
  assert_stream_ready(stream);
  assert(dest);

  int next_json_type_to_parse = next_value_starting_type(stream->head);

  // Next json object
  dest->type = next_value_starting_type(stream->head);

  switch (dest->type) {
    case BOOL: {
      if(stream->head == 't'){
        if(parse_true(stream))
          goto FAILED;
        dest->data.bool_value = 1;
      }
      if(stream->head == 'f'){
        if(parse_false(stream))
          goto FAILED;
        dest->data.bool_value = 0;
      }
      break;
    }
    case OBJECT: {
      struct json_object obj;
      if (parse_object(stream, &obj))
        goto FAILED;
      dest->data.object = obj;
      break;
    }
    case 2: {
      struct json_array arr;
      if (parse_array(stream, &arr) < 0)
        goto FAILED;
      dest->data.array = arr;
      break;
    }
    case 3: {
      struct json_number num;
      if(parse_number(stream, &num))
        goto FAILED;
      dest->data.number = num;
      break;
    }
    case 4: {
      if(!(dest->data.string = parse_string(stream)))
        goto FAILED;
      break;
    }
    case 5: {
      if (parse_null(stream))
        goto FAILED;
      dest->data.null = 1;
      break;
    }
    default: {
      goto FAILED;
    }
  }
  return 0 || !stream->ok;

FAILED:
  return 1;
}


struct file_context {
  FILE* fp;
};

void advance_file(struct cstream* stream) {
  assert(stream);
  struct file_context* fctx = stream->private_data;

  int c = fgetc(fctx->fp);

  stream->ok = 1;
  stream->eof = 0;
  stream->head = fgetc(fctx->fp);

  if(stream->head == EOF){
    if(feof(fctx->fp))
      stream->eof = 1;
    else
      stream->ok = 0;
  } else {
    stream->index++;
  }
}

int file_cstream(const char* filename, struct cstream* dest) {
  assert(dest);
  assert(filename);

  FILE *fp = fopen(filename, "r");
  if(fp == NULL)
    return 1;

  struct file_context *fctx = malloc_err(sizeof *fctx, "file context", ({ return -1; }));
  *fctx = (struct file_context) {
    .fp = fp
  };

  struct cstream ret = (struct cstream) {
    .private_data = fctx,
    .advance = advance_file
  };

  ret.advance(&ret); // Start on the first char

  memcpy(dest, &ret, sizeof ret);

  return 0 || !ret.ok;
}

int cleanup_file_cstream(struct cstream* dest) {
  assert(dest);
  struct file_context* fctx = dest->private_data;
  assert(fctx);
  int ret = fclose(fctx->fp);
  free(fctx);
  return ret;
}

int fparse_json(const char *filename, struct json* dest) {
  if(!dest || !filename)
    return 1;

  struct cstream stream;
  if(file_cstream(filename, &stream))
    return 1;

  bzero(dest, sizeof(struct json));

  if(advance_ignore_ws_if_ws(&stream))
    goto FAILED;

  if(parse_json(&stream, dest))
    goto FAILED;

  cleanup_file_cstream(&stream);
  return 0;

FAILED:
  cleanup_file_cstream(&stream);
  return 1;
}

void print_json_with_indents(struct json json, size_t indents);

void print_json_member(struct json_member member) {
  printf("\"%s\":", member.key);
  print_json(member.value);
}

void print_json_number(struct json_number number) {
  switch (number.type) {
    case 0:
      printf("%lld", number.data.int_val);
      break;
    case 1:
      printf("%f", number.data.float_val);
      break;
    default:
      json_error("Invalid number type: %d", number.type);
      break;
  }
}

void print_json(struct json json) {
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
        print_json_member(json.data.object.members[i]);
        if (i != json.data.object.len - 1) {
          printf(",");
        }
      }
      printf("}");
      break;
    case 2:
      printf("[");
      for (int i = 0; i < json.data.array.len; ++i) {
        print_json(json.data.array.values[i]);
        if (i != json.data.array.len - 1) {
          printf(",");
        }
      }
      printf("]");
      break;
    case 3:
      print_json_number(json.data.number);
      break;
    case 4:
      printf("\"%s\"", json.data.string);
      break;
    case 5:
      printf("null");
      break;
    default:
      json_error("Invalid type: %d", json.type);
  }
}

void free_json_member(struct json_member member) {
  free(member.key);
  member.key = NULL;
  free_json(member.value);
}

void free_json(struct json json) {
  switch (json.type) {
    case 1:
      for (int i = 0; i < json.data.object.len; ++i) {
        print_json_member(json.data.object.members[i]);
        if (i != json.data.object.len - 1) {
          free_json_member(json.data.object.members[i]);
        }
        free(json.data.object.members);
        json.data.object.members = NULL;
        json.data.object.len = 0;
      }
      break;
    case 2:
      for (int i = 0; i < json.data.array.len; ++i) {
        print_json(json.data.array.values[i]);
        if (i != json.data.array.len - 1) {
          free_json(json.data.array.values[i]);
        }
        free(json.data.array.values);
        json.data.array.values = NULL;
        json.data.array.len = 0;
      }
      break;
    case 3:
      print_json_number(json.data.number);
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
    json_error("Can't compute power greater than e18\n");
    return -1;
  }
  if (n < 0) {
    json_error("Quick pow isn't designed for negative n\n");
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


