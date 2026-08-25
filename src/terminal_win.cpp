
#include "terminal.h"

#include <windows.h>

namespace editor {

Terminal::Terminal()
    : in_(0), out_(0), inMode_(0), outMode_(0), codePage_(0), raw_(false), eof_(false) {

    codePage_ = GetConsoleOutputCP();
    SetConsoleOutputCP(CP_UTF8);

    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hIn == INVALID_HANDLE_VALUE || hOut == INVALID_HANDLE_VALUE) return;

    DWORD inMode = 0, outMode = 0;
    if (!GetConsoleMode(hIn, &inMode)) return;
    if (!GetConsoleMode(hOut, &outMode)) return;

    in_ = hIn;
    out_ = hOut;
    inMode_ = inMode;
    outMode_ = outMode;

    if (!applyModes()) return;

    raw_ = true;
}

bool Terminal::applyModes() {
    HANDLE hIn = (HANDLE)in_;
    HANDLE hOut = (HANDLE)out_;

    DWORD wantOut = outMode_ | ENABLE_VIRTUAL_TERMINAL_PROCESSING | ENABLE_PROCESSED_OUTPUT;
    if (!SetConsoleMode(hOut, wantOut)) return false;

    DWORD wantIn = inMode_;
    wantIn &= ~(DWORD)(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT |
                       ENABLE_MOUSE_INPUT | ENABLE_QUICK_EDIT_MODE);
    wantIn |= ENABLE_EXTENDED_FLAGS | ENABLE_VIRTUAL_TERMINAL_INPUT;
    if (!SetConsoleMode(hIn, wantIn)) {
        SetConsoleMode(hOut, outMode_);
        return false;
    }
    return true;
}

void Terminal::reclaim() {
    if (!raw_) return;
    applyModes();
    SetConsoleOutputCP(CP_UTF8);
}

Terminal::~Terminal() {
    if (codePage_ != 0) SetConsoleOutputCP(codePage_);
    if (!raw_) return;
    SetConsoleMode((HANDLE)in_, inMode_);
    SetConsoleMode((HANDLE)out_, outMode_);
}

void Terminal::size(int& rows, int& cols) const {
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (!out_ || !GetConsoleScreenBufferInfo((HANDLE)out_, &info)) {
        rows = 24;
        cols = 80;
        return;
    }

    cols = info.srWindow.Right - info.srWindow.Left + 1;
    rows = info.srWindow.Bottom - info.srWindow.Top + 1;
    if (cols <= 0) cols = 80;
    if (rows <= 0) rows = 24;
}

void Terminal::write(const std::string& s) {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;

    size_t sent = 0;
    while (sent < s.size()) {
        DWORD wrote = 0;
        if (!WriteFile(hOut, s.data() + sent, (DWORD)(s.size() - sent), &wrote, NULL))
            return;
        if (wrote == 0) return;
        sent += wrote;
    }
}

bool Terminal::readByte(char& c) const {
    HANDLE hIn = in_ ? (HANDLE)in_ : GetStdHandle(STD_INPUT_HANDLE);
    if (hIn == INVALID_HANDLE_VALUE) return false;

    if (WaitForSingleObject(hIn, 100) != WAIT_OBJECT_0) return false;

    if (raw_) {

        INPUT_RECORD record;
        DWORD seen = 0;
        if (!PeekConsoleInput(hIn, &record, 1, &seen) || seen == 0) return false;
        if (record.EventType != KEY_EVENT || !record.Event.KeyEvent.bKeyDown ||
            record.Event.KeyEvent.uChar.AsciiChar == 0) {
            ReadConsoleInput(hIn, &record, 1, &seen);
            return false;
        }
    }

    DWORD got = 0;
    if (!ReadFile(hIn, &c, 1, &got, NULL)) {
        if (!raw_) eof_ = true;
        return false;
    }
    if (got == 0 && !raw_) eof_ = true;
    return got == 1;
}

}
