#ifndef _SSG_CONF_H_
#define _SSG_CONF_H_

#define SSG_VERSION (1)
#define SSG_VERSION_MAJOR (SSG_VERSION / 100)
#define SSG_VERSION_MINOR (SSG_VERSION % 100)

#define METADATA_FILE "sausage.toml"
#define STATIC_DIR "static"
#define WASM_DIR "result/bin/wasm"
#define OUTPUT_DIR "public"

#define MAX_PATH_LEN 1024

#endif
