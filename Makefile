# RStudio - an editor that drives the cc1 compiler and shc. RStudio is the
# terminal half and the window is RStudio.exe on Windows; this builds the
# terminal one. .exe on every machine, not only Windows: the three programs in
# this family carry one name each wherever they are.
#
# The binaries were called ed1 and ed1gui until 2026-08-22, on the grounds that
# the product name was what the pair was called and the binaries kept their own
# names. That is reversed: one name, everywhere.
#
# Everything except one file is ordinary C++14 and builds anywhere. The
# exception is the terminal, and even that is smaller than it looks: Windows 10
# and later understand the same escape sequences as a Unix terminal once
# ENABLE_VIRTUAL_TERMINAL_PROCESSING is set, so src/terminal_win.cpp differs
# from src/terminal.cpp only in how the console is put into raw mode. The
# drawing, the key decoding and the status bar are the same code on both.
#
# On Windows with MSVC, use build.bat instead - it calls cl directly, since
# that machine has no make.

UNAME_S := $(shell uname -s 2>/dev/null)

ifeq ($(origin CXX),default)
  ifeq ($(UNAME_S),Darwin)
    CXX := clang++
  else
    CXX := g++
  endif
endif

# C++14, not because nothing newer works here but because cc1 is C++14 and the
# arena it is developed in holds itself to what it compiles. That is why
# src/path.cpp exists: <filesystem> is C++17.
CXXFLAGS := -std=c++14 -Wall -Wextra -Werror -pedantic -O2

# MSYS and MinGW report themselves here, and want the Windows console.
ifneq (,$(findstring MINGW,$(UNAME_S)))
  TERM_SRC := src/terminal_win.cpp
else ifneq (,$(findstring MSYS,$(UNAME_S)))
  TERM_SRC := src/terminal_win.cpp
else
  TERM_SRC := src/terminal.cpp
endif

# Two front ends over one core, and this is where that split is written down.
# CORE_SRC is what both of them compile: every rule the editor has, and none of
# the drawing. The window compiles exactly this list plus its own two files, so
# tools/make-projects.py checks winforms/RStudioGui.vcxproj against it - that
# project is kept by hand, and a file added here and forgotten there is a link
# error on the one machine that builds the window and nowhere else.
CORE_SRC := src/buffer.cpp src/compile.cpp \
       src/indent.cpp src/syntax.cpp \
       src/toolchain.cpp src/json.cpp src/project.cpp src/find.cpp \
       src/utf8.cpp src/workspace.cpp src/symbols.cpp src/demangle_win.cpp \
       src/path.cpp src/process.cpp src/debugger.cpp src/settings.cpp src/about.cpp

# The terminal's own half. src/help.cpp is here rather than in the core because
# only this front end shows the manual - the window's Help menu has Keys and
# About and no Contents.
#
# TERMINAL_SRC and TERM_SRC above are different things: that one is a single
# file, which of the two terminals this machine has.
TERMINAL_SRC := src/main.cpp src/editor.cpp src/menu.cpp src/tree.cpp \
       src/help.cpp \
       src/terminal_common.cpp \
       $(TERM_SRC)

SRC := $(CORE_SRC) $(TERMINAL_SRC)

# The Shalimar half lives apart from the three DWARF debuggers on purpose: a
# Shalimar program stops itself, so nothing here has anything to say to gdb,
# lldb or cdb, and src/debugger.cpp has nothing to say to it.
SHM_SRC := src/shalimar/channel.cpp src/shalimar/session.cpp

# The objects go under obj/ rather than beside the sources they came from,
# so that a listing of src/ is the code and nothing else.
OBJDIR := obj
OBJ := $(patsubst src/%.cpp,$(OBJDIR)/%.o,$(SRC) $(SHM_SRC))

# Where the finished program goes. `.` is this directory, which is what every
# suite and script here already expects, so a plain `make` is unchanged. The
# workspace build names one directory and has all three programs built into
# it - the editor and the two compilers it drives - so that what RStudio finds
# beside itself is what was just built, rather than what somebody remembered
# to copy.
BINDIR ?= .
EDITOR := $(BINDIR)/RStudio.exe

# Which Shalimar runtime this machine's shc builds, spelled the same way
# Compiler-S/Makefile spells it. Named here because `confirm` below has to ask
# for the archive by name, and a glob would pass on a directory holding some
# other machine's.
ifeq ($(UNAME_S),Darwin)
  SHM_TARGET ?= arm64-darwin
else
  SHM_TARGET ?= x86_64-linux
endif

$(EDITOR): $(OBJ)
	@mkdir -p $(BINDIR)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ)

# One rule for both, making whatever directory the object goes in. A second
# pattern rule for the subdirectory would be ambiguous with this one - the
# stem 'shalimar/channel' matches it too, and which of the two make prefers is
# not something to have to know.
$(OBJDIR)/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -c -o $@ $<

# Which headers each object depends on is the compiler's answer, not a list
# kept by hand here. The list that used to be here had gone stale: editor.cpp
# had come to include debugger.h and the line for editor.o did not say so, so a
# member added to Debugger rebuilt debugger.o and not editor.o. One binary then
# held two ideas of where that class's members were, and it segfaulted - after
# a run of tests that had looked like a parser bug. A clean build hid it, which
# is the worst thing a bug of this kind can do.
-include $(OBJ:.o=.d)

# The two pieces with a contract: the layout rules, and the reading of cc1's
# diagnostic - which has to cope with a Windows path whose drive letter is
# followed by a colon that is not a separator.
test: tests/test
	./tests/test

tests/test: tests/test.cpp src/compile.cpp src/indent.cpp src/syntax.cpp \
            src/toolchain.cpp src/json.cpp src/project.cpp src/find.cpp \
       src/utf8.cpp src/workspace.cpp src/symbols.cpp src/demangle_win.cpp \
            src/path.cpp src/process.cpp src/debugger.cpp src/settings.cpp src/about.cpp src/help.cpp \
            src/buffer.cpp \
            winforms/bridge.cpp winforms/bridge.h src/compile.h src/indent.h src/syntax.h \
            src/json.h src/project.h src/path.h src/buffer.h
	$(CXX) $(CXXFLAGS) -Isrc -Iwinforms -o $@ tests/test.cpp winforms/bridge.cpp \
	    src/compile.cpp src/indent.cpp \
	    src/syntax.cpp src/toolchain.cpp src/json.cpp src/project.cpp src/find.cpp \
       src/utf8.cpp src/workspace.cpp src/symbols.cpp src/demangle_win.cpp \
	    src/path.cpp src/process.cpp src/debugger.cpp src/settings.cpp src/about.cpp src/help.cpp \
	    src/buffer.cpp $(SHM_SRC)

# The other half of the checking: the editor itself, driven by keystrokes.
# CC1 and SHC name compilers for the build cases; without them those cases
# are skipped rather than failed.
session: tests/session $(EDITOR)
	CC1="$(CC1)" SHC="$(SHC)" ./tests/session $(EDITOR)

tests/session: tests/session.cpp src/path.cpp src/path.h
	$(CXX) $(CXXFLAGS) -Isrc -o $@ tests/session.cpp src/path.cpp

check: test session

# ---- what RStudio drives, and the confirmation that it is there -------------
#
# The editor links against none of these. It *runs* them, and it finds them
# beside itself - path::besideProgram, asked before PATH, so that a compiler
# shipped with this copy is the one this copy runs. Which means "built" and
# "usable" are two different states, and until now nothing checked the second.
#
# That gap is not hypothetical: a bin/ was assembled holding shc.exe without
# the runtime archives shc links, and every build stayed green and every suite
# passed, because a suite that cannot find a compiler skips its cases and says
# so quietly. The failure waited for somebody to open a Shalimar file and press
# Run, and then read as a broken compiler rather than an incomplete directory.
#
# Both archives are named, not only the release one. Debug is the editor's
# default configuration and links the other file, so checking one of the two
# would confirm exactly the half that was not about to be used.
DEPENDENCIES := cc1.exe shc.exe \
       lib/shmrt-$(SHM_TARGET).a lib/shmrt-$(SHM_TARGET)-debug.a

confirm: $(EDITOR)
	@missing=0; \
	for dep in $(DEPENDENCIES); do \
	    if [ -e "$(BINDIR)/$$dep" ]; then \
	        echo "  ok       $$dep"; \
	    else \
	        echo "  MISSING  $$dep"; \
	        missing=1; \
	    fi; \
	done; \
	if [ $$missing -ne 0 ]; then \
	    echo ""; \
	    echo "RStudio.exe is in $(BINDIR) without what it drives. Build the three"; \
	    echo "together with 'make -f workspace.mk', or name them with \$$CC1 and \$$SHC."; \
	    exit 1; \
	fi; \
	echo ""; \
	echo "RStudio.exe and everything it drives are in $(BINDIR)"

# The Xcode project is generated from the source list above rather than kept by
# hand, so it cannot fall behind it. Run this after adding or removing a file.
xcodeproj:
	python3 tools/make-projects.py

# What gets used, as against what gets built. The binaries land beside their
# objects because that is where a build puts them; this is where the product
# lives - one directory holding what you would actually run, away from the
# project space it was compiled in.
#
# PRODUCT names it, so a different one can be asked for without editing this.
PRODUCT ?= $(HOME)/cc1-studio

# `confirm` and not `$(EDITOR)`, for the reason build.bat gives on its own
# product rule: an editor without its compilers is not a product, it is half of
# one that fails at the first Ctrl-B. This rule shipped the editor alone until
# 2026-08-24 while the Windows one had already been fixed, so a Mac or Linux
# product could not build anything it was given. Depending on `confirm` means
# the same list that guards the build guards the product, and a missing
# compiler stops this rather than being discovered by the person reviewing it.
product: confirm
	mkdir -p "$(PRODUCT)/bin/lib" "$(PRODUCT)/examples"
	cp $(EDITOR) "$(PRODUCT)/bin/"
	cp $(BINDIR)/cc1.exe $(BINDIR)/shc.exe "$(PRODUCT)/bin/"
# Into bin/lib/ rather than anywhere tidier, because that is where shc looks:
# beside its own binary. Both archives, debug included - see DEPENDENCIES.
	cp $(BINDIR)/lib/shmrt-$(SHM_TARGET).a \
	   $(BINDIR)/lib/shmrt-$(SHM_TARGET)-debug.a "$(PRODUCT)/bin/lib/"
	cp README.md "$(PRODUCT)/"
# All three languages, and the headers. Copying only *.c and *.cpp shipped
# table.cpp and vector3.cpp without the headers they include, so neither would
# compile on arrival, and left out gcd.shl, primes.shl and rotmat.shl
# altogether - which is every Shalimar program there is here, in the product
# whose third language is Shalimar. example.pro goes too, so that there is a
# project to open rather than only loose files.
	cp examples/*.c examples/*.h examples/*.cpp examples/*.shl examples/*.pro \
	   "$(PRODUCT)/examples/"
	@echo "RStudio is in $(PRODUCT)"

clean:
	rm -rf $(OBJDIR)
	rm -f $(EDITOR) tests/test tests/session

.PHONY: test session check confirm xcodeproj product clean
