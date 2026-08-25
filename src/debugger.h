#ifndef EDITOR_DEBUGGER_H
#define EDITOR_DEBUGGER_H

#include <cstddef>
#include <string>
#include <vector>

#include "process.h"
#include "project.h"
#include "toolchain.h"

namespace editor {

enum DebuggerKind {
    DebuggerNone = 0,
    DebuggerLldb,
    DebuggerGdb,
    DebuggerCdb
};

DebuggerKind dbg_here();
const char* dbg_name(DebuggerKind kind);
const char* dbg_program(DebuggerKind kind);

DebuggerKind dbg_for(ToolchainKind kind, const std::string& arch);

bool dbg_stopsItself(ToolchainKind kind);

struct DebugPlan {
    ToolchainKind kind;
    DebuggerKind engine;
    bool stopsItself;
    std::vector<std::string> blind;

    DebugPlan() : kind(ToolAuto), engine(DebuggerNone), stopsItself(false) {}

    bool possible() const { return stopsItself || engine != DebuggerNone; }
};

DebugPlan dbg_planFor(const Toolchain& tool, const std::vector<Part>& parts,
                      const std::string& arch);

std::string dbg_whyNot(ToolchainKind kind, const std::string& arch);

struct Stop {
    bool stopped;
    bool exited;
    int status;
    std::string file;
    size_t line;
    std::string function;
    std::string said;

    Stop() : stopped(false), exited(false), status(0), line(0) {}
};

struct Variable {
    std::string name;
    std::string type;
    std::string value;
};

struct StackFrame {
    std::string function;
    std::string file;
    size_t line;

    StackFrame() : line(0) {}
};

struct Watch {
    std::string expression;
    std::string value;
    bool ok;

    Watch() : ok(false) {}
};

class Debugger {
public:
    Debugger();
    ~Debugger();

    bool start(DebuggerKind kind, const std::string& executable,
               const std::string& program = std::string());
    bool running() const { return kind_ != DebuggerNone && child_.running(); }
    DebuggerKind kind() const { return kind_; }
    void stop();

    bool breakAt(const std::string& file, size_t line);
    bool clearBreakpoints();

    Stop run();
    Stop resume();
    Stop stepOver();
    Stop stepInto();
    Stop stepOut();

    std::vector<Variable> locals();

    std::vector<StackFrame> frames();

    bool selectFrame(size_t which);

    bool setVariable(const std::string& name, const std::string& value,
                     std::string* said = 0);

    std::string evaluate(const std::string& expression, bool* ok = 0);

    void addWatch(const std::string& expression);
    void setWatch(size_t which, const std::string& expression);
    void removeWatch(size_t which);
    const std::vector<Watch>& watches() const { return watches_; }
    void readWatches();

    std::string ask(const std::string& command);

private:
    Debugger(const Debugger&);
    Debugger& operator=(const Debugger&);

    Stop afterMoving(const std::string& command);

    Stop afterStepping(const std::string& command);

    Process child_;
    DebuggerKind kind_;
    bool onConsole_;
    std::string executable_;

    Stop last_;

    std::vector<Watch> watches_;
};

Stop dbg_readStop(DebuggerKind kind, const std::string& said);

bool dbg_stoppedWithNoSource(const std::string& said);

bool dbg_wentNowhere(DebuggerKind kind, const Stop& before, const Stop& after);

std::string dbg_withoutEscapes(const std::string& text);
std::string dbg_withoutEcho(const std::string& said, const std::string& asked,
                            const std::string& marker);

std::string dbg_programOutput(DebuggerKind kind, const std::string& said);

std::vector<Variable> dbg_readVariables(DebuggerKind kind, const std::string& said);

std::vector<StackFrame> dbg_readFrames(DebuggerKind kind, const std::string& said);

std::string dbg_frameLine(const StackFrame& frame, bool looking = false);
size_t dbg_frameOnLine(const std::vector<StackFrame>& stack, const std::string& line);

std::string dbg_variableLine(const Variable& variable);
size_t dbg_variableOnLine(const std::vector<Variable>& locals, const std::string& line);

std::string dbg_watchLine(const Watch& watch);
size_t dbg_watchOnLine(const std::vector<Watch>& watches, const std::string& line);

std::string dbg_readValue(DebuggerKind kind, const std::string& said);

std::string dbg_lookingAt(const StackFrame& frame);

std::string dbg_stopLine(const std::string& file, size_t line, const std::string& function);

}

#endif
