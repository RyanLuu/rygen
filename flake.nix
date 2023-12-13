{
  description = "Rygen flake";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = inputs@{ flake-parts, ... }:
    flake-parts.lib.mkFlake { inherit inputs; } {
      systems = [
        "x86_64-linux"
      ];
      perSystem = { config, self', inputs', pkgs, system, lib, ... }:
        let
          stdenv = pkgs.keepDebugInfo pkgs.clangStdenv;
          wasmenv = pkgs.pkgsCross.wasi32.llvmPackages_16.stdenv;
        in
        rec {
          devShells.default = pkgs.mkShell.override { inherit stdenv; } {
            name = "clang";
            packages = with pkgs; [
              clang-tools_16
              man-pages
              man-pages-posix
              libllvm
              valgrind
              gdb
              lighttpd
            ] ++ packages.default.buildInputs; # get man pages for build inputs
          };
          packages.default =
            let
              rygen-wasm = (wasmenv.mkDerivation rec {
                name = "rygen-wasm";
                src = ./src/wasm;
                buildPhase = ''
                  $CC -v -o add.wasm add.c -nostartfiles -Wl,--no-entry -Wl,--export-all
                '';
                installPhase = ''
                  mkdir -p $out
                  cp *.wasm $out
                '';
              });
            in
            stdenv.mkDerivation rec {
              name = "rygen";
              src = ./src;

              buildInputs = with pkgs; [
                cmark
                tomlc99
              ];

              buildPhase =
                let
                  sources = builtins.concatStringsSep " "
                    [ "main.c" "meta.c" "tmpl.c" "util.c" "mustach/mustach.c" ];
                  includes = builtins.concatStringsSep " "
                    (builtins.map (l: "-I${lib.getDev l}/include") buildInputs);
                  ldpath = builtins.concatStringsSep " "
                    (builtins.map (l: "-I${lib.getLib l}/lib") buildInputs);
                in
                ''
                  cc -Wall -Werror -Wpedantic -o ${name} ${sources} ${includes} ${ldpath} -lcmark -ltoml
                '';

              installPhase = ''
                mkdir -p $out/bin/wasm
                cp ${lib.getBin rygen-wasm}/* $out/bin/wasm/
                mkdir -p $out/bin
                cp ${name} $out/bin/
              '';
            };
        };
    };
}

