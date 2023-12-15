#include "util.h"

#include <string.h>

void *malloc_panic(size_t size) {
  void *p = malloc(size);
  if (size > 0 && p == NULL) {
    PANIC("Failed to allocate memory");
  }
  return p;
}

string_t read_file(const char *path) {
  FILE *fp = fopen(path, "r");
  if (fp == NULL) {
    PANIC_ERRNO("Failed to open template %s", path);
  }
  fseek(fp, 0, SEEK_END);
  long length = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  string_t string = {
      .data = malloc_panic(length + 1),
      .length = length,
  };
  if (fread(string.data, length, 1, fp) == 0 && ferror(fp)) {
    PANIC_ERRNO("Failed to read template %s", path);
  }
  fclose(fp);
  return string;
}

static char empty = '\0';

char *empty_string(void) { return &empty; }
