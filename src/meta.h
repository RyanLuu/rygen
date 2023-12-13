#ifndef _SSG_META_H_
#define _SSG_META_H_

#include <stdint.h>

#include "toml.h"

typedef struct {
  char *slug;
  char *title;
  char date[11]; // YYYY-MM-DD\0
  char *content;
  uint32_t *tag_handles;
  uint32_t num_tags;
  char *js;
} meta_post_t;

typedef struct {
  char *id;
  uint32_t *post_handles;
  uint32_t num_posts;
} meta_tag_t;

typedef struct {
  char *site_name;
  char *version;
  meta_post_t *posts;
  uint32_t num_posts;
  meta_tag_t *tags;
  uint32_t num_tags;
  char **pages;
  uint32_t num_pages;
} meta_t;

extern meta_t *meta_parse(char *filename);
extern void meta_free(meta_t *meta);
extern void meta_debug(const meta_t *meta);

#endif
