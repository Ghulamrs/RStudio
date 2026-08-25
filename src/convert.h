#ifndef EDITOR_CONVERT_H
#define EDITOR_CONVERT_H

#include <string>

#include "compile.h"

namespace editor {

// c2s, the C89 <-> Shalimar source converter, driven over the open file.
//
// Not a toolchain. A toolchain produces a program and the editor reads its
// complaints against the source that made it; this produces a *source file*
// in the other language, and what it says about the parts it could not carry
// belongs beside that file rather than in a diagnostic pane. So it is its own
// thing rather than a fifth ToolchainKind, and it is found the way the
// compilers are - beside the editor first, so that a converter shipped with
// this copy is the one this copy runs.

// What became of one conversion.
struct Conversion {
    bool ran = false;        // c2s was found and started
    bool ok = false;         // and it converted the whole file
    int status = 0;          // what it exited with: 0, 1 refused, 2 unreadable
    std::string produced;    // the file written, when one was
    std::string output;      // everything c2s said, markers and questions alike
};

// Where c2s is. The C2S environment variable wins, then a copy beside the
// editor, then nothing - and nothing is not an error until somebody asks for
// a conversion, which is why this answers with an empty string rather than
// complaining.
std::string findConverter();

// The file a conversion of `sourcePath` should be written to: the same name
// with the other language's extension, beside the original. Empty when the
// path has no extension this converter knows, which is the one case the
// caller has to turn away.
std::string convertedName(const std::string& sourcePath, bool toShalimar);

// Runs it. `toShalimar` picks the direction explicitly rather than letting
// c2s infer it from the extension, because the menu item the user pressed is
// a better answer than the suffix - it is exactly the file with the wrong
// suffix that the Language menu exists for.
Conversion convert(const std::string& converter, const std::string& sourcePath,
                   const std::string& outputPath, bool toShalimar,
                   LineSink sink = 0, void* context = 0);

}  // namespace editor

#endif
