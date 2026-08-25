#ifndef EDITOR_PROCESS_H
#define EDITOR_PROCESS_H

#include <string>

namespace editor {

class Process {
public:
    Process();
    ~Process();

    bool start(const std::string& command);

    bool startOnConsole(const std::string& command);

    bool running() const { return running_; }

    bool say(const std::string& line);

    std::string readUntil(const std::string& marker, bool* found = 0,
                          int timeoutMs = 30000);

    void stop();

private:
    Process(const Process&);
    Process& operator=(const Process&);

    struct Held;
    Held* held_;

    bool running_;
    std::string pending_;
};

}

#endif
