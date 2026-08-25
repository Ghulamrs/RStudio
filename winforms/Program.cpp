
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

    }
}

static void OnUnhandled(Object^, UnhandledExceptionEventArgs^ e) {
    Note("unhandled: " + e->ExceptionObject->ToString());
}

static void QuietConsoleForChildren() {
    if (GetConsoleWindow() != NULL) return;
    if (!AllocConsole()) return;

    HWND console = GetConsoleWindow();
    if (console != NULL) ShowWindow(console, SW_HIDE);

    FILE* ignored = NULL;
    freopen_s(&ignored, "NUL", "r", stdin);
    freopen_s(&ignored, "NUL", "w", stdout);
    freopen_s(&ignored, "NUL", "w", stderr);
}

[STAThreadAttribute]
int main(array<String^>^ arguments) {

    rstudio_watch_for_faults("RStudioGui-fault.log");
    QuietConsoleForChildren();
    AppDomain::CurrentDomain->UnhandledException +=
        gcnew UnhandledExceptionEventHandler(OnUnhandled);

    try {
        Note("starting, " + arguments->Length + " arguments");
        editor::installPlatformDemangler();
        Application::EnableVisualStyles();
        Application::SetCompatibleTextRenderingDefault(false);

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
