#include "find.h"

namespace editor {

Match findNext(const std::vector<std::string>& lines, const std::string& needle,
               size_t row, size_t col) {
    Match match;
    if (needle.empty() || lines.empty()) return match;
    if (row >= lines.size()) row = lines.size() - 1;

    for (size_t step = 0; step <= lines.size(); ++step) {
        size_t at = (row + step) % lines.size();
        size_t from = (step == 0) ? col : 0;
        if (from > lines[at].size()) continue;

        size_t found = lines[at].find(needle, from);

        if (step == lines.size() && found != std::string::npos && found >= col)
            return match;

        if (found != std::string::npos) {
            match.found = true;
            match.row = at;
            match.col = found;
            return match;
        }
    }
    return match;
}

Match findPrevious(const std::vector<std::string>& lines, const std::string& needle,
                   size_t row, size_t col) {
    Match match;
    if (needle.empty() || lines.empty()) return match;
    if (row >= lines.size()) row = lines.size() - 1;

    for (size_t step = 0; step <= lines.size(); ++step) {
        size_t at = (row + lines.size() - (step % lines.size())) % lines.size();

        size_t upTo;
        if (step == 0) {
            if (col == 0) continue;
            upTo = col - 1;
        } else {
            upTo = lines[at].size();
        }

        size_t found = lines[at].rfind(needle, upTo);
        if (found == std::string::npos) continue;
        if (step == lines.size() && found < col) return match;

        match.found = true;
        match.row = at;
        match.col = found;
        return match;
    }
    return match;
}

size_t replaceAll(std::vector<std::string>& lines, const std::string& needle,
                  const std::string& with) {
    if (needle.empty()) return 0;

    size_t count = 0;
    for (size_t i = 0; i < lines.size(); ++i) {
        std::string& line = lines[i];
        size_t at = 0;
        for (;;) {
            size_t found = line.find(needle, at);
            if (found == std::string::npos) break;
            line.replace(found, needle.size(), with);

            at = found + with.size();
            ++count;
        }
    }
    return count;
}

}
