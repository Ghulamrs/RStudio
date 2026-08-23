#ifndef EDITOR_TREE_H
#define EDITOR_TREE_H

#include <cstddef>
#include <set>
#include <string>
#include <vector>

#include "project.h"

namespace editor {

// The project down the left. Directories are read through path.h, which is
// opendir on one machine and FindFirstFile on the other - this is C++14, where
// there is no <filesystem> to do it for us. The two spellings sit in one file
// behind one set of functions, so there is a single place they can drift apart
// and it is small enough to check.
struct TreeEntry {
    std::string path;
    std::string name;
    bool directory = false;
    bool group = false;    // a project's group, which is not a directory

    // A row in the "Open files" section rather than in the project. It is
    // still a file you can press enter on; what it is not is somewhere a file
    // can be *put*, which is why groupUnderCursor has to be able to tell.
    bool session = false;
    bool open = false;
    int depth = 0;
};

class Tree {
public:
    Tree();

    void setRoot(const std::string& path);

    // Shows the project's groups instead of the directory. A group is not a
    // directory and nothing on disk matches it, which is the point: it is the
    // project's own arrangement of the same files.
    // The project's groups, with the files that are open listed above them.
    //
    // Both, because they answer different questions and a pane that answers
    // only one of them is the pane people report as broken: the project says
    // what the work *is* and does not move when you open a file, and the open
    // list says what you are *doing* and is the only part that can.
    void showProject(const Project& project, const std::vector<std::string>& open);

    // The files that are open, which is what the pane shows when there is no
    // project to show instead. Paths, in the order they were opened; a
    // document that has never been saved has no path and is not one of them.
    void showOpenFiles(const std::vector<std::string>& paths);

    // Nothing at all, and nothing read from the disk to fill the gap.
    void clear();

    const std::string& root() const { return root_; }
    const std::string& error() const { return error_; }

    const std::vector<TreeEntry>& entries() const { return entries_; }
    size_t size() const { return entries_.size(); }

    // Opens a directory, or closes one already open. A file is left to the
    // caller, which is the only place that knows what opening a file means.
    void toggle(size_t index);
    void reread();

    // Where a path sits in the list, or size() when it is not shown.
    size_t find(const std::string& path) const;

private:
    void gather(const std::string& dir, int depth);

    // What the pane is a view of. This was a bool - the project, or the
    // directory - and the two states it has grown are both things closing a
    // project needs. A pane with no project used to fall back to listing
    // whichever directory it was standing in, which is a different answer to
    // "what am I working on" than the honest one, which is "nothing".
    //
    // reread() is the reason this is not worked out from the other fields: it
    // re-reads the disk, and it must not do that over a view the disk did not
    // produce.
    enum Showing { ShowingDirectory, ShowingProject, ShowingOpenFiles, ShowingNothing };
    Showing showing_;

    std::string root_;
    std::string error_;
    std::vector<TreeEntry> entries_;
    std::set<std::string> opened_;
};

}  // namespace editor

#endif
