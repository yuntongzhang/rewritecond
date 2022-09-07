# RewriteCond

Transform conditionals in C/C++ program to assignment + reference to a single variable.

Source-to-source transformation based on clang Transformer.

Tested on Ubuntu-18 and clang-14. Other OS versions probably also work provided that
the clang binaries (with version 14.0.0 and above) work on that OS.

## Usage

1. Download and unzip clang and llvm binaries (version 14 or above). For ubuntu-18, download from:
https://github.com/llvm/llvm-project/releases/download/llvmorg-14.0.0/clang+llvm-14.0.0-x86_64-linux-gnu-ubuntu-18.04.tar.xz.

2. In this directory, run `make BINARY_DIR_PATH=<path-to-unzipped-folder>`.
Tool should now be in `build`. A shortcut is created at root called `rewritecond`.

### Running on simple example

To run the tool, do `./rewritecond examples/test.c --`. The file `examples/test.c` shows
what can be handled by the current tool.

### Running on large codebase

For large codebase, a compilation database is required. First, install bear:

```
sudo apt install bear
```

To generate a compilation database, prepend the build command with `bear` and build the target
project. For example, if the original build command is `make`, now it becomes `bear make`.
After building, there should be a file `compile_commands.json` generated.

To run the tool, do:

```
./rewrite_cond <path_to_file>`
```
The compilation database is automatically searched in the parent directories of the input file.
Alternatively, specify the directory containing `compile_commands.json` with `-p=<dir>`.
