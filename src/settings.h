#ifndef EDITOR_SETTINGS_H
#define EDITOR_SETTINGS_H

#include <string>

namespace editor {

namespace settings {

std::string fileName();

std::string formerFileName();

std::string lastProject();

bool rememberProject(const std::string& directory);

bool plainFrame();

std::string configuration();
bool rememberConfiguration(const std::string& which);

std::string codeFont();
bool rememberCodeFont(const std::string& described);

std::string setAside();
bool rememberPlainFrame(bool plain);

}
}

#endif
