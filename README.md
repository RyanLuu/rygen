# rygen

`rygen` is a static site generator.

## Getting started

First, build and run rygen:

```
nix build
result/bin/rygen
```

Then, start an HTTP server:

```
lighttpd -D -f lighttpd.conf 
# or
cd public; python3 -m http.server
```

Finally, visit it at http://localhost:8000/.

## Adding content

To add a post, simply write Markdown in `posts/foo.md`, then add the following to `rygen.toml`:

```toml
[post.foo]
title = "New post"
tags = [ "bar", "baz" ]
date = 2023-12-31
```

## Customizing

HTML templates can be found in `templates/` and use [mustache](http://mustache.github.io/) as a templating language.

CSS styles and other static content can be found in `static/`. The color scheme can be easily replaced with another Base16 color scheme by replacing `static/color.css` with another [css-variables theme](https://github.com/samme/base16-styles/tree/master/css-variables). You can also create your own theme with the [css-variables template](https://github.com/samme/base16-styles/blob/master/templates/css-variables.mustache).

## Future plans

- Code highlighting with [tree-sitter](https://github.com/tree-sitter/tree-sitter)
- Arbitrary data specified in `rygen.toml` and queryable in templates
- Write `publish` script that manages publish date and creates/updates pages
- Better JS & WASM support

