# RewriteCond

Transform conditionals in C/C++ program to assignment + reference to a single variable.

Source-to-source transformation based on clang Transformer.

Tested on Ubuntu-18 and clang-14. Other OS versions probably also work provided that
the clang binaries (with version 14.0.0 and above) work on that OS.

## Usage

1. Download and unzip clang and llvm binaries (version 14 or above). For ubuntu-18, download from:
https://github.com/llvm/llvm-project/releases/download/llvmorg-14.0.0/clang+llvm-14.0.0-x86_64-linux-gnu-ubuntu-18.04.tar.xz.

2. In this directory, run `make BINARY_DIR_PATH=<path-to-unzipped-folder>`.
Tool should now be in `build`.

3. To run the tool, do `build/rewrite_cond examples/test.c --`. The file `examples/test.c` shows
what can be handled by the current tool.
