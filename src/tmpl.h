#ifndef _SSG_TMPL_H_
#define _SSG_TMPL_H_

#include "meta.h"
#include "util.h"

typedef enum { ROOT = 0, POST, TAG, POST_TAG, POST_JS, TAG_POST } closure_state_e;

typedef struct {
  meta_t *meta;
  uint32_t index;
  uint32_t index_inner;
  closure_state_e state;
} closure_t;

extern void make_output_dir(char *path);
extern void copy_files(char *fromdir, char *todir);
extern string_t read_template(const char *slug);
extern void render_html(closure_t *closure, string_t tmpl, char *slug_out);

#endif
