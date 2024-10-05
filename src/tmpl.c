#include "tmpl.h"

#include <assert.h>
#include <cmark.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <tree_sitter/api.h>
#include <unistd.h>

#include "hescape/hescape.h"
#include "mustach/mustach.h"
#include "util.h"

#define TS_GRAMMARS                                                                                \
  X(c)                                                                                             \
  X(rust)

#define X(grammar) TSLanguage *tree_sitter_##grammar(void);
TS_GRAMMARS
#undef X

typedef struct {
  char *name;
  TSLanguage *(*tsl)(void);
} Language;

Language g_languages[] = {
#define X(grammar) {#grammar, tree_sitter_##grammar},
    TS_GRAMMARS
#undef X
};

FILE *open_post_md(const char *slug) {
  char path[MAX_PATH_LEN];
  snprintf(path, sizeof(path), "posts/%s.md", slug);
  FILE *fp = fopen(path, "r");
  if (fp == NULL) {
    PANIC_ERRNO("Failed to open post %s", path);
  }
  return fp;
}

static bool is_interesting_node(TSNode node) {
  char *interesting_node_types[] = {
      // commment
      "line_comment",
      "comment",
      // string literal
      "string_literal",
      // numeric literal
      "integer_literal",
      "float_literal",
      "number_literal",
      // type
      "primitive_type",
      "type_identifier",
      // preproc
      "preproc_include",
  };
  for (size_t i = 0; i < arrlen(interesting_node_types); ++i) {
    if (strcmp(ts_node_type(node), interesting_node_types[i]) == 0) {
      return true;
    }
  }
  return false;
}

static char *highlight_code_node(char *dest, const char *src, uint32_t *cursor, TSNode node) {
  printf("%s\n", ts_node_type(node));
  uint32_t child_count = ts_node_child_count(node);
  if (child_count == 0) {
    uint32_t start = ts_node_start_byte(node);
    uint32_t end = ts_node_end_byte(node);
    if (*cursor < start) {
      char *esc_code = NULL;
      int len = hesc_escape_html((uint8_t **)&esc_code, (uint8_t *)src + *cursor, start - *cursor);
      dest += sprintf(dest, "%.*s", len, esc_code);
    }
    if (is_interesting_node(node)) {
      dest += sprintf(dest, "<span class=\"%s\">%.*s</span>", ts_node_type(node), end - start,
                      src + start);
    } else {
      char *esc_code = NULL;
      int len = hesc_escape_html((uint8_t **)&esc_code, (uint8_t *)src + start, end - start);
      dest += sprintf(dest, "%.*s", len, esc_code);
    }
    *cursor = end;
  } else {
    bool interesting = is_interesting_node(node);
    if (interesting) {
      dest += sprintf(dest, "<span class=\"%s\">", ts_node_type(node));
    }
    for (uint32_t i = 0; i < child_count; ++i) {
      dest = highlight_code_node(dest, src, cursor, ts_node_child(node, i));
    }
    if (interesting) {
      dest += sprintf(dest, "</span>");
    }
  }
  return dest;
}

static char *highlight_code(char *dest, const char *src, uint32_t srclen, TSTree *tree) {
  TSNode root_node = ts_tree_root_node(tree);
  uint32_t cursor = 0;
  dest = highlight_code_node(dest, src, &cursor, root_node);
  if (cursor < srclen) {
    dest += sprintf(dest, "%.*s", srclen - cursor, src + cursor);
  }
  return dest;
}

char *render_post_content(const char *slug) {
  FILE *post_md = open_post_md(slug);
  cmark_node *node = cmark_parse_file(post_md, CMARK_OPT_DEFAULT);

  // DEBUG
  {
    cmark_iter *iter = cmark_iter_new(node);
    cmark_event_type e;
    do {
      e = cmark_iter_next(iter);
      switch (e) {
      case CMARK_EVENT_NONE:
        break;
      case CMARK_EVENT_DONE:
        break;
      case CMARK_EVENT_ENTER:
        if (cmark_node_get_type(cmark_iter_get_node(iter)) == CMARK_NODE_CODE_BLOCK) {
          cmark_node *code_block_node = cmark_iter_get_node(iter);
          const char *code = cmark_node_get_literal(code_block_node);

          TSLanguage *language = NULL;
          const char *fence_info = cmark_node_get_fence_info(code_block_node);
          for (int i = 0; i < sizeof(g_languages) / sizeof(*g_languages); ++i) {
            if (!strcmp(fence_info, g_languages[i].name)) {
              language = g_languages[i].tsl();
              break;
            }
          }
          if (language == NULL) {
            break;
          }
          TSParser *parser = ts_parser_new();
          ts_parser_set_language(parser, language);
          assert(ts_parser_language(parser) != NULL);
          TSTree *tree = ts_parser_parse_string(parser, NULL, code, strlen(code));
          ts_parser_delete(parser);

          char code_buf[4096]; // TODO: dynamic buffer
          char *code_start =
              code_buf + sprintf(code_buf, "<pre><code class=\"language-%s\">", fence_info);
          cmark_node *new_code_node = cmark_node_new(CMARK_NODE_HTML_BLOCK);
          char *code_end = highlight_code(code_start, code, strlen(code), tree);
          sprintf(code_end, "</code></pre>");
          cmark_node_set_literal(new_code_node, code_buf);
          assert(cmark_node_insert_after(code_block_node, new_code_node));
          cmark_node_free(code_block_node); // automatically unlinks
          ts_tree_delete(tree);
        }
        break;
      case CMARK_EVENT_EXIT:
        break;
      }
    } while (e != CMARK_EVENT_DONE);
    cmark_iter_free(iter);
  }

  fclose(post_md);
  char *html = cmark_render_html(node, CMARK_OPT_UNSAFE);
  cmark_node_free(node);
  return html;
}

string_t read_template(const char *slug) {
  char path[MAX_PATH_LEN];
  snprintf(path, sizeof(path), "templates/%s.mustache", slug);
  return read_file(path);
}

int enter(void *closure, const char *name) {
  closure_t *c = (closure_t *)closure;
  switch (c->state) {
  case ROOT:
    if (strcmp("posts", name) == 0 && c->meta->num_posts > 0) {
      c->state = POST;
      return 1;
    } else if (strcmp("tags", name) == 0 && c->meta->num_tags > 0) {
      c->state = TAG;
      return 1;
    }
    break;
  case POST:
    if (strcmp("tags", name) == 0 && c->meta->posts[c->index].num_tags > 0) {
      c->state = POST_TAG;
      return 1;
    } else if (strcmp("js", name) == 0) {
      char path[MAX_PATH_LEN];
      int bytes = snprintf(path, MAX_PATH_LEN, OUTPUT_DIR "/scripts/post/%s.js",
                           c->meta->posts[c->index].slug);
      assert(bytes >= 0 && bytes < MAX_PATH_LEN);
      if (access(path, R_OK) == 0) {
        c->state = POST_JS;
        return 1;
      }
    }
    break;
  case TAG:
    if (strcmp("posts", name) == 0 && c->meta->tags[c->index].num_posts > 0) {
      c->state = TAG_POST;
      return 1;
    }
    break;
  case POST_TAG:
  case POST_JS:
  case TAG_POST:
    break;
  }
  return 0;
}

int next(void *closure) {
  closure_t *c = (closure_t *)closure;
  if (c->state == POST && c->index + 1 < c->meta->num_posts) {
    ++c->index;
    return 1;
  }
  if (c->state == TAG && c->index + 1 < c->meta->num_tags) {
    ++c->index;
    return 1;
  }
  if (c->state == POST_TAG && c->index_inner + 1 < c->meta->posts[c->index].num_tags) {
    ++c->index_inner;
    return 1;
  }
  if (c->state == TAG_POST && c->index_inner + 1 < c->meta->tags[c->index].num_posts) {
    ++c->index_inner;
    return 1;
  }
  return 0;
}

int leave(void *closure) {
  closure_t *c = (closure_t *)closure;
  switch (c->state) {
  case ROOT:
    PANIC("Unreachable");
  case POST:
  case TAG:
    c->state = ROOT;
    c->index = 0;
    break;
  case POST_TAG:
    c->state = POST;
    c->index_inner = 0;
    break;
  case POST_JS:
    c->state = POST;
    break;
  case TAG_POST:
    c->state = TAG;
    c->index_inner = 0;
    break;
  }
  return 0;
}

char *get_post(meta_post_t *post, const char *name) {
  if (strcmp(name, "slug") == 0) {
    return post->slug;
  } else if (strcmp(name, "title") == 0) {
    return post->title;
  } else if (strcmp(name, "desc") == 0) {
    return (post->desc != NULL) ? post->desc : "";
  } else if (strcmp(name, "content") == 0) {
    post->content = render_post_content(post->slug);
    return post->content;
  } else if (strcmp(name, "date") == 0) {
    return post->date;
  }
  return NULL;
}

char *get_tag(meta_tag_t *tag, const char *name) {
  printf("tag.'%s'\n", name);
  if (strcmp(name, "id") == 0) {
    printf("tag id = %s\n", tag->id);
    return tag->id;
  }
  return NULL;
}

char *get_js(meta_post_t *post, const char *name) {
  if (strcmp(name, "path") == 0) {
    char path[MAX_PATH_LEN];
    int bytes = snprintf(path, sizeof(path), "/scripts/post/%s.js", post->slug);
    assert(bytes >= 0 && bytes < MAX_PATH_LEN);
    post->js = malloc_panic(bytes + 1);
    memcpy(post->js, path, bytes + 1);
    return post->js;
  }
  return NULL;
}

char *get_root(meta_t *meta, const char *name) {
  if (strcmp(name, "site_name") == 0) {
    return meta->site_name;
  } else if (strcmp(name, "site_url") == 0) {
    return meta->site_url;
  } else if (strcmp(name, "site_desc") == 0) {
    return meta->site_desc;
  } else if (strcmp(name, "version") == 0) {
    return meta->version;
  }
  return NULL;
}

int get(void *closure, const char *name, struct mustach_sbuf *sbuf) {
  closure_t *c = (closure_t *)closure;
  *sbuf = (struct mustach_sbuf){
      .value = NULL,
      .closure = closure,
      .freecb = NULL,
      .length = 0,
  };
  switch (c->state) {
  case ROOT:
    break;
  case POST:
    sbuf->value = get_post(&c->meta->posts[c->index], name);
    break;
  case TAG:
    sbuf->value = get_tag(&c->meta->tags[c->index], name);
    break;
  case POST_TAG: {
    meta_post_t *post = &c->meta->posts[c->index];
    uint32_t tag_handle = post->tag_handles[c->index_inner];
    sbuf->value = get_tag(&c->meta->tags[tag_handle], name);
  } break;
  case POST_JS:
    sbuf->value = get_js(&c->meta->posts[c->index], name);
    break;
  case TAG_POST: {
    meta_tag_t *tag = &c->meta->tags[c->index];
    uint32_t post_handle = tag->post_handles[c->index_inner];
    sbuf->value = get_post(&c->meta->posts[post_handle], name);
  } break;
  }
  if (sbuf->value == NULL) {
    sbuf->value = get_root(c->meta, name);
  }
  if (sbuf->value == NULL) {
    fprintf(stderr, "Failed to get value %s in state %d\n", name, c->state);
    return MUSTACH_ERROR_USER(0);
  }
  return MUSTACH_OK;
}

int partial(void *closure, const char *name, struct mustach_sbuf *sbuf) {
  string_t tmpl = read_template(name);
  *sbuf = (struct mustach_sbuf){
      .value = tmpl.data,
      .closure = closure,
      .freecb = free,
      .length = tmpl.length,
  };
  return MUSTACH_OK;
}

FILE *open_output_file(char *slug, char *fext) {
  char path[MAX_PATH_LEN];
  snprintf(path, sizeof(path), OUTPUT_DIR "/%s.%s", slug, fext);
  FILE *fp = fopen(path, "w");
  if (fp == NULL) {
    PANIC_ERRNO("Failed to open file %s", path);
  }
  return fp;
}

void render_file(closure_t *closure, string_t tmpl, char *slug_out, char *ext) {
  FILE *fp_out = open_output_file(slug_out, ext);

  struct mustach_itf itf = {
      .start = NULL,
      .put = NULL,
      .enter = enter,
      .next = next,
      .leave = leave,
      .partial = partial,
      .emit = NULL,
      .get = get,
      .stop = NULL,
  };
  int status =
      mustach_file(tmpl.data, tmpl.length, &itf, closure, Mustach_With_NoExtensions, fp_out);
  if (status == -1) {
    PANIC_ERRNO("Failed to render template %*s to page %s", (int)tmpl.length, tmpl.data, slug_out);
  } else if (status < -1) {
    PANIC("Failed to render template: %d", status);
  }
  fclose(fp_out);
}

void make_output_dir(char *path) {
  if (mkdir(path, 0777)) {
    if (errno == EEXIST) {
      struct stat statbuf;
      lstat(path, &statbuf);
      if (!S_ISDIR(statbuf.st_mode)) {
        PANIC("Non-directory already exists: %s (0%06o)", path, statbuf.st_mode);
      }
      return;
    }
    PANIC_ERRNO("Failed to make directory: %s", path);
  }
}

void copy_files(char *fromdir, char *todir) {
  DIR *dirp = opendir(fromdir);
  if (dirp == NULL) {
    PANIC_ERRNO("Failed to open directory: %s", fromdir);
  }
  struct dirent *ep;
  while ((ep = readdir(dirp)) != NULL) {
    if (strcmp(".", ep->d_name) == 0 || strcmp("..", ep->d_name) == 0) {
      continue;
    }
    char static_pathname[MAX_PATH_LEN];
    int s = snprintf(static_pathname, MAX_PATH_LEN, "%s/%s", fromdir, ep->d_name);
    if (s < 0) {
      PANIC_ERRNO("Failed to construct static file path");
    } else if (s >= MAX_PATH_LEN) {
      PANIC("Failed to construct static file path: length exceeded (%d >= %d)", s, MAX_PATH_LEN);
    }
    switch (ep->d_type) {
    case DT_UNKNOWN: {
      struct stat statbuf;
      lstat(static_pathname, &statbuf);
      if (S_ISREG(statbuf.st_mode)) {
        goto not_regular;
      }
    } break;
    case DT_REG: // regular file
      break;
    default:
    not_regular:
      // skip file if not regular
      continue;
    }
    FILE *static_fp = fopen(static_pathname, "r");
    if (static_fp == NULL) {
      printf("Skipping file %s: failed to open: %s\n", static_pathname, strerror(errno));
      continue;
    }
    char output_pathname[MAX_PATH_LEN];
    s = snprintf(output_pathname, MAX_PATH_LEN, "%s/%s", todir, ep->d_name);
    if (s < 0) {
      PANIC_ERRNO("Failed to construct output file path");
    } else if (s >= MAX_PATH_LEN) {
      PANIC("Failed to construct output file path: length exceeded (%d >= %d)", s, MAX_PATH_LEN);
    }
    FILE *output_fp = fopen(output_pathname, "w+");
    if (output_fp == NULL) {
      printf("Skipping file %s: failed to open: %s\n", output_pathname, strerror(errno));
      fclose(static_fp);
      continue;
    }
    printf("  %s => %s\n", static_pathname, output_pathname);
    while (!feof(static_fp)) {
      char buffer[4096];
      size_t bytes_read = fread(buffer, 1, sizeof(buffer), static_fp);
      if (bytes_read < sizeof(buffer) && ferror(static_fp)) {
        PANIC("Failed to read from file %s", static_pathname);
      }
      size_t bytes_written = fwrite(buffer, bytes_read, 1, output_fp);
      if (bytes_written < bytes_read && ferror(output_fp)) {
        PANIC("Failed to write to file %s", output_pathname);
      }
    }
    fclose(static_fp);
    fclose(output_fp);
  }
  closedir(dirp);
}
