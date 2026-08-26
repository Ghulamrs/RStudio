#include "about.h"
#include "path.h"

#include <cstdio>
#include <string>

#if defined(_WIN32)
#define POPEN  _popen
#define PCLOSE _pclose
#else
#define POPEN  popen
#define PCLOSE pclose
#endif

namespace editor {
namespace about {

const char* name() { return "RStudio"; }

const char* version() { return "1.2"; }

namespace {

// **The compilers are asked rather than listed.** Writing their numbers here
// would be writing them twice, and this copy would be the one that went stale:
// the editor does not build them and has no way to know when one moved.
//
// It also answers the question a reader of this box actually has, which is not
// "what was this built against" but "what is it driving now". A copy standing
// beside a compiler from another release is exactly the case worth seeing, and
// a hard-coded string would hide it.
//
// Absence is not an error. The editor finds what it drives beside itself, and
// a copy shipped on its own is an ordinary state - so the row says so rather
// than disappearing. A missing line is harder to notice than one that reports
// what is missing.
std::string askVersion(const std::string& program) {
    const std::string found = path::besideProgram(program);
    if (found.empty()) return std::string();

    // Quoted, because a path may hold a space. `--version` is the flag all
    // three answer, and each writes one line and stops.
    const std::string command = "\"" + found + "\" --version 2>&1";
    FILE* pipe = POPEN(command.c_str(), "r");
    if (!pipe) return std::string();

    char buffer[256];
    std::string line;
    if (std::fgets(buffer, sizeof buffer, pipe)) line = buffer;
    PCLOSE(pipe);

    while (!line.empty() && (line[line.size() - 1] == '\n' || line[line.size() - 1] == '\r'))
        line.resize(line.size() - 1);
    return line;
}

void pad(std::string& row, std::size_t width) {
    while (row.size() < width) row += ' ';
}

std::string cell(const std::string& program) {
    const std::string answer = askVersion(program);
    return answer.empty() ? program + " - not beside this program" : answer;
}

// **All three across one line.** One per line pushed the last line of the box
// off the bottom of the panel, which shows seven and does not scroll for a
// dialog - and the box was seven before the compilers were added to it, so
// there was never room for three more. Three short answers fit the width with
// room to spare, and a row that says "not beside this program" is longer but
// only ever appears where the others are absent too.
void tools(std::vector<std::string>& said,
           const std::string& a, const std::string& b, const std::string& c) {
    std::string row = "  " + cell(a);
    pad(row, 22); row += cell(b);
    pad(row, 42); row += cell(c);
    said.push_back(row);
}

}

std::vector<std::string> lines() {
    std::vector<std::string> said;
    said.push_back(std::string(name()) + " " + version());
    said.push_back("A terminal editor and a window, over one core.");
    // Asked one at a time, in the order the menus offer the languages. The
    // list replaced a sentence naming the compilers, which said less: this
    // says which ones are actually here, and which are not.
    said.push_back("the compilers it drives, as they answer for themselves:");
    tools(said, "cc1.exe", "shc.exe", "c2s.exe");
    said.push_back("");
    said.push_back("Copyright (c) 2026 G. R. Akhtar");
    said.push_back("Islamabad, Pakistan");
    return said;
}

}
}
