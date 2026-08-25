#ifndef EDITOR_ABOUT_H
#define EDITOR_ABOUT_H

#include <string>
#include <vector>

namespace editor {

namespace about {

const char* name();
const char* version();

std::vector<std::string> lines();

}
}

#endif
