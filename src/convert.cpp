#include "convert.h"

#include <cstdlib>

#include "path.h"

namespace editor {

// runCaptured is compile.cpp's, declared in compile.h. Every program this
// editor runs goes through it - one place that joins stderr to stdout, feeds
// the console a line at a time, and gives back an exit status. A converter is
// a program like any other, so it goes there too rather than growing a second
// runner here.

std::string findConverter() {
    const char* fromEnv = std::getenv("C2S");
    if (fromEnv && *fromEnv) return fromEnv;

    // c2s.exe first, which is what Converter-C2S builds on every machine,
    // then bare c2s - the same two names in the same order as cc1 and shc,
    // for the same reason.
    std::string beside = path::besideProgram("c2s.exe");
    if (beside.empty()) beside = path::besideProgram("c2s");
    return beside;
}

namespace {

// The last dot of the last component, or npos. A dot in a directory name is
// not an extension of the file.
size_t extensionAt(const std::string& path) {
    const size_t dot = path.find_last_of('.');
    if (dot == std::string::npos) return std::string::npos;
    const size_t slash = path.find_last_of("/\\");
    if (slash != std::string::npos && dot < slash) return std::string::npos;
    return dot;
}

}  // namespace

std::string convertedName(const std::string& sourcePath, bool toShalimar) {
    if (sourcePath.empty()) return std::string();
    const size_t dot = extensionAt(sourcePath);
    const std::string stem =
        dot == std::string::npos ? sourcePath : sourcePath.substr(0, dot);

    // .shm going out, .c coming back. Shalimar's other spelling, .shl, is
    // read but not written: one of the two has to be the name this editor
    // makes, and .shm is the one Compiler-S and the phone app both use.
    return stem + (toShalimar ? ".shm" : ".c");
}

Conversion convert(const std::string& converter, const std::string& sourcePath,
                   const std::string& outputPath, bool toShalimar,
                   LineSink sink, void* context) {
    Conversion result;
    if (converter.empty() || sourcePath.empty() || outputPath.empty()) {
        return result;
    }

    // Quoted, both of them, because a path with a space in it is ordinary on
    // two of the three machines this runs on.
    const std::string command = "\"" + converter + "\" " +
                                (toShalimar ? "--to-shalimar " : "--to-c ") +
                                "\"" + sourcePath + "\" -o \"" + outputPath + "\"";

    result.status = runCaptured(command, result.output, sink, context);
    result.ran = result.status >= 0;

    // c2s exits 0 only when the whole file came across. 1 means it refused,
    // or that it wrote the file with #BEYOND SHALIMAR markers in it and is
    // saying so - and in that second case the file is there and is worth
    // opening, because the markers are the work left to do and they quote
    // the C they stand for. 2 means it could not read or write at all.
    result.ok = result.ran && result.status == 0;
    if (result.ran && result.status != 2) result.produced = outputPath;
    return result;
}

}  // namespace editor
