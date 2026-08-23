#include "tree.h"

#include <algorithm>

#include "path.h"

namespace editor {

namespace {

// Directories that are build output or version control. Showing them would
// bury the four files anyone is actually looking for.
bool skip(const std::string& name) {
    if (name.empty()) return true;
    if (name[0] == '.') return true;          // .git, .vs, and the rest
    return name == "obj" || name == "build" || name == "x64" || name == "Debug" ||
           name == "Release" || name == "node_modules";
}

}  // namespace

Tree::Tree() : showing_(ShowingDirectory) {}

void Tree::setRoot(const std::string& where) {
    root_ = path::absolute(where);
    showing_ = ShowingDirectory;
    opened_.clear();
    reread();
}

void Tree::showProject(const Project& project, const std::vector<std::string>& open) {
    showing_ = ShowingProject;
    root_ = project.root();
    error_.clear();
    entries_.clear();

    // What is open, first, and only when something is. An empty heading would
    // be a row spent saying nothing.
    if (!open.empty()) {
        TreeEntry head;
        head.name = "Open files";
        head.path = "open:";      // no path can collide with it
        head.directory = true;
        head.group = true;
        head.session = true;
        head.depth = 0;
        head.open = true;         // never folded: it is the part that moves
        entries_.push_back(head);

        for (size_t i = 0; i < open.size(); ++i) {
            TreeEntry file;
            file.name = path::filename(open[i]);
            file.path = open[i];
            file.session = true;
            file.depth = 1;
            entries_.push_back(file);
        }
    }

    for (size_t i = 0; i < project.groups().size(); ++i) {
        const Group& group = project.groups()[i];

        TreeEntry head;
        head.name = group.name;
        // Marked with a name no path can collide with, so a group called src
        // and a directory called src do not open and close together.
        head.path = "group:" + group.name;
        head.directory = true;
        head.group = true;
        head.depth = 0;
        head.open = opened_.count(head.path) > 0 || opened_.empty();
        entries_.push_back(head);

        if (!head.open) continue;

        // **The file's own name, not its path.** A project's list is written
        // relative to the root - `src/first.c` - and the pane used to show it
        // that way, so a project of any size read as a column of repeated
        // directory names with the one distinguishing word at the end of each.
        // Worse, it says something untrue about where the file is: `src/` in
        // the pane looks like a directory of whatever tree you are standing
        // in, and the user's reading of it was that the file lived inside
        // RStudio's own source.
        //
        // The grouping is what a project has instead of directories - that is
        // the whole idea of a group - so the path adds nothing the pane was
        // already saying. Where the file really is stays one keystroke away:
        // `path` here is the absolute one, and the status bar names it in full
        // the moment the file is opened.
        for (size_t j = 0; j < group.files.size(); ++j) {
            TreeEntry file;
            file.name = path::filename(group.files[j]);
            file.path = project.absolute(group.files[j]);
            file.depth = 1;
            entries_.push_back(file);
        }
    }
}

// One flat list, in the order they were opened. No groups: there is no project
// to have grouped them, and inventing a heading would be the pane claiming an
// arrangement that nothing on disk agrees with.
void Tree::showOpenFiles(const std::vector<std::string>& paths) {
    showing_ = ShowingOpenFiles;
    error_.clear();
    entries_.clear();

    for (size_t i = 0; i < paths.size(); ++i) {
        TreeEntry file;
        file.path = paths[i];
        file.name = path::filename(paths[i]);
        file.depth = 0;
        entries_.push_back(file);
    }
}

void Tree::clear() {
    showing_ = ShowingNothing;
    error_.clear();
    entries_.clear();
}

void Tree::reread() {
    // Only a directory view is re-read from the disk. Every other view was
    // built from something else - the project file, or the list of open
    // documents - and re-reading would quietly replace it with a listing.
    if (showing_ != ShowingDirectory) return;
    entries_.clear();
    error_.clear();
    if (root_.empty()) return;
    gather(root_, 0);
}

void Tree::gather(const std::string& dir, int depth) {
    bool readable = false;
    std::vector<path::Entry> found = path::entries(dir, &readable);
    if (!readable) {
        if (depth == 0) error_ = "cannot read " + dir;
        return;
    }

    // Directories first, then files, each in name order - the order a person
    // reads a project in, rather than the order the filesystem hands them over.
    std::sort(found.begin(), found.end(),
              [](const path::Entry& a, const path::Entry& b) {
                  if (a.directory != b.directory) return a.directory;
                  return a.name < b.name;
              });

    for (size_t i = 0; i < found.size(); ++i) {
        std::string name = found[i].name;
        if (skip(name)) continue;

        bool isDir = found[i].directory;

        TreeEntry entry;
        entry.path = path::join(dir, name);
        entry.name = name;
        entry.directory = isDir;
        entry.depth = depth;
        entry.open = isDir && opened_.count(entry.path) > 0;
        entries_.push_back(entry);

        if (entry.open) gather(entry.path, depth + 1);
    }
}

void Tree::toggle(size_t index) {
    if (index >= entries_.size()) return;
    TreeEntry entry = entries_[index];
    if (!entry.directory) return;

    if (entry.group) {
        // Groups start open, so the set holds the ones that are, and closing
        // the first one has to put every other group into it first.
        if (opened_.empty())
            for (size_t i = 0; i < entries_.size(); ++i)
                if (entries_[i].group) opened_.insert(entries_[i].path);
        if (opened_.count(entry.path))
            opened_.erase(entry.path);
        else
            opened_.insert(entry.path);
        return;   // the caller shows the project again
    }

    if (opened_.count(entry.path))
        opened_.erase(entry.path);
    else
        opened_.insert(entry.path);
    reread();
}

size_t Tree::find(const std::string& where) const {
    std::string want = path::absolute(where);
    for (size_t i = 0; i < entries_.size(); ++i)
        if (entries_[i].path == want) return i;
    return entries_.size();
}

}  // namespace editor
