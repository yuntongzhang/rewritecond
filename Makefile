#-------------------------------------------------------------------------------
# Make file for building libtool tools based on compiled llvm-clang binaries.
#
# Inspired by: https://github.com/eliben/llvm-clang-samples/blob/master/Makefile
#
#-------------------------------------------------------------------------------

# The VAR that likely requires chaning is BINARY_DIR_PATH. It should point to the untarred directory
# of downloaded llvm built binaries.
# Sample of the download link: https://github.com/llvm/llvm-project/releases/download/llvmorg-14.0.0/clang+llvm-14.0.0-x86_64-linux-gnu-ubuntu-18.04.tar.xz
#
# Then, change BINARY_DIR_PATH according, either here or by `make BINARY_DIR_PATH=VALUE`.

# directory of the untar binaries
BINARY_DIR_PATH := $$HOME/llvm-binaries
LLVM_SRC_PATH := $(BINARY_DIR_PATH)
LLVM_BUILD_PATH := $(BINARY_DIR_PATH)/bin
LLVM_BIN_PATH 	:= $(BINARY_DIR_PATH)/bin

$(info -----------------------------------------------)
$(info Using LLVM_SRC_PATH = $(LLVM_SRC_PATH))
$(info Using LLVM_BUILD_PATH = $(LLVM_BUILD_PATH))
$(info Using LLVM_BIN_PATH = $(LLVM_BIN_PATH))
$(info -----------------------------------------------)

CXX := $(BINARY_DIR_PATH)/bin/clang++
CXXFLAGS := -fno-rtti -O0 -g

LLVM_CXXFLAGS := `$(LLVM_BIN_PATH)/llvm-config --cxxflags`
LLVM_LDFLAGS := `$(LLVM_BIN_PATH)/llvm-config --ldflags --libs --system-libs`

# List of Clang libraries to link. The proper -L will be provided by the call to llvm-config
# Note that I'm using -Wl,--{start|end}-group around the Clang libs; this is
# because there are circular dependencies that make the correct order difficult
# to specify and maintain. The linker group options make the linking somewhat
# slower, but IMHO they're still perfectly fine for tools that link with Clang.
CLANG_LIBS := \
	-Wl,--start-group \
	-lclangAnalysis \
	-lclangAST \
	-lclangASTMatchers \
	-lclangBasic \
	-lclangDriver \
	-lclangSerialization \
	-lclangLex \
	-lclangParse \
	-lclangSema \
	-lclangFrontend \
	-lclangFrontendTool \
	-lclangRewrite \
	-lclangEdit \
	-lclangToolingCore \
	-lclangToolingRefactoring \
	-lclangTooling \
	-lclangToolingInclusions \
	-lclangFormat \
	-lclangTransformer \
	-Wl,--end-group


# where to put build artifacts.
BUILDDIR := build

.PHONY: all
all: make_builddir \
	$(BUILDDIR)/rewrite_cond

.PHONY: make_builddir
make_builddir:
	@test -d $(BUILDDIR) || mkdir $(BUILDDIR)

.PHONY: rewrite_cond
rewrite_cond: $(BUILDDIR)/rewrite_cond

$(BUILDDIR)/rewrite_cond: RewriteCond.cpp
	$(CXX) $(CXXFLAGS) $(LLVM_CXXFLAGS) $^ $(CLANG_LIBS) $(LLVM_LDFLAGS) -o $@

clean:
	rm -rf $(BUILDDIR)/*

format:
	find . -name "*.cpp" | xargs clang-format -style=file -i
