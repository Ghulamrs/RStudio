// The way in. Windows Forms wants a single-threaded apartment, and says so by
// refusing to show some dialogs without one.
//
// Everything is wrapped, and what goes wrong is written down. A GUI that dies
// on a machine you are not sitting at tells you nothing otherwise.

#include <cstdio>

#include <windows.h>

#include "MainForm.h"

#include "symbols.h"



using namespace System;
using namespace System::Windows::Forms;

static void Note(String^ what) {
    try {
        System::IO::File::AppendAllText("RStudioGui.log",
                                        DateTime::Now.ToString("HH:mm:ss") + "  " + what +
                                            Environment::NewLine);
    } catch (Exception^) {
        // Nowhere to write is not worth dying for.
    }
}

static void OnUnhandled(Object^, UnhandledExceptionEventArgs^ e) {
    Note("unhandled: " + e->ExceptionObject->ToString());
}

// One console for the window, made once and never shown.
//
// The window is /SUBSYSTEM:WINDOWS and so has no console. Every compiler and
// every program it runs goes through cmd, which is a console program, so
// Windows made one for each of them - and that is the black rectangle that
// blinked on the desktop at every build and every run.
//
// It was not fixable where *this* editor starts its child, and the reason is
// narrower than it first looked. CREATE_NO_WINDOW asks for a console with no
// window, STARTF_USESHOWWINDOW/SW_HIDE says it again, and with cmd.exe as the
// child neither holds on Windows 11: the default-terminal handoff gives the
// console to Windows Terminal and a window appears anyway, class
// CASCADIA_HOSTING_WINDOW_CLASS - the watcher read that title off the window.
//
// Measured with cmd and only with cmd. A compiler started *directly*, with no
// shell in between, may well be silenced by CREATE_NO_WINDOW alone; that is
// the usual recipe and it is not tested here, because runCaptured cannot drop
// cmd - see below. DETACHED_PROCESS does remove the window, and
// removes some of the output with it - 22 checks, "a build that fails says so"
// among them - because what writes those goes to the console rather than to
// the handle it was given.
//
// So the child is left exactly as it was, and this gives it something to
// inherit. A process that already has a console hands it to its children and
// no new one is made, so there is nothing to hand off and nothing to draw.
// Hidden immediately, and the handles pointed at NUL so that nothing the
// editor or a library prints ever goes anywhere surprising.
static void QuietConsoleForChildren() {
    if (GetConsoleWindow() != NULL) return;   // started from a terminal; use that one
    if (!AllocConsole()) return;              // no console to be had; the blink stays

    HWND console = GetConsoleWindow();
    if (console != NULL) ShowWindow(console, SW_HIDE);

    FILE* ignored = NULL;
    freopen_s(&ignored, "NUL", "r", stdin);
    freopen_s(&ignored, "NUL", "w", stdout);
    freopen_s(&ignored, "NUL", "w", stderr);
}

[STAThreadAttribute]
int main(array<String^>^ arguments) {
    // Its own debugger, since the machine has none.
    // Its own debugger. There is none installed on the machine this is built
    // for, and a crash with no stack is a crash you cannot fix.
    rstudio_watch_for_faults("RStudioGui-fault.log");
    QuietConsoleForChildren();
    AppDomain::CurrentDomain->UnhandledException +=
        gcnew UnhandledExceptionEventHandler(OnUnhandled);

    try {
        Note("starting, " + arguments->Length + " arguments");
        editor::installPlatformDemangler();
        Application::EnableVisualStyles();
        Application::SetCompatibleTextRenderingDefault(false);

        // RStudioGui [project-directory] [file ...] - every file named gets a tab.
        String^ directory = arguments->Length > 0 ? arguments[0] : nullptr;
        array<String^>^ files =
            gcnew array<String^>(arguments->Length > 1 ? arguments->Length - 1 : 0);
        for (int i = 1; i < arguments->Length; ++i) files[i - 1] = arguments[i];

        Note("building the window");
        rstudiogui::MainForm^ window = gcnew rstudiogui::MainForm(directory, files);
        Note("window built, running");
        Application::Run(window);
        Note("closed cleanly");
    } catch (Exception^ problem) {
        Note("caught: " + problem->ToString());
        return 1;
    }
    return 0;
}
