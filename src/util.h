#ifndef _SSG_UTIL_H_
#define _SSG_UTIL_H_

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "conf.h"

#define PANIC(...)                                                                                 \
  fprintf(stderr, "panic: (%s %s:%d) ", __func__, __FILE__, __LINE__);                             \
  fprintf(stderr, __VA_ARGS__);                                                                    \
  fprintf(stderr, "\n");                                                                           \
  exit(EXIT_FAILURE);

#define PANIC_ERRNO(...)                                                                           \
  fprintf(stderr, "panic: (%s %s:%d) ", __func__, __FILE__, __LINE__);                             \
  fprintf(stderr, __VA_ARGS__);                                                                    \
  fprintf(stderr, ": %s\n", strerror(errno));                                                      \
  exit(EXIT_FAILURE);

#define arrlen(a) (size_t)(sizeof(a) / sizeof(*(a)))

typedef struct {
  char *data;
  size_t length;
} string_t;

extern void *malloc_panic(size_t size);
extern string_t read_file(const char *filename);
extern char *empty_string(void);

#endif
