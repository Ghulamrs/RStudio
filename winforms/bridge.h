#ifndef RSTUDIO_BRIDGE_H
#define RSTUDIO_BRIDGE_H

// The seam between the managed form and the native editor, and the reason it
// exists is worth writing down.
//
// A /clr translation unit that instantiates std::string or std::vector, in a
// binary whose native translation units instantiate the same templates,
// corrupts the heap before main is ever reached - exit code 0xC0000374. The
// linker folds the two instantiations together and one side's allocation ends
// up paired with the other side's free. It was reproduced from a bare Windows
// Forms application: it ran, it ran with all nine native files linked in, and
// it died the moment one std::vector<std::string> appeared in the managed file.
//
// So nothing below names a C++ type. No STL, no editor headers, no classes -
// plain C declarations, opaque handles and char pointers. bridge.cpp holds all
// of it on the native side, and the form never sees a template.
//
// Every const char* returned is owned by the thing that returned it and stays
// good until the next call on that thing, or until it is freed. Anything
// returned as char* is the caller's, and rstudio_free takes it back.

#ifdef __cplusplus
extern "C" {
#endif

/* Kinds, languages, toolchains and configurations, repeated here as plain
   integers so that the form can speak about them without including the
   headers that define them. bridge.cpp checks they still agree. */
enum {
    RSTUDIO_KIND_NORMAL = 0, RSTUDIO_KIND_KEYWORD, RSTUDIO_KIND_TYPE, RSTUDIO_KIND_STRING,
    RSTUDIO_KIND_CHAR, RSTUDIO_KIND_COMMENT, RSTUDIO_KIND_PREPROC, RSTUDIO_KIND_NUMBER,
    RSTUDIO_KIND_LABEL
};
enum { RSTUDIO_LANG_PLAIN = 0, RSTUDIO_LANG_C, RSTUDIO_LANG_CPP, RSTUDIO_LANG_SHALIMAR,
       RSTUDIO_LANG_ASM, RSTUDIO_LANG_JSON };

// Which language's punctuation the layout rules follow. Not the same question
// as which language the file is - assembly and plain text are laid out by
// neither - so it is a value of its own rather than a Language.
enum { RSTUDIO_DIALECT_C = 0, RSTUDIO_DIALECT_SHALIMAR };
enum { RSTUDIO_TOOL_AUTO = 0, RSTUDIO_TOOL_CC1, RSTUDIO_TOOL_MSVC, RSTUDIO_TOOL_SHC, RSTUDIO_TOOL_CXX };
enum { RSTUDIO_CONFIG_DEBUG = 0, RSTUDIO_CONFIG_RELEASE };

/* Catches a crash and writes the faulting address and a symbolised stack to
   RStudioGui-fault.log. There is no WinDbg on the machine this is built
   for, and
   dbghelp is, so the program carries its own. Does nothing off Windows. */
void rstudio_watch_for_faults(const char* logPath);

/* ---- laying code out ---------------------------------------------------- */

/* The whole buffer in, the whole buffer out, lines separated by \n. */
char* rstudio_reindent(const char* text, int width, int tabs, int caseIndent,
                   int dialect);

/* What a newline typed at row and col should be followed by. */
char* rstudio_indent_after_newline(const char* text, int row, int col,
                               int width, int tabs, int caseIndent, int dialect);

/* The leading space one line should have, for the tab key and for a line whose
   own layout changed the moment a brace was typed on it. */
char* rstudio_indent_for(const char* text, int row, int width, int tabs, int caseIndent,
                     int dialect);

void rstudio_free(char* what);

/* ---- finding and replacing ---------------------------------------------- */

/* 1 when found, and where it was written into row and col. Both wrap once and
   stop where they started. */
int rstudio_find_next(const char* text, const char* needle, int row, int col,
                  int* foundRow, int* foundCol);
int rstudio_find_previous(const char* text, const char* needle, int row, int col,
                      int* foundRow, int* foundCol);

/* The whole text with every occurrence replaced, and how many there were. */
char* rstudio_replace_all(const char* text, const char* needle, const char* with,
                      int* howMany);

/* ---- colouring ---------------------------------------------------------- */

/* Rich Edit records a change of colour in its undo buffer exactly as it records
   typing, so colouring a file fills that buffer before anybody has touched the
   text: Ctrl-Z then undoes a colour, moves the caret to it, and marks a file
   modified that was only ever looked at. These suspend and resume the recording
   around a colouring pass, given the editing window's handle.

   Undoing your own typing is unaffected - only what happens between the two
   calls goes unrecorded. Anywhere but Windows, and wherever the interface
   cannot be had, they do nothing and colouring behaves as before. */
/* The font the window last drew code in, as the window spelled it, and a way
   to keep the next choice. Empty when nothing was ever chosen. */
/* Where an unreadable configuration was put, or empty. Worth saying once at
   startup: the settings went back to their defaults and the old file is still
   there to look at. */
const char* rstudio_settings_set_aside(void);

const char* rstudio_code_font(void);
int rstudio_remember_code_font(const char* described);

void rstudio_undo_suspend(void* windowHandle);
void rstudio_undo_resume(void* windowHandle);

int rstudio_language_for(const char* path);

/* Which layout rules a language wants. */
int rstudio_dialect_for(int language);

/* One kind per byte of the line, written into kinds. `state` carries the block
   comment across lines and is read and written. Returns how many were set. */
int rstudio_highlight(const char* line, int language, int* state,
                  unsigned char* kinds, int kindsSize);

/* ---- what a build produced ---------------------------------------------- */

/* The functions, exports, imports and strings found in an assembly listing,
   already laid out a line at a time. Not debug information, whatever the target
   writes: this build stops at the assembly, so there is nothing running to
   debug. This is what there is to know instead. */
char* rstudio_describe_build(const char* assembly);

/* What the panel says above that listing - what debug information this target
   has, and what the listing is instead. The terminal front end shows the same
   words, because both ask here rather than writing them out. */
char* rstudio_debug_note(int kind, const char* arch);

/* ---- the project -------------------------------------------------------- */

typedef struct RStudioProject RStudioProject;

RStudioProject* rstudio_project_new(void);
void rstudio_project_free(RStudioProject* project);

/* 1 when a project was read, 0 when there is none or it is broken; the reason
   goes into error when there is one. */
int rstudio_project_load(RStudioProject* project, const char* directory,
                     char* error, int errorSize);

const char* rstudio_project_name(RStudioProject* project);
int rstudio_project_groups(RStudioProject* project);
const char* rstudio_project_group_name(RStudioProject* project, int group);
int rstudio_project_files(RStudioProject* project, int group);
const char* rstudio_project_file(RStudioProject* project, int group, int file);
const char* rstudio_project_absolute(RStudioProject* project, const char* relative);

int rstudio_project_indent_width(RStudioProject* project);
int rstudio_project_indent_tabs(RStudioProject* project);
int rstudio_project_case_indent(RStudioProject* project);
int rstudio_project_toolchain(RStudioProject* project);
/* Debug or release for this machine, and remembering a change to it. It was
   rstudio_project_config, read off the project; it is settings now, because
   which one you are building is what you are doing today rather than what the
   program is, and a project file travels. 0 debug, 1 release. */
int rstudio_configuration(void);
void rstudio_remember_configuration(int config);
const char* rstudio_project_arch(RStudioProject* project);

/* ---- changing the project ------------------------------------------------ */

/* The shape a path may have: the root, or one directory under it, and no
   deeper. It comes from the core so that both front ends keep one rule; the
   reason goes into `why` when the answer is no. */
int rstudio_project_allows(const char* relative, char* why, int whySize);

/* Which group a file's name puts it in - "Headers" for a .h, "Shalimar" for a
   .shl, "Sources" for the rest, and empty for anything this editor does not
   compile. The same call the terminal front end makes, so a header added in
   the window lands where a header added in the terminal does. */
const char* rstudio_group_for_file(const char* name);

/* The suffix a named project file has - ".pro" - so the window's dialogs can
   filter on it without keeping their own copy of the answer. */
const char* rstudio_project_suffix(void);

/* Write the project out under a different name and keep saving there. 1 when
   it worked; the reason goes into `why` when it did not. The old file is left
   where it is - converting a project is asked for, never done in passing. */
int rstudio_project_save_as(RStudioProject* project, const char* file,
                            char* why, int whySize);

int rstudio_project_loaded(RStudioProject* project);
const char* rstudio_project_root(RStudioProject* project);
void rstudio_project_set_root(RStudioProject* project, const char* path);

/* Puts the project away: nothing loaded, and RStudio.json left exactly as it was.
   Closing a project is a change to what is being looked at and not to what the
   project is, so nothing is written and nothing is removed from it. */
void rstudio_project_close(RStudioProject* project);
const char* rstudio_project_relative(RStudioProject* project, const char* path);
const char* rstudio_project_file_name(void);

/* Each of these does the disk work, keeps the project's list in step and
   writes the project back out - all of it in the core, so the window and the
   terminal do the same thing rather than two similar things. 1 when it worked;
   the message and the path say what happened and stay good until the next one. */
int rstudio_create_file(RStudioProject* project, const char* relative, const char* group);
int rstudio_rename_file(RStudioProject* project, const char* fromAbsolute, const char* toRelative);
int rstudio_delete_file(RStudioProject* project, const char* absolute);
int rstudio_move_to_group(RStudioProject* project, const char* absolute, const char* group);
int rstudio_add_existing(RStudioProject* project, const char* absolute, const char* group);
// Out of the project's list; the file stays on the disk.
int rstudio_remove_from_project(RStudioProject* project, const char* absolute);
/* A project made out of what is already in a directory, for when there is no
   RStudio.json to read - and the two things that decide where the editor opens
   when it is given nothing: the project it was last in, and the small one made
   in your own files the first time there is no answer to that.

   All three have been in the core since the terminal half used them; the
   window simply never asked, so it opened on an empty pane where the terminal
   opened on your work. */
int rstudio_begin_from_what_is_there(RStudioProject* project, const char* directory);
const char* rstudio_last_project(void);
int rstudio_remember_project(const char* directory);
const char* rstudio_demo_directory(void);

int rstudio_begin_project(RStudioProject* project, const char* directory, const char* name,
                      const char* firstFile);
int rstudio_save_project(RStudioProject* project);

const char* rstudio_outcome_message(RStudioProject* project);
const char* rstudio_outcome_path(RStudioProject* project);

/* ---- compilers ---------------------------------------------------------- */

const char* rstudio_arch(int index);              /* 0, 1, 2 */
const char* rstudio_toolchain_name(int kind);

/* What the core calls a language and a configuration. Here so that both front
   ends say the same words for the same thing - the terminal's status bar has
   had these since the beginning and the window had no way to ask for them. */
const char* rstudio_language_name(int language);
const char* rstudio_config_name(int config);
int rstudio_resolve(int toolchainKind, int language);
int rstudio_can_compile(int kind, int language);
const char* rstudio_refusal(int kind, int language);
int rstudio_uses_arch(int kind);

/* Whether a build for this target can be run on this machine, which is not the
   same question as whether it can be compiled - every target compiles to
   assembly anywhere, and only the host's own goes on to a program. The second
   gives the reason when the answer is no, and an empty string when it is yes. */
int rstudio_runs_here(int kind, const char* arch);
const char* rstudio_why_not_run(int kind, const char* arch);
const char* rstudio_host_arch(void);

const char* rstudio_shown_command(const char* cc1, const char* cl, const char* shc, int kind,
                              const char* source, int language, const char* arch,
                              int config);

/* c2s, the C89 <-> Shalimar converter, driven over the open file. Not a
   toolchain: what comes out is source in the other language rather than a
   program, so it has its own handle and says nothing to the diagnostic pane.

   rstudio_converts_from answers which way a file of this language goes, and 0
   for a language with no other side - C++, JSON, assembly, plain text.
   rstudio_find_converter answers where c2s is, or "" when nothing was found;
   the string is the caller's and rstudio_free takes it back. */
int rstudio_converts_from(int language, int* toShalimar);
char* rstudio_find_converter(void);
char* rstudio_converted_name(const char* source, int toShalimar);

typedef struct RStudioConversion RStudioConversion;

RStudioConversion* rstudio_convert(const char* converter, const char* source,
                                   const char* output, int toShalimar);
void rstudio_conversion_free(RStudioConversion* made);
int rstudio_conversion_ran(RStudioConversion* made);
int rstudio_conversion_ok(RStudioConversion* made);
const char* rstudio_conversion_produced(RStudioConversion* made);
const char* rstudio_conversion_output(RStudioConversion* made);

typedef struct RStudioBuild RStudioBuild;

RStudioBuild* rstudio_build(const char* cc1, const char* cl, const char* shc, int kind, const char* source,
                    int language, const char* arch, int config);

/* The project's own program, as against the file in front of you. Which of the
   two you meant is said by which one you asked for; neither needs the other to
   be unavailable, and compiling one file never needs a project at all.

   rstudio_project_target_ready works out the sources and answers 1 when there is
   something to build. When it answers 0, rstudio_project_target_why is one line
   for a status bar and rstudio_project_target_detail is the rest of it for a box
   with room - the refusal that matters being a project holding both C and C++,
   which cc1 and cl cannot be asked to build between them. */
int rstudio_project_builds(RStudioProject* project);
int rstudio_project_target_ready(RStudioProject* project);
const char* rstudio_project_target_why(RStudioProject* project);
const char* rstudio_project_target_detail(RStudioProject* project);
int rstudio_project_target_language(RStudioProject* project);
int rstudio_project_target_sources(RStudioProject* project);

/* The same target by the parts it is actually built from: one per group, each
   with its own compiler. A target of C and C++ has two, and there is no single
   language or compiler that describes it - which is what these are for.
   rstudio_project_part_toolchain takes the same compiler paths and override kind
   as rstudio_build_target, since what a part goes to depends on all of them. */
int rstudio_project_target_parts(RStudioProject* project);
const char* rstudio_project_part_group(RStudioProject* project, int index);
int rstudio_project_part_language(RStudioProject* project, int index);
int rstudio_project_part_toolchain(RStudioProject* project, int index, const char* cc1,
                               const char* cl, const char* shc, int kind);
const char* rstudio_project_target_source(RStudioProject* project, int index);
const char* rstudio_project_target_program(RStudioProject* project);

/* How the project's program is stepped through, which is not answered by
   asking about one compiler: a target built from two groups can have a
   debugger that sees one of them and not the other.

   rstudio_project_debug_plan works it out and answers 1 when F8 can do anything at
   all. When it answers 0, rstudio_project_why_not_debug is the reason. After a 1,
   rstudio_project_debug_kind is the compiler to start with - which is not always
   the one the target's language would suggest - and the blind groups are the
   ones whose code carries no debug information, worth saying before the build
   rather than after somebody has tried to stop in one.

   The plan is kept on the project, as the parts are, so the strings outlive
   the call. Ask rstudio_project_target_ready first, as with the build. */
int rstudio_project_debug_plan(RStudioProject* project, const char* cc1, const char* cl,
                           const char* shc, int kind, const char* arch);
int rstudio_project_debug_kind(RStudioProject* project);
const char* rstudio_project_why_not_debug(RStudioProject* project);
int rstudio_project_blind_groups(RStudioProject* project);
const char* rstudio_project_blind_group(RStudioProject* project, int index);

/* Builds it. Ask rstudio_project_target_ready first: this answers null when there
   is nothing to build, and the reason is where that call left it. */
RStudioBuild* rstudio_build_target(RStudioProject* project, const char* cc1, const char* cl, const char* shc,
                           int kind, const char* arch, int config);
void rstudio_build_free(RStudioBuild* built);

int rstudio_build_ok(RStudioBuild* built);
const char* rstudio_build_output(RStudioBuild* built);
const char* rstudio_build_assembly(RStudioBuild* built);   /* joined with \n */
int rstudio_build_assembly_lines(RStudioBuild* built);
int rstudio_build_has_error(RStudioBuild* built);
/* Which file the error is in. It used to go without saying - a build was one
   file and that was the one you were looking at - and a project build is
   several, so it has to be asked. */
const char* rstudio_build_error_file(RStudioBuild* built);
int rstudio_build_error_line(RStudioBuild* built);
int rstudio_build_error_column(RStudioBuild* built);
const char* rstudio_build_error_message(RStudioBuild* built);

/* Compiling, linking and running, which is three things that can each go their
   own way: rstudio_ran_built says a program came out, rstudio_ran_ran says it was
   started, and rstudio_ran_status is what it returned once it had. A program that
   returns 1 is not a build that failed. */
typedef struct RStudioRan RStudioRan;

RStudioRan* rstudio_run(const char* cc1, const char* cl, const char* shc, int kind, const char* source,
                int language, const char* arch, int config);

/* Runs a program that is already built - the project's, once it has been. */
RStudioRan* rstudio_run_built(const char* program);
void rstudio_run_free(RStudioRan* ran);

int rstudio_ran_built(RStudioRan* ran);
int rstudio_ran_ran(RStudioRan* ran);
int rstudio_ran_status(RStudioRan* ran);
const char* rstudio_ran_output(RStudioRan* ran);
int rstudio_ran_has_error(RStudioRan* ran);
int rstudio_ran_error_line(RStudioRan* ran);
int rstudio_ran_error_column(RStudioRan* ran);
const char* rstudio_ran_error_message(RStudioRan* ran);

const char* rstudio_shown_run_command(const char* cc1, const char* cl, const char* shc, int kind,
                                  const char* source, int language, const char* arch,
                                  int config);

/* Who this is and whose it is, a line at a time joined with \n. The window puts
   it in a box and the terminal prints it in its panel; neither writes it. */
char* rstudio_about(void);

/* ---- stopping on a line ------------------------------------------------- */

/* A program built and left where it is, which is what a debugger attaches to.
   Freeing the handle removes the program with it. */
typedef struct RStudioProgram RStudioProgram;

RStudioProgram* rstudio_build_program(const char* cc1, const char* cl, const char* shc, int kind, const char* source,
                              int language, const char* arch, int config);
void rstudio_program_free(RStudioProgram* built);

int rstudio_program_ok(RStudioProgram* built);
const char* rstudio_program_path(RStudioProgram* built);
const char* rstudio_program_output(RStudioProgram* built);
int rstudio_program_has_error(RStudioProgram* built);
int rstudio_program_error_line(RStudioProgram* built);
int rstudio_program_error_column(RStudioProgram* built);
const char* rstudio_program_error_message(RStudioProgram* built);

/* Which debugger can read what this compiler writes for this target: 0 none,
   1 lldb, 2 gdb. Not a question about the machine alone - on Windows a C file
   goes to cc1 and comes out as MASM with no line table, while a C++ file goes
   to cl and comes out with CodeView in a .pdb. */
int rstudio_debugger_for(int kind, const char* arch);
const char* rstudio_debugger_name(int kind);
const char* rstudio_no_debugger_because(int kind, const char* arch);

/* And the question that comes before it: whether a program this compiler
   builds carries its own way of stopping and needs no debugger at all. Only
   shc does - a Shalimar program stops itself.

   The order matters and is the whole of what the window used to get wrong.
   rstudio_debugger_for answers 0 for shc and is right to; reading that as a
   refusal refuses the one language that needs nothing to be installed. So this
   is asked first, exactly as the terminal half asks it. */
int rstudio_debugger_stops_itself(int kind);

/* Why a release build cannot be stopped, which is not the same reason for the
   two: a C build is missing -g, and a Shalimar one links a runtime with no
   debugger in it - there being no -g here to have left out. The key that
   changes it is the front end's own and is not part of this sentence. */
const char* rstudio_release_cannot_stop(int kind);

/* And why a start that was asked for did not happen, in the terms that apply:
   a debugger that is not installed, or a program that never said it was
   ready. */
const char* rstudio_why_it_did_not_start(int kind, const char* arch);

typedef struct RStudioDebugger RStudioDebugger;

RStudioDebugger* rstudio_debugger_new(void);
void rstudio_debugger_free(RStudioDebugger* debugger);

/* Starts whichever of the two applies, and the handle holds both: gdb, lldb or
   cdb on one side, and a Shalimar program's own session on the other. It takes
   the compiler and the target rather than a debugger, because which of those
   two it is, is decided from those - and deciding it here rather than in the
   window is what keeps one answer to it. Everything below asks the handle
   which half is live rather than being told. */
int rstudio_debugger_start(RStudioDebugger* debugger, int kind, const char* arch,
                       const char* program);
int rstudio_debugger_running(RStudioDebugger* debugger);
void rstudio_debugger_stop(RStudioDebugger* debugger);

/* Which half that is. The window asks it for one thing only - whether to offer
   the keys for what a Shalimar program cannot do - and the three sentences
   below are what it says instead of offering them. */
int rstudio_debugging_shalimar(RStudioDebugger* debugger);

/* The Debug tab's line where the variables would be, right for whichever half
   is live: this place has none, or no place ever will. Empty means two
   different things and the difference is a gap against a decision.

   The two below are empty when the thing can be done, and the reason when it
   cannot - so the window puts up what it is given rather than deciding for
   itself which case it is in. */
const char* rstudio_locals_none_because(RStudioDebugger* debugger);
const char* rstudio_cannot_watch(RStudioDebugger* debugger);
const char* rstudio_cannot_walk_stack(RStudioDebugger* debugger);

int rstudio_debugger_break(RStudioDebugger* debugger, const char* file, int line);
int rstudio_debugger_clear(RStudioDebugger* debugger);

/* Each of these moves the program and keeps what came of it, which is then
   read with the rstudio_stop_ calls below. */
void rstudio_debugger_run(RStudioDebugger* debugger);
void rstudio_debugger_resume(RStudioDebugger* debugger);
void rstudio_debugger_step_over(RStudioDebugger* debugger);
void rstudio_debugger_step_into(RStudioDebugger* debugger);
void rstudio_debugger_step_out(RStudioDebugger* debugger);

int rstudio_stop_stopped(RStudioDebugger* debugger);
int rstudio_stop_exited(RStudioDebugger* debugger);
int rstudio_stop_status(RStudioDebugger* debugger);
const char* rstudio_stop_file(RStudioDebugger* debugger);
int rstudio_stop_line(RStudioDebugger* debugger);
const char* rstudio_stop_function(RStudioDebugger* debugger);

/* What the debugger itself printed on its way to that stop - cdb's or lldb's
   own words, and the debugged program's output along with them, since a
   debuggee writes down the debugger's stream. Empty when it said nothing.

   Six things were readable about a stop here and this was the seventh, kept on
   the native side where the terminal half could reach it and the window could
   not. So a stop the window could not make sense of was reported as "the
   debugger stopped answering" with nothing under it, while the terminal
   printed what had actually been said. */
const char* rstudio_stop_said(RStudioDebugger* debugger);

/* And the same thing with the debugger's own words taken out: what the program
   printed on its way to this stop, for the console. Empty when it printed
   nothing. The filtering is dbg_programOutput's, on the native side, so both
   front ends show the same thing. */
const char* rstudio_stop_output(RStudioDebugger* debugger);

/* Whether a stop that named no place is a program standing where there is no
   source - stepping off the end of main - rather than a debugger that died.
   The first is carried on from; the second is the end of the session. */
int rstudio_stop_no_source(RStudioDebugger* debugger);

/* What is in scope where it stopped, read after a move. */
int rstudio_locals_count(RStudioDebugger* debugger);
const char* rstudio_local_name(RStudioDebugger* debugger, int index);
const char* rstudio_local_type(RStudioDebugger* debugger, int index);
const char* rstudio_local_value(RStudioDebugger* debugger, int index);

// How a variable is written in the Debug tab, which variable a line of it is
// about, and the setting of one. The window writes the line with the first and
// reads it back with the second rather than counting rows of its own, exactly
// as it does with the frames - see dbg_variableLine.
//
// rstudio_set_variable answers 0 when the debugger would not take it, and
// rstudio_set_complaint is what it said about that: its own words name the mistake
// better than any this could invent. A set that worked re-reads the locals, so
// rstudio_local_* answer with what is in there afterwards.
const char* rstudio_local_text(RStudioDebugger* debugger, int index);
int rstudio_locals_on_line(RStudioDebugger* debugger, const char* line);
int rstudio_set_variable(RStudioDebugger* debugger, const char* name, const char* value);
const char* rstudio_set_complaint(RStudioDebugger* debugger);

// Expressions the debugger keeps answering, read again after every move and
// every change of frame - that being the whole of what a watch is. The list
// lives in the core beside the debugger rather than in either front end, so
// that both refresh them at exactly the same moments.
//
// rstudio_watch_set with an empty expression takes that watch away; there is no
// list here that needs a key of its own for removing something.
void rstudio_watch_add(RStudioDebugger* debugger, const char* expression);
int rstudio_watch_count(RStudioDebugger* debugger);
const char* rstudio_watch_text(RStudioDebugger* debugger, int index);
const char* rstudio_watch_expression(RStudioDebugger* debugger, int index);
int rstudio_watch_on_line(RStudioDebugger* debugger, const char* line);
void rstudio_watch_set(RStudioDebugger* debugger, int index, const char* expression);

// And how it got there: the frame it is standing in first, and what called it
// after that. One frame is a program standing in main, which the Debug tab
// says nothing about - see dbg_readFrames for why the stack stops there.
int rstudio_stack_count(RStudioDebugger* debugger);
const char* rstudio_stack_function(RStudioDebugger* debugger, int index);
const char* rstudio_stack_file(RStudioDebugger* debugger, int index);
int rstudio_stack_line(RStudioDebugger* debugger, int index);

// How a frame is written in the Debug tab, and which frame a line of that tab
// is about - -1 for a line that is not one. The window writes the line with
// the first and reads it back with the second rather than counting rows of its
// own, so that the row it acts on is the row the core named. See dbg_frameLine.
const char* rstudio_stack_text(RStudioDebugger* debugger, int index);
int rstudio_stack_on_line(RStudioDebugger* debugger, const char* line);

// Which frame the variables are to be read from. All three debuggers keep a
// current frame and answer with its variables, so this is what makes a
// caller's locals readable at all; rstudio_locals_* answer from it afterwards.
// Every move puts it back to 0, the frame the program stopped in.
//
// rstudio_looking_text is the line that says whose they are, empty at frame 0
// where the stop's own line already says it.
int rstudio_debugger_look_at(RStudioDebugger* debugger, int which);

// The tab's first line, which names the frame the program stopped in. The
// window writes it with this and compares a clicked row against it, so that
// enter on that row is enter on frame 0 - see dbg_stopLine.
const char* rstudio_stop_line_text(const char* file, int line, const char* function);
int rstudio_looking_at(RStudioDebugger* debugger);
const char* rstudio_looking_text(RStudioDebugger* debugger);

#ifdef __cplusplus
}
#endif

#endif
