#ifndef EDITOR_COMPILE_H
#define EDITOR_COMPILE_H

#include <cstddef>
#include <string>
#include <vector>

#include "project.h"
#include "toolchain.h"

namespace editor {

struct Diagnostic {
    bool present = false;
    std::string file;
    size_t line = 0;
    size_t col = 0;
    std::string message;
};

struct Build {
    bool ok = false;
    Diagnostic diag;
    std::string output;
    std::vector<std::string> asmLines;
};

extern const char* const kArches[3];

typedef void (*LineSink)(void* context, const std::string& line);

int runCaptured(const std::string& command, std::string& output,
                LineSink sink = 0, void* context = 0);

Build build(const Toolchain& tool, ToolchainKind kind, const std::string& sourcePath,
            Language lang, const std::string& arch, Configuration config,
            LineSink sink = 0, void* context = 0);

struct Ran {
    bool built = false;
    bool ran = false;
    int status = 0;
    Diagnostic diag;
    std::string output;
};

struct Built {
    bool ok;
    Diagnostic diag;
    std::string output;
    std::string program;
    std::vector<std::string> leftovers;

    Built() : ok(false) {}
};

Built buildProgram(const Toolchain& tool, ToolchainKind kind, const std::string& sourcePath,
                   Language lang, const std::string& arch, Configuration config,
                   LineSink sink = 0, void* context = 0);

Built buildTarget(const Toolchain& tool, ToolchainKind kind,
                  const std::vector<std::string>& sources, Language lang,
                  const std::string& arch, Configuration config,
                  const std::string& program, LineSink sink = 0, void* context = 0);

Built buildParts(const Toolchain& tool, const std::vector<Part>& parts,
                 const std::string& arch, Configuration config,
                 const std::string& program, LineSink sink = 0, void* context = 0);

Ran runBuilt(const std::string& program, LineSink sink = 0, void* context = 0);

void removeProgram(const Built& built);

Ran runProgram(const Toolchain& tool, ToolchainKind kind, const std::string& sourcePath,
               Language lang, const std::string& arch, Configuration config,
               LineSink sink = 0, void* context = 0);

Diagnostic parseDiagnostic(const std::string& text, const std::string& source = std::string());

}

#endif
