#include "settings.h"

#include <cctype>
#include <cstdio>

#include "json.h"
#include "path.h"

namespace editor {
namespace settings {

// ~/.rstudio/config.json - a directory of its own rather than a loose dotfile
// in the home root.
//
// **In the user's path and not beside the binary, and that is not a
// preference.** Three things decide it. The binary's directory is build output
// here - x64\Release\ or the repository root - and is deleted and rebuilt
// routinely, taking anything kept in it. There are several copies of this
// editor on each machine, so settings beside one of them would mean each
// remembering a different last project. And a real install is often not
// writable without administrator rights, where a home directory always is.
std::string fileName() {
    std::string home = path::homeDir();
    if (home.empty()) return std::string();
    return path::join(path::join(home, ".rstudio"), "config.json");
}

// The two names this had before, newest first. Read when the current one is
// not there, and retired on the first write - see writeAll. Kept in a list
// because there have now been two renames in one day and a third would
// otherwise mean touching three functions.
const char* const kFormerNames[2] = {".rstudioconfig.json", ".ed1config.json"};

std::string formerFileName() {
    std::string home = path::homeDir();
    if (home.empty()) return std::string();

    for (size_t i = 0; i < 2; ++i) {
        std::string old = path::join(home, kFormerNames[i]);
        if (path::exists(old)) return old;
    }
    // None on disk: name the most recent one, which is what a message about
    // the old name should say.
    return path::join(home, kFormerNames[0]);
}

namespace {

// The one to read: the current name, or the name it had before 2026-08-23 when
// that is what is on this machine, or nothing when the editor has never run
// here. Reading the old one matters more than it looks - it holds which
// project you were last in, which is the whole of what a first run without
// arguments has to go on.
std::string toRead() {
    std::string now = fileName();
    if (!now.empty() && path::exists(now)) return now;

    std::string home = path::homeDir();
    if (home.empty()) return std::string();

    for (size_t i = 0; i < 2; ++i) {
        std::string old = path::join(home, kFormerNames[i]);
        if (path::exists(old)) return old;
    }
    return std::string();
}

}  // namespace

namespace {

// The file as it stands, or an empty object. Everything written here is
// read-modify-write: the file holds more than one thing now, and a writer that
// builds a fresh object throws away whatever it did not know about.
// A pointer made once and never destroyed, not a string. A function-local
// static with a destructor registers an atexit handler, and in the mixed-mode
// binary that corrupts the heap - the hazard debugger.cpp's dbg_program was
// written around, and this file is linked into the same binary.
std::string* moved = 0;
bool movedTo() { return moved != 0; }
void rememberMoved(const std::string& where) { moved = new std::string(where); }

bool writeAll(const Json& root);

Json readAll() {
    std::string where = toRead();
    if (where.empty()) return Json::object();

    FILE* in = std::fopen(where.c_str(), "rb");
    if (!in) return Json::object();
    std::string text;
    char chunk[1024];
    size_t got;
    while ((got = std::fread(chunk, 1, sizeof chunk, in)) > 0) text.append(chunk, got);
    std::fclose(in);

    std::string why;
    Json root = Json::parse(text, why);
    if (why.empty() && root.is(Json::Object)) return root;

    // Unreadable. Three things, in this order: keep the old one, put a good
    // one in its place, and let the front end say so.
    //
    // Only when there was something in it, though: an empty file is nobody's
    // work, and renaming that would leave litter for no reason.
    bool anything = false;
    for (size_t i = 0; i < text.size(); ++i)
        if (!std::isspace(static_cast<unsigned char>(text[i]))) { anything = true; break; }

    if (anything && !movedTo()) {
        std::string aside = where + ".error";
        path::remove(aside);          // the newest bad one is the interesting one
        if (path::rename(where, aside)) {
            rememberMoved(aside);
            // A fresh one now rather than at the next setting changed, so that
            // what is on disk always matches what the editor believes, and so
            // that a file exists to be written to at all.
            writeAll(Json::object());
        }
    }
    return Json::object();
}

bool writeAll(const Json& root) {
    std::string where = fileName();
    if (where.empty()) return false;

    // ~/.rstudio may not exist yet, and fopen will not make it.
    path::makeDirectories(path::parent(where));

    FILE* out = std::fopen(where.c_str(), "wb");
    if (!out) return false;
    std::string text = root.write();
    std::fwrite(text.data(), 1, text.size(), out);
    std::fclose(out);

    // Written under the current name, so the one it had before is finished -
    // and it is removed rather than left, because a settings file that is on
    // disk and silently not read is worse than no file at all. Everything it
    // held came through readAll and has just been written to the new one, so
    // there is nothing in it to lose.
    //
    // Unlike a project's RStudio.json, which is somebody's file in somebody's
    // directory and is saved back under whichever name it was found by. This
    // one is the editor's own bookkeeping in a home directory, and migrating
    // it is the editor's business.
    std::string home = path::homeDir();
    for (size_t i = 0; !home.empty() && i < 2; ++i) {
        std::string old = path::join(home, kFormerNames[i]);
        if (old != where && path::exists(old)) path::remove(old);
    }
    return true;
}

}  // namespace

bool plainFrame() { return readAll().get("plain").boolean(false); }

bool rememberPlainFrame(bool plain) {
    Json root = readAll();
    root.set("plain", Json::fromBool(plain));
    return writeAll(root);
}

std::string setAside() {
    readAll();   // the moving happens there, on the first read of a bad file
    return moved ? *moved : std::string();
}

std::string codeFont() { return readAll().get("font").text(std::string()); }

bool rememberCodeFont(const std::string& described) {
    Json root = readAll();
    root.set("font", Json::fromText(described));
    return writeAll(root);
}

// Through readAll, like everything else here. It used to open the file itself
// - its own fopen, its own parse, the same twenty lines - and that copy is what
// made the rename to .rstudioconfig.json quietly lose the last project: readAll
// had learned to fall back to the old name and this had not. A second
// implementation of "read the settings" is a second thing to teach.
// What was open last: a project file by name, or - for anything remembered
// before named projects existed - the directory one was found in. Either is
// handed straight to Project::load, which takes both.
std::string lastProject() {
    std::string project = readAll().get("project").text("");
    if (project.empty() || !path::exists(project)) return std::string();
    return project;
}

bool rememberProject(const std::string& directory) {
    if (fileName().empty() || directory.empty()) return false;

    // Read, change one thing, write: this file holds more than the project
    // now, and building a fresh object here would throw the rest away.
    Json root = readAll();
    root.set("project", Json::fromText(path::absolute(directory)));

    // And through writeAll, for the same reason lastProject reads through
    // readAll: this had its own copy of the write, so the migration that
    // retires the old file never ran on the one path that always runs.
    return writeAll(root);
}

}  // namespace settings
}  // namespace editor
