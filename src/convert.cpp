#include "convert.h"

#include <cstdlib>

#include "path.h"

namespace editor {

std::string findConverter() {
    const char* fromEnv = std::getenv("C2S");
    if (fromEnv && *fromEnv) return fromEnv;

    std::string beside = path::besideProgram("c2s.exe");
    if (beside.empty()) beside = path::besideProgram("c2s");
    return beside;
}

namespace {

size_t extensionAt(const std::string& path) {
    const size_t dot = path.find_last_of('.');
    if (dot == std::string::npos) return std::string::npos;
    const size_t slash = path.find_last_of("/\\");
    if (slash != std::string::npos && dot < slash) return std::string::npos;
    return dot;
}

}

std::string convertedName(const std::string& sourcePath, bool toShalimar) {
    if (sourcePath.empty()) return std::string();
    const size_t dot = extensionAt(sourcePath);
    const std::string stem =
        dot == std::string::npos ? sourcePath : sourcePath.substr(0, dot);

    return stem + (toShalimar ? ".shm" : ".c");
}

Conversion convert(const std::string& converter, const std::string& sourcePath,
                   const std::string& outputPath, bool toShalimar,
                   LineSink sink, void* context) {
    Conversion result;
    if (converter.empty() || sourcePath.empty() || outputPath.empty()) {
        return result;
    }

    const std::string command = "\"" + converter + "\" " +
                                (toShalimar ? "--to-shalimar " : "--to-c ") +
                                "\"" + sourcePath + "\" -o \"" + outputPath + "\"";

    result.status = runCaptured(command, result.output, sink, context);
    result.ran = result.status >= 0;

    result.ok = result.ran && result.status == 0;
    if (result.ran && result.status != 2) result.produced = outputPath;
    return result;
}

}
