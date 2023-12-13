#include "meta.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

#define TOML_ERRBUF_SIZE 256

#define MAX_TAGS 256
#define MAX_POSTS_PER_TAG 64

uint32_t meta_tag_handle(const meta_t *meta, const char *tag) {
  for (int i = 0; i < meta->num_tags; ++i) {
    if (strcmp(tag, meta->tags[i].id) == 0) {
      return i;
    }
  }
  return -1;
}

int meta_add_tag(meta_t *meta, char *id) {
  if (meta->num_tags == MAX_TAGS) {
    PANIC("Exceeded maximum tag count of %d", MAX_TAGS);
  }
  int ret = meta->num_tags;
  meta->tags[ret] = (meta_tag_t){
      .id = id, .post_handles = malloc_panic(MAX_POSTS_PER_TAG * sizeof(uint32_t)), .num_posts = 0};
  ++meta->num_tags;
  return ret;
}

void meta_add_tag_to_post(meta_t *meta, uint32_t post_handle, uint32_t local_tag_idx,
                          uint32_t tag_handle) {
  meta->posts[post_handle].tag_handles[local_tag_idx] = tag_handle;
  meta_tag_t *tag = &meta->tags[tag_handle];
  tag->post_handles[tag->num_posts] = post_handle;
  ++tag->num_posts;
}

int meta_post_date_cmp(const void *a, const void *b) {
  return -strncmp(((meta_post_t *)a)->date, ((meta_post_t *)b)->date, 10);
}

meta_t *meta_render(const toml_table_t *meta_toml) {
  meta_t *meta = malloc_panic(sizeof(meta_t));

  toml_datum_t name_toml = toml_string_in(meta_toml, "name");
  if (!name_toml.ok) {
    PANIC("Failed to get site name");
  }
  meta->site_name = name_toml.u.s;

  meta->version = malloc_panic(8);
  snprintf(meta->version, 8, "v%d.%d", SSG_VERSION_MAJOR, SSG_VERSION_MINOR);

  toml_table_t *post_toml = toml_table_in(meta_toml, "post");
  meta->num_posts = 0;
  if (post_toml) {
    while (toml_key_in(post_toml, meta->num_posts) != NULL) {
      ++meta->num_posts;
    }
  }
  meta->posts = malloc_panic(meta->num_posts * sizeof(meta_post_t));
  meta->tags = malloc_panic(MAX_TAGS * sizeof(meta_tag_t));
  meta->num_tags = 0;

  toml_array_t *pages_toml = toml_array_in(meta_toml, "pages");
  assert(pages_toml != NULL);
  assert(toml_array_type(pages_toml) == 's' || toml_array_type(pages_toml) == 0);
  meta->num_pages = toml_array_nelem(pages_toml);
  meta->pages = malloc_panic(meta->num_pages * sizeof(char *));
  for (uint32_t i = 0; i < meta->num_pages; ++i) {
    toml_datum_t page_toml = toml_string_at(pages_toml, i);
    assert(page_toml.ok);
    meta->pages[i] = page_toml.u.s;
  }

  for (uint32_t post_handle = 0; post_handle < meta->num_posts; ++post_handle) {
    meta->posts[post_handle].content = NULL; // lazy loaded during template rendering
    meta->posts[post_handle].js = NULL;      // lazy loaded during template rendering
    const char *slug = toml_key_in(post_toml, post_handle);
    meta->posts[post_handle].slug = malloc_panic(strlen(slug) + 1);
    strcpy(meta->posts[post_handle].slug, slug);
    toml_table_t *post_i_toml = toml_table_in(post_toml, meta->posts[post_handle].slug);

    toml_datum_t title_toml = toml_string_in(post_i_toml, "title");
    if (!title_toml.ok) {
      PANIC("Failed to get title for post %s", meta->posts[post_handle].slug);
    }
    meta->posts[post_handle].title = title_toml.u.s;

    toml_datum_t date_toml = toml_timestamp_in(post_i_toml, "date");
    if (!date_toml.ok) {
      PANIC("Failed to get date for post %s", meta->posts[post_handle].slug);
    }
    int bytes_written =
        sprintf(meta->posts[post_handle].date, "%04d-%02d-%02d", *date_toml.u.ts->year,
                *date_toml.u.ts->month, *date_toml.u.ts->day);
    if (bytes_written != 10) {
      PANIC("Failed to materialize date for post %s", meta->posts[post_handle].slug);
    }
    free(date_toml.u.ts);

    toml_array_t *tags_toml = toml_array_in(post_i_toml, "tags");
    if (tags_toml == NULL) {
      meta->posts[post_handle].num_tags = 0;
    } else {
      meta->posts[post_handle].num_tags = toml_array_nelem(tags_toml);
    }
    meta->posts[post_handle].tag_handles =
        malloc_panic(sizeof(uint32_t) * meta->posts[post_handle].num_tags);
    for (int itag = 0; itag < meta->posts[post_handle].num_tags; ++itag) {
      toml_datum_t tag_toml = toml_string_at(tags_toml, itag);
      if (!tag_toml.ok) {
        PANIC("Failed to get tag %d for post %s", itag, meta->posts[post_handle].slug);
      }
      int tag_handle = meta_tag_handle(meta, tag_toml.u.s);
      if (tag_handle == -1) {
        tag_handle = meta_add_tag(meta, tag_toml.u.s);
      } else {
        free(tag_toml.u.s);
      }
      meta_add_tag_to_post(meta, post_handle, itag, tag_handle);
    }
  }
  qsort(meta->posts, meta->num_posts, sizeof(meta_post_t), meta_post_date_cmp);
  return meta;
}

meta_t *meta_parse(char *filename) {
  FILE *fp = fopen(filename, "r");
  if (fp == NULL) {
    PANIC_ERRNO("Failed to open %s", filename);
  }
  char errbuf[TOML_ERRBUF_SIZE];
  toml_table_t *meta_toml = toml_parse_file(fp, errbuf, sizeof(errbuf));
  fclose(fp);
  if (meta_toml == NULL) {
    PANIC("Failed to parse %s: %s", filename, errbuf);
  }
  meta_t *meta = meta_render(meta_toml);
  toml_free(meta_toml);
  return meta;
}

void meta_free(meta_t *meta) {
  for (int i = 0; i < meta->num_posts; ++i) {
    free(meta->posts[i].slug);
    free(meta->posts[i].title);
    free(meta->posts[i].tag_handles);
    if (meta->posts[i].content != NULL) {
      free(meta->posts[i].content);
    }
    if (meta->posts[i].js != NULL) {
      free(meta->posts[i].js);
    }
  }
  free(meta->posts);

  for (int i = 0; i < meta->num_tags; ++i) {
    free(meta->tags[i].id);
    free(meta->tags[i].post_handles);
  }
  free(meta->tags);

  for (int i = 0; i < meta->num_pages; ++i) {
    free(meta->pages[i]);
  }
  free(meta->pages);

  free(meta->site_name);
  free(meta->version);
  free(meta);
}

void meta_debug(const meta_t *meta) {
  printf("Name: %s\n", meta->site_name);
  printf("Pages: [ ");
  for (uint32_t i = 0; i < meta->num_pages; ++i) {
    printf("%s ", meta->pages[i]);
  }
  printf("]\n");
  printf("Posts: %u\n", meta->num_posts);
  for (uint32_t i = 0; i < meta->num_posts; ++i) {
    printf("  %s: \"%s\" %s [ ", meta->posts[i].slug, meta->posts[i].title, meta->posts[i].date);
    for (uint32_t j = 0; j < meta->posts[i].num_tags; ++j) {
      printf("%u ", meta->posts[i].tag_handles[j]);
    }
    printf("]\n");
  }
  printf("Tags: %u\n", meta->num_tags);
  for (uint32_t i = 0; i < meta->num_tags; ++i) {
    printf("  %s: [ ", meta->tags[i].id);
    for (uint32_t j = 0; j < meta->tags[i].num_posts; ++j) {
      printf("%u ", meta->tags[i].post_handles[j]);
    }
    printf("]\n");
  }
}
