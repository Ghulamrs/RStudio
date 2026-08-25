#ifndef EDITOR_FIND_H
#define EDITOR_FIND_H

#include <cstddef>
#include <string>
#include <vector>

namespace editor {

struct Match {
    bool found = false;
    size_t row = 0;
    size_t col = 0;
};

Match findNext(const std::vector<std::string>& lines, const std::string& needle,
               size_t row, size_t col);

Match findPrevious(const std::vector<std::string>& lines, const std::string& needle,
                   size_t row, size_t col);

size_t replaceAll(std::vector<std::string>& lines, const std::string& needle,
                  const std::string& with);

}

#endif
