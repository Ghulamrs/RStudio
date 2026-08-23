#!/usr/bin/env python3
"""Writes the project files that build ed1, cc1 and shc together.

    python3 tools/make-projects.py            write them
    python3 tools/make-projects.py --check    say whether they are current

Three machines, three shapes, one idea: open one thing and get all three
programs, with the editor built after the two compilers it drives.

    macOS    RStudio.xcworkspace          RStudio.exe, cc1.exe, shc.exe
    Windows  RStudio.sln                  RStudioConsole, RStudioGui, cc1, shc
    Linux    workspace.mk                 make -f workspace.mk

Was make-xcodeproj.py while Xcode was all it wrote.

Three command line tools, built by clang++, from three separate repositories:

    RStudio  this editor         RStudio/Editor.xcodeproj
    cc1      the C compiler      ../Compiler-C/cc1.xcodeproj
    shc      the Shalimar one    ../Compiler-S/shc.xcodeproj

and RStudio.xcworkspace, which opens all three at once so that a change to a
compiler and the change to the editor that goes with it are one build and one
issue list.

**Each project is generated from that repository's own Makefile.** A hand-kept
project drifts: someone adds a file to the Makefile, forgets the project, and
Xcode quietly builds yesterday's program - which is not an error, just a
smaller program, so nothing says so. That has now happened twice here. Once
when sources were added and this was not re-run, and once when this script
stopped reading a Makefile variable that had been added to it. --check is the
answer to both: it rebuilds every project in memory and compares.

**Two projects are kept by hand and are checked rather than written**, because
what is in them besides the source list cannot be derived from a Makefile:
winforms/RStudioGui.vcxproj compiles one file managed and every other file
native, and Compiler-C/msvc/cc1.vcxproj belongs to another repository. Their
source lists are compared against the Makefiles all the same - that is the part
that drifts, and the window's had drifted the whole time nobody was checking
it. --check also insists the Makefile is made of the variables named here and
no others, so adding one and forgetting this script fails loudly.

The identifiers are derived from the file names and the product, so
regenerating an unchanged project produces a byte-identical file and the
comparison is exact.

A project is written into the repository it belongs to, so its paths are
relative to that repository and mean something inside it. The workspace lives
here and reaches the other two with ../ - which is the one thing in this that
assumes all three are checked out side by side.
"""

import hashlib
import os
import re
import sys

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SIBLINGS = os.path.dirname(HERE)


def ident(product, *parts):
    """A stable 24-hex-digit identifier, seeded by the product it belongs to.

    Xcode wants these unique within a project. Seeding with the product as well
    as the name keeps three projects from sharing identifiers, which is not
    strictly a fault across separate files but makes two of them impossible to
    tell apart when reading a diff.
    """
    digest = hashlib.sha1((product + ":" + ":".join(parts)).encode()).hexdigest()
    return digest[:24].upper()


def from_makefile(root, variables):
    """The .cpp named by these Makefile variables, relative to the repository."""
    text = open(os.path.join(root, "Makefile")).read()
    found = []
    for variable in variables:
        match = re.search(r"^%s *:?= (.*?)(?=\n[A-Z#]|\n\n)" % variable, text, re.S | re.M)
        if not match:
            sys.exit("could not find %s in %s/Makefile" % (variable, root))
        # A slash in the middle: src/backend/X86_64.cpp and runtime/Shortest.cpp
        # are both one path. A character class without one matched nothing at
        # all rather than failing, which is how the Shalimar half went missing.
        found += re.findall(r"([A-Za-z0-9_]+(?:/[A-Za-z0-9_]+)*\.cpp)", match.group(1))
    return found


def composed_of(root, variable):
    """The $(NAMES) one Makefile variable is made of, as a set.

    This exists because of how this script failed once before: a variable was
    added to the Makefile, SRC was made to include it, and nothing here read
    it - so every project quietly built a smaller program than make did. The
    lists below say which variables to read; this says which ones the Makefile
    actually uses, and main() insists the two agree. Adding a variable and
    forgetting this script is then a loud failure instead of a silent one.
    """
    text = open(os.path.join(root, "Makefile")).read()
    match = re.search(r"^%s *:?= (.*?)(?=\n[A-Z#]|\n\n)" % variable, text, re.S | re.M)
    if not match:
        sys.exit("could not find %s in %s/Makefile" % (variable, root))
    return set(re.findall(r"\$\(([A-Za-z0-9_]+)\)", match.group(1)))


def by_glob(root, directories):
    """The .cpp in these directories - for a Makefile that says $(wildcard ...).

    Compiler-C's does, so there is no list to read and the directories are the
    list. Sorted, so that a project regenerated on two machines is the same
    file on both.
    """
    found = []
    for directory in directories:
        where = os.path.join(root, directory)
        if not os.path.isdir(where):
            sys.exit("no %s in %s" % (directory, root))
        for f in sorted(os.listdir(where)):
            if f.endswith(".cpp"):
                found.append(directory + "/" + f)
    return found


def headers_under(root, directories):
    found = []
    for directory in directories:
        top = os.path.join(root, directory)
        for where, _, files in os.walk(top):
            for f in files:
                if f.endswith(".h"):
                    found.append(os.path.relpath(os.path.join(where, f), root))
    return sorted(found)


# What the editor is made of, as the Makefile now says it: the core both front
# ends compile, the terminal's own half, and the Shalimar session. SRC is
# CORE_SRC and TERMINAL_SRC together and names no files of its own, so reading
# it would find nothing - which composed_of is here to keep true.
EDITOR_VARIABLES = ("CORE_SRC", "TERMINAL_SRC", "SHM_SRC")


def rstudio_sources():
    """The editor's sources, minus the Windows terminal, which clang here cannot build."""
    names = from_makefile(HERE, EDITOR_VARIABLES)
    names = [n for n in names if not n.endswith("terminal_win.cpp")]
    # $(TERM_SRC) is chosen by the Makefile at build time; on a Mac it is this.
    if "src/terminal.cpp" not in names:
        names.append("src/terminal.cpp")
    return sorted(set(names))


# The three, and where each one's sources come from. Everything specific to a
# project is here; nothing below this knows which one it is writing.
def projects():
    return [
        {
            "product": "RStudio.exe",
            "root": HERE,
            "out": os.path.join(HERE, "Editor.xcodeproj"),
            "sources": rstudio_sources(),
            "headers": headers_under(HERE, ("src",)),
            "include": "$(SRCROOT)/src",
            # The editor drives these two, so building it builds them first.
            # Not a link dependency - all three are separate programs and
            # nothing of cc1 or shc ends up inside ed1 - but a real ordering:
            # a change to a compiler and the change to the editor that goes
            # with it are one build and one issue list, which is the whole
            # reason for a workspace rather than three windows.
            # These strings must be the *products* the other two projects use,
            # because the remote identifiers are derived from them. Getting one
            # wrong does not make Xcode complain - it silently drops the
            # dependency and builds only this target, which is how renaming
            # cc1 to cc1.exe stopped the workspace building the compilers
            # without anything saying so.
            "depends": [("cc1.exe", "../Compiler-C/cc1.xcodeproj"),
                        ("shc.exe", "../Compiler-S/shc.xcodeproj")],
        },
        {
            "product": "cc1.exe",
            "root": os.path.join(SIBLINGS, "Compiler-C"),
            "out": os.path.join(SIBLINGS, "Compiler-C", "cc1.xcodeproj"),
            # Its Makefile says $(wildcard src/*.cpp) $(wildcard src/backend/*.cpp),
            # so the directories are the list.
            "sources": by_glob(os.path.join(SIBLINGS, "Compiler-C"),
                               ("src", "src/backend")),
            "headers": headers_under(os.path.join(SIBLINGS, "Compiler-C"), ("src",)),
            "include": "$(SRCROOT)/src $(SRCROOT)/lib",
        },
        {
            "product": "shc.exe",
            "root": os.path.join(SIBLINGS, "Compiler-S"),
            "out": os.path.join(SIBLINGS, "Compiler-S", "shc.xcodeproj"),
            # SOURCES names runtime/Shortest.cpp as well as src/, which is why
            # paths here are relative to the repository and not to src/.
            "sources": sorted(set(from_makefile(os.path.join(SIBLINGS, "Compiler-S"),
                                                ("SOURCES",)))),
            "headers": headers_under(os.path.join(SIBLINGS, "Compiler-S"),
                                     ("src", "runtime")),
            "include": "$(SRCROOT)/src $(SRCROOT)/runtime",
        },
    ]


# Xcode's own template turns on warnings none of these three Makefiles uses -
# -Wshorten-64-to-32 among them - and with warnings as errors that makes the
# project refuse a program `make` builds without complaint. cc1 failed exactly
# that way the first time this workspace was tried.
#
# The point of generating these from the Makefiles is that Xcode builds what
# make builds. A project that is *stricter* than the build it mirrors diverges
# just as surely as one that is laxer; it simply fails instead of passing. So
# the Xcode-only extras are turned off and the flag set is the Makefile's:
# -Wall -Wextra -pedantic, as errors.
COMMON = """				ALWAYS_SEARCH_USER_PATHS = NO;
				CLANG_CXX_LANGUAGE_STANDARD = "c++14";
				CLANG_ENABLE_OBJC_ARC = YES;
				CLANG_WARN_IMPLICIT_SIGN_CONVERSION = NO;
				CODE_SIGN_STYLE = Automatic;
				GCC_TREAT_WARNINGS_AS_ERRORS = YES;
				GCC_WARN_64_TO_32_BIT_CONVERSION = NO;
				MACOSX_DEPLOYMENT_TARGET = 10.15;
				PRODUCT_NAME = %s;
				SDKROOT = macosx;
				USER_HEADER_SEARCH_PATHS = "%s";
				WARNING_CFLAGS = (
					"-Wall",
					"-Wextra",
					"-pedantic",
				);"""


def project_text(spec):
    product = spec["product"]
    cpps = spec["sources"]
    hpps = spec["headers"]

    def i(*parts):
        return ident(product, *parts)

    PROJECT = i("project")
    TARGET = i("target")
    PRODUCT = i("product")
    MAIN_GROUP = i("group", "main")
    SRC_GROUP = i("group", "src")
    PRODUCTS_GROUP = i("group", "products")
    SOURCES_PHASE = i("phase", "sources")
    FRAMEWORKS = i("phase", "frameworks")
    PROJECT_CONFIGS = i("configlist", "project")
    TARGET_CONFIGS = i("configlist", "target")

    common = COMMON % (product, spec["include"])

    # Depending on a target in another project takes five objects per
    # dependency, and the remote identifiers have to be the ones that project
    # actually used - which is why ident() is seeded by product and derived
    # rather than invented. Getting one wrong gives Xcode a project it calls
    # damaged, with no clue which reference it could not follow.
    depends = spec.get("depends", [])
    dep_ids = []
    for other, where in depends:
        dep_ids.append({
            "product": other,
            "path": where,
            "file": i("projectref", other),          # the .xcodeproj on disk
            "group": i("projectproducts", other),    # its Products group, here
            "refproxy": i("referenceproxy", other),  # its product, seen from here
            "prodproxy": i("containerproxy", "product", other),
            "depproxy": i("containerproxy", "target", other),
            "dependency": i("targetdependency", other),
            # the two identifiers that belong to the *other* project
            "remote_target": ident(other, "target"),
            "remote_product": ident(other, "product"),
        })

    def config_id(which, name):
        return i("config", which, name)

    def build_configuration(which, name, extra):
        return ("\t\t%s /* %s */ = {\n\t\t\tisa = XCBuildConfiguration;\n"
                "\t\t\tbuildSettings = {\n%s\n%s\n\t\t\t};\n\t\t\tname = %s;\n\t\t};\n"
                % (config_id(which, name), name, common, extra, name))

    lines = []
    lines.append("// !$*UTF8*$!\n{\n\tarchiveVersion = 1;\n\tclasses = {\n\t};\n"
                 "\tobjectVersion = 56;\n\tobjects = {\n")

    lines.append("\n/* Begin PBXBuildFile section */\n")
    for name in cpps:
        lines.append("\t\t%s /* %s in Sources */ = {isa = PBXBuildFile; "
                     "fileRef = %s /* %s */; };\n"
                     % (i("build", name), name, i("file", name), name))
    lines.append("/* End PBXBuildFile section */\n")

    lines.append("\n/* Begin PBXFileReference section */\n")
    for name in cpps:
        lines.append("\t\t%s /* %s */ = {isa = PBXFileReference; "
                     "lastKnownFileType = sourcecode.cpp.cpp; path = %s; "
                     "sourceTree = \"<group>\"; };\n" % (i("file", name), name, name))
    for name in hpps:
        lines.append("\t\t%s /* %s */ = {isa = PBXFileReference; "
                     "lastKnownFileType = sourcecode.c.h; path = %s; "
                     "sourceTree = \"<group>\"; };\n" % (i("file", name), name, name))
    lines.append("\t\t%s /* %s */ = {isa = PBXFileReference; "
                 "explicitFileType = \"compiled.mach-o.executable\"; "
                 "includeInIndex = 0; path = %s; sourceTree = BUILT_PRODUCTS_DIR; };\n"
                 % (PRODUCT, product, product))
    for d in dep_ids:
        lines.append("\t\t%s /* %s.xcodeproj */ = {isa = PBXFileReference; "
                     "lastKnownFileType = \"wrapper.pb-project\"; name = %s.xcodeproj; "
                     "path = %s; sourceTree = \"<group>\"; };\n"
                     % (d["file"], d["product"], d["product"], d["path"]))
    lines.append("/* End PBXFileReference section */\n")

    if dep_ids:
        lines.append("\n/* Begin PBXContainerItemProxy section */\n")
        for d in dep_ids:
            # proxyType 2 is the other project's product; 1 is its target.
            lines.append("\t\t%s /* PBXContainerItemProxy */ = {\n"
                         "\t\t\tisa = PBXContainerItemProxy;\n"
                         "\t\t\tcontainerPortal = %s /* %s.xcodeproj */;\n"
                         "\t\t\tproxyType = 2;\n"
                         "\t\t\tremoteGlobalIDString = %s;\n"
                         "\t\t\tremoteInfo = %s;\n\t\t};\n"
                         % (d["prodproxy"], d["file"], d["product"],
                            d["remote_product"], d["product"]))
            lines.append("\t\t%s /* PBXContainerItemProxy */ = {\n"
                         "\t\t\tisa = PBXContainerItemProxy;\n"
                         "\t\t\tcontainerPortal = %s /* %s.xcodeproj */;\n"
                         "\t\t\tproxyType = 1;\n"
                         "\t\t\tremoteGlobalIDString = %s;\n"
                         "\t\t\tremoteInfo = %s;\n\t\t};\n"
                         % (d["depproxy"], d["file"], d["product"],
                            d["remote_target"], d["product"]))
        lines.append("/* End PBXContainerItemProxy section */\n")

    lines.append("\n/* Begin PBXFrameworksBuildPhase section */\n")
    lines.append("\t\t%s /* Frameworks */ = {\n\t\t\tisa = PBXFrameworksBuildPhase;\n"
                 "\t\t\tbuildActionMask = 2147483647;\n\t\t\tfiles = (\n\t\t\t);\n"
                 "\t\t\trunOnlyForDeploymentPostprocessing = 0;\n\t\t};\n" % FRAMEWORKS)
    lines.append("/* End PBXFrameworksBuildPhase section */\n")

    lines.append("\n/* Begin PBXGroup section */\n")
    referenced = "".join("\t\t\t\t%s /* %s.xcodeproj */,\n" % (d["file"], d["product"])
                         for d in dep_ids)
    lines.append("\t\t%s = {\n\t\t\tisa = PBXGroup;\n\t\t\tchildren = (\n"
                 "\t\t\t\t%s /* Sources */,\n%s\t\t\t\t%s /* Products */,\n"
                 "\t\t\t);\n\t\t\tsourceTree = \"<group>\";\n\t\t};\n"
                 % (MAIN_GROUP, SRC_GROUP, referenced, PRODUCTS_GROUP))
    children = "".join("\t\t\t\t%s /* %s */,\n" % (i("file", n), n) for n in cpps + hpps)
    # The group has a name and no path: every file carries its own path from
    # the repository root, which is the only shape that takes src/, src/backend/
    # and runtime/ in one list.
    lines.append("\t\t%s /* Sources */ = {\n\t\t\tisa = PBXGroup;\n\t\t\tchildren = (\n%s"
                 "\t\t\t);\n\t\t\tname = Sources;\n\t\t\tsourceTree = \"<group>\";\n\t\t};\n"
                 % (SRC_GROUP, children))
    lines.append("\t\t%s /* Products */ = {\n\t\t\tisa = PBXGroup;\n\t\t\tchildren = (\n"
                 "\t\t\t\t%s /* %s */,\n\t\t\t);\n\t\t\tname = Products;\n"
                 "\t\t\tsourceTree = \"<group>\";\n\t\t};\n"
                 % (PRODUCTS_GROUP, PRODUCT, product))
    for d in dep_ids:
        lines.append("\t\t%s /* Products */ = {\n\t\t\tisa = PBXGroup;\n"
                     "\t\t\tchildren = (\n\t\t\t\t%s /* %s */,\n\t\t\t);\n"
                     "\t\t\tname = Products;\n\t\t\tsourceTree = \"<group>\";\n\t\t};\n"
                     % (d["group"], d["refproxy"], d["product"]))
    lines.append("/* End PBXGroup section */\n")

    if dep_ids:
        lines.append("\n/* Begin PBXReferenceProxy section */\n")
        for d in dep_ids:
            lines.append("\t\t%s /* %s */ = {\n\t\t\tisa = PBXReferenceProxy;\n"
                         "\t\t\tfileType = \"compiled.mach-o.executable\";\n"
                         "\t\t\tpath = %s;\n\t\t\tremoteRef = %s /* PBXContainerItemProxy */;\n"
                         "\t\t\tsourceTree = BUILT_PRODUCTS_DIR;\n\t\t};\n"
                         % (d["refproxy"], d["product"], d["product"], d["prodproxy"]))
        lines.append("/* End PBXReferenceProxy section */\n")

    lines.append("\n/* Begin PBXNativeTarget section */\n")
    lines.append("\t\t%s /* %s */ = {\n\t\t\tisa = PBXNativeTarget;\n"
                 "\t\t\tbuildConfigurationList = %s;\n\t\t\tbuildPhases = (\n"
                 "\t\t\t\t%s /* Sources */,\n\t\t\t\t%s /* Frameworks */,\n\t\t\t);\n"
                 "\t\t\tbuildRules = (\n\t\t\t);\n\t\t\tdependencies = (\n%s\t\t\t);\n"
                 "\t\t\tname = %s;\n\t\t\tproductName = %s;\n"
                 "\t\t\tproductReference = %s /* %s */;\n"
                 "\t\t\tproductType = \"com.apple.product-type.tool\";\n\t\t};\n"
                 % (TARGET, product, TARGET_CONFIGS, SOURCES_PHASE, FRAMEWORKS,
                    "".join("\t\t\t\t%s /* PBXTargetDependency */,\n" % d["dependency"]
                            for d in dep_ids),
                    product, product, PRODUCT, product))
    lines.append("/* End PBXNativeTarget section */\n")

    if dep_ids:
        lines.append("\n/* Begin PBXTargetDependency section */\n")
        for d in dep_ids:
            lines.append("\t\t%s /* PBXTargetDependency */ = {\n"
                         "\t\t\tisa = PBXTargetDependency;\n\t\t\tname = %s;\n"
                         "\t\t\ttargetProxy = %s /* PBXContainerItemProxy */;\n\t\t};\n"
                         % (d["dependency"], d["product"], d["depproxy"]))
        lines.append("/* End PBXTargetDependency section */\n")

    lines.append("\n/* Begin PBXProject section */\n")
    lines.append("\t\t%s /* Project object */ = {\n\t\t\tisa = PBXProject;\n"
                 "\t\t\tattributes = {\n\t\t\t\tBuildIndependentTargetsInParallel = 1;\n"
                 "\t\t\t\tLastUpgradeCheck = 1600;\n\t\t\t};\n"
                 "\t\t\tbuildConfigurationList = %s;\n"
                 "\t\t\tdevelopmentRegion = en;\n\t\t\thasScannedForEncodings = 0;\n"
                 "\t\t\tknownRegions = (\n\t\t\t\ten,\n\t\t\t\tBase,\n\t\t\t);\n"
                 "\t\t\tmainGroup = %s;\n\t\t\tproductRefGroup = %s /* Products */;\n"
                 "\t\t\tprojectDirPath = \"\";\n%s\t\t\tprojectRoot = \"\";\n"
                 "\t\t\ttargets = (\n\t\t\t\t%s /* %s */,\n\t\t\t);\n\t\t};\n"
                 % (PROJECT, PROJECT_CONFIGS, MAIN_GROUP, PRODUCTS_GROUP,
                    ("\t\t\tprojectReferences = (\n" +
                     "".join("\t\t\t\t{\n\t\t\t\t\tProductGroup = %s /* Products */;\n"
                             "\t\t\t\t\tProjectRef = %s /* %s.xcodeproj */;\n\t\t\t\t},\n"
                             % (d["group"], d["file"], d["product"]) for d in dep_ids) +
                     "\t\t\t);\n") if dep_ids else "",
                    TARGET, product))
    lines.append("/* End PBXProject section */\n")

    lines.append("\n/* Begin PBXSourcesBuildPhase section */\n")
    compiled = "".join("\t\t\t\t%s /* %s in Sources */,\n" % (i("build", n), n)
                       for n in cpps)
    lines.append("\t\t%s /* Sources */ = {\n\t\t\tisa = PBXSourcesBuildPhase;\n"
                 "\t\t\tbuildActionMask = 2147483647;\n\t\t\tfiles = (\n%s\t\t\t);\n"
                 "\t\t\trunOnlyForDeploymentPostprocessing = 0;\n\t\t};\n"
                 % (SOURCES_PHASE, compiled))
    lines.append("/* End PBXSourcesBuildPhase section */\n")

    lines.append("\n/* Begin XCBuildConfiguration section */\n")
    debug = "\t\t\t\tDEBUG_INFORMATION_FORMAT = dwarf;\n\t\t\t\tGCC_OPTIMIZATION_LEVEL = 0;"
    release = ("\t\t\t\tDEBUG_INFORMATION_FORMAT = \"dwarf-with-dsym\";\n"
               "\t\t\t\tGCC_OPTIMIZATION_LEVEL = 2;")
    for which in ("project", "target"):
        lines.append(build_configuration(which, "Debug", debug))
        lines.append(build_configuration(which, "Release", release))
    lines.append("/* End XCBuildConfiguration section */\n")

    lines.append("\n/* Begin XCConfigurationList section */\n")
    for listing, which in ((PROJECT_CONFIGS, "project"), (TARGET_CONFIGS, "target")):
        lines.append("\t\t%s /* %s */ = {\n\t\t\tisa = XCConfigurationList;\n"
                     "\t\t\tbuildConfigurations = (\n"
                     "\t\t\t\t%s /* Debug */,\n\t\t\t\t%s /* Release */,\n\t\t\t);\n"
                     "\t\t\tdefaultConfigurationIsVisible = 0;\n"
                     "\t\t\tdefaultConfigurationName = Debug;\n\t\t};\n"
                     % (listing, which, config_id(which, "Debug"),
                        config_id(which, "Release")))
    lines.append("/* End XCConfigurationList section */\n")

    lines.append("\t};\n\trootObject = %s /* Project object */;\n}\n" % PROJECT)
    return "".join(lines)


def guid(product):
    """A stable GUID for a generated .vcxproj, derived from the product name.

    Visual Studio wants one per project and wants the solution to agree with
    the project file about it. Deriving it means the two cannot disagree and a
    regenerated project is the same file, which is what --check rests on.
    """
    d = hashlib.sha1(("rstudio-vcxproj:" + product).encode()).hexdigest().upper()
    return "{%s-%s-%s-%s-%s}" % (d[:8], d[8:12], d[12:16], d[16:20], d[20:32])


# **shc is not one file.** It links a runtime archive that it looks for beside
# its own binary - lib/ next to it, then ../lib - so a project that builds
# shc.exe and nothing else produces a compiler that compiles, writes correct
# assembly, and then dies at the link with
#
#   LINK : fatal error LNK1181: cannot open input file
#          '...\x64\Release\lib\shmrt-x86_64-windows-debug.lib'
#
# which reads as a broken compiler rather than an incomplete directory. That is
# what RStudio.sln produced until 2026-08-23, and no build and no suite could
# see it: -S needs no runtime, so only pressing Run on a Shalimar file said so.
#
# Twice from the same sources, as Compiler-S/build.bat does it: the release
# archive holds no debugger code at all, and the debug one is the same program
# plus a session dormant until SHM_DEBUG arms it. Both are named because a
# debug build links the second, and shipping one would leave exactly the half
# about to be used missing.
#
# Two things here are not typos. cl will not create the directory /Fo names -
# it says so three files later as "Cannot open compiler generated file", which
# reads as a disk problem - so the object directories are made first. And the
# doubled backslash in /Fo is required: a single one before a closing quote
# escapes the quote, and cl then answers "D8003: missing source filename".
SHC_RUNTIME_SOURCES = ("Shortest", "Failure", "Numbers", "Array", "Console", "Runtime")

def shc_runtime_step():
    """The PostBuildEvent that puts shc's runtime in lib/ beside shc.exe."""
    def compiled(names, into):
        return " ".join('"$(ProjectDir)runtime\%s.cpp"' % n for n in names), \
               " ".join('"$(IntDir)%s\%s.obj"' % (into, n) for n in names)

    release_src, release_obj = compiled(SHC_RUNTIME_SOURCES, "rt")
    debug_src, debug_obj = compiled(SHC_RUNTIME_SOURCES + ("Debug",), "rtd")
    flags = ("/nologo /std:c++14 /W4 /WX /EHsc /permissive- /O2 "
             "/D_CRT_SECURE_NO_WARNINGS")

    return (
        '    <PostBuildEvent>\n'
        '      <Message>building the Shalimar runtime beside shc.exe</Message>\n'
        '      <Command>if not exist "$(OutDir)lib" mkdir "$(OutDir)lib"\n'
        'if not exist "$(IntDir)rt" mkdir "$(IntDir)rt"\n'
        'if not exist "$(IntDir)rtd" mkdir "$(IntDir)rtd"\n'
        'cl %s /Fo"$(IntDir)rt\\\\" /c %s\n'
        'if errorlevel 1 exit /b 1\n'
        'lib /nologo /out:"$(OutDir)lib\shmrt-x86_64-windows.lib" %s\n'
        'if errorlevel 1 exit /b 1\n'
        'cl %s /DSHM_DEBUG=1 /Fo"$(IntDir)rtd\\\\" /c %s\n'
        'if errorlevel 1 exit /b 1\n'
        'lib /nologo /out:"$(OutDir)lib\shmrt-x86_64-windows-debug.lib" %s\n'
        'if errorlevel 1 exit /b 1</Command>\n'
        '    </PostBuildEvent>\n'
        % (flags, release_src, release_obj, flags, debug_src, debug_obj))


def vcxproj_text(product, sources, defines, extra=""):
    """A command line tool for MSVC, held to the same flags build.bat uses.

    /std:c++14 /W4 /WX /EHsc /permissive- - the same four this project has
    always been built with on that machine, so the solution and build.bat
    produce the same program rather than two that differ in what they refused.
    """
    configurations = "".join(
        '    <ProjectConfiguration Include="%s|x64">\n'
        '      <Configuration>%s</Configuration>\n'
        '      <Platform>x64</Platform>\n'
        '    </ProjectConfiguration>\n' % (c, c) for c in ("Debug", "Release"))

    per_config = ""
    for c, debug_libraries, optimisation in (("Debug", "true", "Disabled"),
                                             ("Release", "false", "MaxSpeed")):
        per_config += (
            '  <PropertyGroup Condition="\'$(Configuration)|$(Platform)\'==\'%s|x64\'" '
            'Label="Configuration">\n'
            '    <ConfigurationType>Application</ConfigurationType>\n'
            '    <UseDebugLibraries>%s</UseDebugLibraries>\n'
            '    <PlatformToolset>v143</PlatformToolset>\n'
            '    <CharacterSet>MultiByte</CharacterSet>\n'
            '  </PropertyGroup>\n' % (c, debug_libraries))

    definitions = ";".join(defines + ["%(PreprocessorDefinitions)"])
    compiled = "".join('    <ClCompile Include="%s" />\n' % s.replace("/", "\\")
                       for s in sources)

    return (
        '<?xml version="1.0" encoding="utf-8"?>\n'
        '<Project DefaultTargets="Build" ToolsVersion="17.0" '
        'xmlns="http://schemas.microsoft.com/developer/msbuild/2003">\n'
        '  <ItemGroup Label="ProjectConfigurations">\n%s  </ItemGroup>\n'
        '  <PropertyGroup Label="Globals">\n'
        '    <VCProjectVersion>17.0</VCProjectVersion>\n'
        '    <ProjectGuid>%s</ProjectGuid>\n'
        '    <RootNamespace>%s</RootNamespace>\n'
        '    <WindowsTargetPlatformVersion>10.0</WindowsTargetPlatformVersion>\n'
        '  </PropertyGroup>\n'
        '  <Import Project="$(VCTargetsPath)\\Microsoft.Cpp.Default.props" />\n%s'
        '  <Import Project="$(VCTargetsPath)\\Microsoft.Cpp.props" />\n'
        '  <PropertyGroup>\n'
        '    <TargetName>%s</TargetName>\n'
        '  </PropertyGroup>\n'
        # OutDir is deliberately not set. The C++ default is already the rule
        # these projects want, and is the msbuild spelling of the Makefile's
        # BINDIR: built alone the program lands in this project's own
        # x64\\$(Configuration)\\, and built from a solution it lands in the
        # solution's output directory, beside the editor that drives it.
        # Compiler-C/msvc/cc1.vcxproj is hand-kept and had to override that
        # default; it now undoes the override for this same reason.
        '  <!-- OutDir is left to the C++ default on purpose: this project\'s\n'
        '       own x64\\$(Configuration)\\ when built alone, and the solution\'s\n'
        '       output directory when built from one - which is what puts the\n'
        '       editor and the compilers it drives in one place. -->\n'
        '  <ItemDefinitionGroup>\n'
        '    <ClCompile>\n'
        '      <LanguageStandard>stdcpp14</LanguageStandard>\n'
        '      <WarningLevel>Level4</WarningLevel>\n'
        '      <TreatWarningAsError>true</TreatWarningAsError>\n'
        '      <ExceptionHandling>Sync</ExceptionHandling>\n'
        '      <ConformanceMode>true</ConformanceMode>\n'
        '      <PreprocessorDefinitions>%s</PreprocessorDefinitions>\n'
        '    </ClCompile>\n'
        '    <Link>\n'
        '      <SubSystem>Console</SubSystem>\n'
        '    </Link>\n'
        '%s'
        '  </ItemDefinitionGroup>\n'
        '  <ItemGroup>\n%s  </ItemGroup>\n'
        '  <Import Project="$(VCTargetsPath)\\Microsoft.Cpp.targets" />\n'
        '</Project>\n'
        % (configurations, guid(product), product, per_config, product,
           definitions, extra, compiled))


SOLUTION_FOLDER = "{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}"


def cc1_guid():
    """cc1's own GUID, read out of the project it already has.

    Not derived like the other two: that file is hand-kept in Compiler-C and
    the solution has to name the GUID it actually uses. Reading it is the only
    way the two cannot drift apart.
    """
    return guid_in(os.path.join(SIBLINGS, "Compiler-C", "msvc", "cc1.vcxproj"),
                   "cc1's own project")


def guid_in(where, what):
    """The ProjectGuid a hand-kept .vcxproj already has.

    Two of the four projects in the solution are not written here - cc1's,
    which belongs to another repository, and the window's, which is C++/CLI and
    whose settings were arrived at the hard way. The solution has to name the
    GUID each of them actually uses, and reading it is the only way the two
    cannot drift apart.
    """
    if not os.path.exists(where):
        sys.exit("no %s - the solution needs %s" % (where, what))
    match = re.search(r"<ProjectGuid>(\{[0-9A-Fa-f-]+\})</ProjectGuid>", open(where).read())
    if not match:
        sys.exit("no ProjectGuid in %s" % where)
    return match.group(1).upper()


CC1_GUID = cc1_guid()
GUI_GUID = guid_in(os.path.join(HERE, "winforms", "RStudioGui.vcxproj"),
                   "the window's own project")


def solution_text(entries):
    """RStudio.sln - the three, with winconsole depending on both compilers.

    A .sln says a dependency with ProjectSection(ProjectDependencies), which
    lists the GUIDs a project must be built after. That is the same idea as the
    Xcode workspace's target dependencies and it is written here for the same
    reason: a change to a compiler and the change to the editor that goes with
    it should be one build.
    """
    out = ("Microsoft Visual Studio Solution File, Format Version 12.00\n"
           "# Visual Studio Version 17\n"
           "VisualStudioVersion = 17.0.31903.59\n"
           "MinimumVisualStudioVersion = 10.0.40219.1\n")

    for name, path, project_guid, after in entries:
        out += 'Project("%s") = "%s", "%s", "%s"\n' % (
            SOLUTION_FOLDER, name, path.replace("/", "\\"), project_guid)
        if after:
            out += "\tProjectSection(ProjectDependencies) = postProject\n"
            for other in after:
                out += "\t\t%s = %s\n" % (other, other)
            out += "\tEndProjectSection\n"
        out += "EndProject\n"

    out += ("Global\n"
            "\tGlobalSection(SolutionConfigurationPlatforms) = preSolution\n"
            "\t\tDebug|x64 = Debug|x64\n\t\tRelease|x64 = Release|x64\n"
            "\tEndGlobalSection\n"
            "\tGlobalSection(ProjectConfigurationPlatforms) = postSolution\n")
    for _, _, project_guid, _ in entries:
        for c in ("Debug", "Release"):
            out += "\t\t%s.%s|x64.ActiveCfg = %s|x64\n" % (project_guid, c, c)
            out += "\t\t%s.%s|x64.Build.0 = %s|x64\n" % (project_guid, c, c)
    out += ("\tEndGlobalSection\n"
            "\tGlobalSection(SolutionProperties) = preSolution\n"
            "\t\tHideSolutionNode = FALSE\n\tEndGlobalSection\n"
            "EndGlobal\n")
    return out


def workspace_mk_text():
    """The Linux answer, which is a Makefile because that is what Linux has.

    There is no workspace to open on that box and inventing one would be worse
    than using what is already there: three Makefiles that work. This recurses
    into all three and gives ed1 the same dependency it has in the other two,
    so `make -f workspace.mk` builds the compilers and then the editor.
    """
    return """# The three programs, built together. Generated by tools/make-projects.py.
#
#   make -f workspace.mk           the compilers, then the editor
#   make -f workspace.mk bin       and all three binaries in bin/
#   make -f workspace.mk check     and run every suite
#   make -f workspace.mk clean
#
# This is the Linux half of what RStudio.xcworkspace is on a Mac and
# RStudio.sln is on Windows: one thing to build, with ed1 after the two
# compilers it drives. It does not reimplement any of their builds - it calls
# the Makefile each repository already has, which is the only way this can stay
# true when one of them changes.
#
# The three are expected side by side. That is the one assumption here, and it
# is the same one the workspace and the solution make.

# Overridable, because they are not called this everywhere. On the Linux box
# the same two repositories are ~/ansicc and ~/shalimar:
#
#   make -f workspace.mk CC1_DIR=$HOME/ansicc SHC_DIR=$HOME/shalimar
CC1_DIR ?= ../Compiler-C
SHC_DIR ?= ../Compiler-S

# ---- one directory, named once and given to all three -----------------------
#
# Each of the three Makefiles takes a BINDIR saying where its finished program
# goes, and each defaults to its own repository - so building any one of them
# alone is exactly what it always was. This is the only place that overrides
# the three at once, because this is the only build that knows all three exist.
#
# The default is RStudio's own root, which is where RStudio.exe is built and
# therefore the directory the editor searches first: it finds the compilers it
# drives with path::besideProgram, before PATH, so that a compiler shipped with
# this copy is the one this copy runs.
#
# **Built into, not collected into.** The rule this replaces built the three in
# three places and copied them here afterwards, and a copy step is a step that
# can be incomplete - shc.exe arrived without the runtime archives it links,
# which no build and no suite could see and only pressing Run revealed.
# Nothing is copied now; the three are simply told where to write.
#
# Absolute, because each sub-make runs in its own directory and a relative path
# would mean three different places.
BINDIR ?= $(CURDIR)
OUT := $(abspath $(BINDIR))

.PHONY: all cc1 shc editor confirm bin check clean

# `confirm` and not `editor`, so that the last thing a workspace build does is
# check that what the editor drives is actually beside it.
all: confirm

cc1:
	$(MAKE) -C $(CC1_DIR) BINDIR=$(OUT)

shc:
	$(MAKE) -C $(SHC_DIR) BINDIR=$(OUT)

# The dependency, said the same way it is said in the other two: the editor is
# built after the compilers it drives. Nothing of them ends up inside it.
editor: cc1 shc
	$(MAKE) BINDIR=$(OUT)

# Asked of RStudio rather than answered here. The editor is the thing that
# knows what it drives - the list is in its own Makefile, beside the code that
# goes looking for them - and this only calls it once all three have been
# built into one place.
confirm: editor
	$(MAKE) BINDIR=$(OUT) confirm

# Each suite is that project's own, called by the name that project uses -
# both compilers say `test` and only the editor says `check`. Calling `check`
# on all three was written first and failed on the first run, which is the
# argument for running one of these before believing it.
#
# And cc1's `test` is a Linux suite. It compares against gcc and runs x86-64
# binaries, so on a Mac it refuses and names what does run here instead. That
# refusal is cc1 being right, so this asks the host and runs what that host
# can - rather than the alternatives, which are to skip cc1 on a Mac or to
# ignore a failure and lose the real ones with it.
HOST := $(shell uname -s)

check: confirm
ifeq ($(HOST),Darwin)
	cd $(CC1_DIR) && ./tests/arm64.sh
	cd $(CC1_DIR) && ./tests/fingerprint.sh
else
	$(MAKE) -C $(CC1_DIR) test
endif
	$(MAKE) -C $(SHC_DIR) test
# The two just built into $(OUT), and not the copies in the compilers' own
# trees. Those are usually the same file and occasionally are not, and the
# occasion is exactly the one worth catching: this build wrote its compilers
# somewhere, and this is the suite that says whether what it wrote works.
#
# Naming the wrong ones does not fail - the editor's suite skips the cases that
# need a compiler and says so quietly - so the count fell from 792 and 232 to
# 686 and 115 and everything still read as green. A suite that skips is not a
# suite that passes.
	$(MAKE) BINDIR=$(OUT) check CC1=$(OUT)/cc1.exe SHC=$(OUT)/shc.exe

# The alternative destination, for anyone who would rather the checkout root
# stayed as it was. Nothing is copied into it - see the `bin` rule below.
BIN := bin

# Emptied first. A binary that was renamed leaves its old self here otherwise,
# and a directory holding both cc1 and cc1.exe is one where nobody can say
# which was run.
# The same build, into bin/ instead of into the root - for anyone who would
# rather the checkout stayed clean. It is one line now because the three
# already take a BINDIR: this names a different one and gets out of the way.
#
# It used to be a second collector with a second destination, which is what
# made it possible for it to collect the wrong set. There is nothing here to
# get wrong any more.
bin:
	$(MAKE) -f workspace.mk BINDIR=$(CURDIR)/$(BIN)

clean:
	rm -rf $(BIN)
	$(MAKE) -C $(CC1_DIR) BINDIR=$(OUT) clean
	$(MAKE) -C $(SHC_DIR) BINDIR=$(OUT) clean
	$(MAKE) BINDIR=$(OUT) clean
"""


def workspace_text(specs):
    """RStudio.xcworkspace - the three projects, opened together.

    The editor first, because that is what somebody is usually here for, and
    the two compilers under it in the order the editor reaches for them.
    """
    rows = []
    for spec in specs:
        where = os.path.relpath(spec["out"], HERE)
        rows.append('   <FileRef\n      location = "group:%s">\n   </FileRef>\n' % where)
    return ('<?xml version="1.0" encoding="UTF-8"?>\n<Workspace\n   version = "1.0">\n'
            + "".join(rows) + '</Workspace>\n')


# ---- the projects that are kept by hand ------------------------------------
#
# Two of them, and neither can be generated for the same kind of reason: what
# is in them besides the source list is load-bearing and is not derivable from
# any Makefile.
#
# winforms/RStudioGui.vcxproj is C++/CLI. One file is compiled managed and
# every other file must be compiled native - a /clr translation unit that
# instantiates the same templates the native ones do corrupts the heap before
# main is reached, which is the first hazard in that directory's README. Those
# per-file settings are the project's whole reason for existing.
#
# Compiler-C/msvc/cc1.vcxproj belongs to another repository and works.
#
# So they are checked rather than written. The source list is the part that
# drifts - a file added to a Makefile and forgotten here - and it is the part
# that can be compared. This is what the comment in main() used to promise and
# nothing did: it said the sources were counted "below", and they were not.
def hand_kept_sources(path, inside):
    """The .cpp a hand-kept .vcxproj compiles, as paths inside its repository.

    A .vcxproj names its files relative to itself, so `..\src\Lexer.cpp` in
    msvc/ and `src/Lexer.cpp` from the Makefile are one file written two ways.
    """
    found = set()
    for named in re.findall(r'<ClCompile Include="([^"]+)"', open(path).read()):
        joined = os.path.normpath(os.path.join(inside, named.replace("\\", "/")))
        found.add(joined.replace(os.sep, "/"))
    return found


def window_sources():
    """What the window's project has to compile: the core, and its own two.

    Not TERMINAL_SRC, which is the other front end's drawing - and this is
    exactly the split CORE_SRC was made to write down.
    """
    core = from_makefile(HERE, ("CORE_SRC", "SHM_SRC"))
    return set(core) | {"winforms/bridge.cpp", "winforms/Program.cpp"}


def drift(path, inside, wanted):
    """What a hand-kept project and its Makefile disagree about, in words."""
    if not os.path.exists(path):
        return "there is no %s" % path
    there = hand_kept_sources(path, inside)
    missing = sorted(wanted - there)
    extra = sorted(there - wanted)
    said = []
    if missing:
        said.append("does not compile " + ", ".join(missing))
    if extra:
        said.append("compiles " + ", ".join(extra) + ", which no Makefile names")
    return "; ".join(said)


def named_sources(text, path):
    """Every source file a generated project names, as a set.

    Which files are built is the thing --check is about; the rest of a project
    file is arrangement. That distinction is not fussiness: **Xcode rewrites a
    .pbxproj when it builds it**, sorting the sections by identifier instead of
    by name. A byte-for-byte check therefore fails after every Xcode build and
    says the project has drifted when nothing has, which is a check nobody
    would keep for long.
    """
    if path.endswith(".pbxproj"):
        return set(re.findall(r'/\* ([^ *]+\.cpp) in Sources \*/', text))
    if path.endswith(".vcxproj"):
        return set(re.findall(r'<ClCompile Include="([^"]+)"', text))
    return None


def same_sources(there, wanted_text, path):
    """Whether what is on disk builds what the Makefile says it should.

    For the project formats, the source lists are compared. For everything else
    - the workspace, the solution, workspace.mk - the whole text is, because
    nothing rewrites those and every line in them was put there on purpose.
    """
    mine = named_sources(wanted_text, path)
    if mine is None:
        return there == wanted_text
    return named_sources(there, path) == mine


def main():
    checking = "--check" in sys.argv[1:]
    specs = projects()
    stale = []

    # Before anything is read: the Makefile must be made of the variables this
    # script knows about and no others. Silence here is what cost two builds.
    composition = {"SRC": {"CORE_SRC", "TERMINAL_SRC"},
                   "OBJ": {"SRC", "SHM_SRC", "OBJDIR"}}
    for variable, expected in composition.items():
        found = composed_of(HERE, variable)
        if found != expected:
            print("%s in the Makefile is made of %s, and this script reads %s."
                  % (variable, ", ".join(sorted(found)) or "nothing",
                     ", ".join(sorted(expected))))
            print("Teach tools/make-projects.py about the difference, or every")
            print("project here will build something other than what make builds.")
            return 1

    wanted = [(os.path.join(s["out"], "project.pbxproj"), project_text(s), s["product"])
              for s in specs]
    wanted.append((os.path.join(HERE, "RStudio.xcworkspace", "contents.xcworkspacedata"),
                   workspace_text(specs), "RStudio.xcworkspace"))

    # ---- Windows -----------------------------------------------------------
    #
    # cc1 already has a project on that machine - Compiler-C/msvc/cc1.vcxproj,
    # kept by hand and working - so the solution references it rather than
    # writing over it. It is not written here; its sources are checked against
    # its Makefile at the end of this function, with the window's.
    windows_sources = [n for n in from_makefile(HERE, EDITOR_VARIABLES)
                       if not n.endswith("terminal.cpp")]
    if "src/terminal_win.cpp" not in windows_sources:
        windows_sources.append("src/terminal_win.cpp")

    wanted.append((os.path.join(HERE, "RStudioConsole.vcxproj"),
                   vcxproj_text("RStudioConsole", sorted(set(windows_sources)),
                                ["_CRT_SECURE_NO_WARNINGS"]),
                   "RStudioConsole.vcxproj"))
    wanted.append((os.path.join(SIBLINGS, "Compiler-S", "shc.vcxproj"),
                   vcxproj_text("shc", specs[2]["sources"], ["_CRT_SECURE_NO_WARNINGS"],
                                shc_runtime_step()),
                   "shc.vcxproj"))

    entries = [
        ("cc1", "../Compiler-C/msvc/cc1.vcxproj", CC1_GUID, []),
        ("shc", "../Compiler-S/shc.vcxproj", guid("shc"), []),
        # the editor after both, which is the dependency this whole thing is
        # for - said in a .sln the way the workspace says it in a .xcodeproj.
        ("RStudioConsole", "RStudioConsole.vcxproj", guid("RStudioConsole"), [CC1_GUID, guid("shc")]),
        # The window, on the same footing as the console half. It is in the
        # solution for two reasons: so that one build makes all four, and
        # because being in a solution is what moves its output into the
        # solution's directory beside the rest - the C++ default does that on
        # its own, so the project file itself needs no OutDir. That matters
        # here: winforms/RStudioGui.vcxproj carries a warning that an earlier
        # version of it set OutDir, IntDir, BasicRuntimeChecks and a platform
        # version, and the binary died at startup with heap corruption before
        # main. Nothing in that file is touched to get this.
        ("RStudioGui", "winforms/RStudioGui.vcxproj", GUI_GUID, [CC1_GUID, guid("shc")]),
    ]
    wanted.append((os.path.join(HERE, "RStudio.sln"), solution_text(entries),
                   "RStudio.sln"))

    # ---- Linux -------------------------------------------------------------
    wanted.append((os.path.join(HERE, "workspace.mk"), workspace_mk_text(),
                   "workspace.mk"))

    # The two kept by hand, checked and never written - see hand_kept_sources.
    for what, path, inside, wanted_sources in (
            ("winforms/RStudioGui.vcxproj",
             os.path.join(HERE, "winforms", "RStudioGui.vcxproj"),
             "winforms", window_sources()),
            ("Compiler-C/msvc/cc1.vcxproj",
             os.path.join(SIBLINGS, "Compiler-C", "msvc", "cc1.vcxproj"),
             "msvc", set(by_glob(os.path.join(SIBLINGS, "Compiler-C"),
                                 ("src", "src/backend"))))):
        wrong = drift(path, inside, wanted_sources)
        if wrong:
            stale.append("%s (%s)" % (what, wrong))

    for path, text, what in wanted:
        if checking:
            there = open(path).read() if os.path.exists(path) else None
            if there is None or not same_sources(there, text, path):
                stale.append(what)
            continue
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w") as f:
            f.write(text)

    if stale:
        print("out of date with the Makefiles:\n  " + "\n  ".join(stale))
        print("A project that builds fewer files than make does is not an error -")
        print("it is a smaller program, and nothing says so.")
        print("  python3 tools/make-projects.py       for the generated ones")
        print("  the two hand-kept ones are edited by hand, on purpose")
        return 1

    if checking:
        print("all three projects and the workspace are what the Makefiles say,")
        print("and so are the two kept by hand - the window's and cc1's")
        return 0

    for spec in specs:
        print("%-4s %d sources, %d headers  ->  %s"
              % (spec["product"], len(spec["sources"]), len(spec["headers"]),
                 os.path.relpath(spec["out"], SIBLINGS)))
    print("RStudio.xcworkspace  opens all three on a Mac")
    print("RStudio.sln          all four for Visual Studio 2022")
    print("workspace.mk         and for make on the Linux box")
    return 0


if __name__ == "__main__":
    sys.exit(main())
