#
# CMake port of the original VC6 workspace (Code/commando.dsw).
#
# MSBuild cannot read .dsp/.dsw at all, so this replaces the workspace rather
# than converting it. Source lists still come from the .dsp files -- see
# cmake/Sources.cmake and the generator that produces it.
#
# Build (Visual Studio generator, when your CMake knows your VS version):
#   cmake -B build -A Win32 .
#   cmake --build build --config Release
#
# Build (any CMake, from an x86 developer prompt -- needed if CMake predates
# your Visual Studio, since the VS generator will not find it):
#   vcvarsall.bat x86
#   cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release .
#   cmake --build build
#
# 32-bit only. 35 files contain x86 inline assembly and the DirectX 8 import
# libraries are 32-bit, so there is no x64 path short of a substantial port.
#

cmake_minimum_required(VERSION 3.20)

# Windows-MSVC.cmake turns on cmcldeps for .rc dependency scanning on any
# Windows host, but cmcldeps.exe ships only with the native CMake build --
# an MSYS2/MinGW cmake does not have it. The Ninja generator still emits the
# cmcldeps-shaped rule with an empty program, so the command collapses to the
# bare word "RC" and rc.exe answers RC1107: invalid usage. chat.rc's
# includes are stable, so scanning them buys nothing; run rc.exe directly.
# Must be set before project(), which is where Windows-MSVC.cmake defaults it.
set(CMAKE_NINJA_CMCLDEPS_RC 0)

project(Renegade
    DESCRIPTION "Command & Conquer Renegade"
    LANGUAGES CXX C RC)

if(NOT MSVC)
    message(FATAL_ERROR "This tree only builds with MSVC. It relies on x86 inline "
                        "assembly, __declspec, and Win32 SDK headers throughout.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR
        "Renegade is a 32-bit target; this generator is configured for "
        "${CMAKE_SIZEOF_VOID_P}-byte pointers.\n"
        "Reconfigure with:  cmake -B build -A Win32 .\n"
        "or configure from an x86 developer prompt with the Ninja generator.")
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/Run")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Libs")

# The original projects link the static CRT.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

option(RENEGADE_EXCLUDE_UNPORTED
    "Skip the translation units that do not yet compile under modern MSVC" ON)
option(RENEGADE_ENGINE_WARNINGS
    "Compile the engine libraries with diagnostics on (they build at /W0)" OFF)
# WWMath::Inv_Sqrt is a __declspec(naked) x87 routine that writes to [esp-8]
# and [esp-12] -- below the stack pointer, which Win32 has no red zone for.
# Turning this on substitutes plain C, to isolate it when debugging.
option(RENEGADE_PORTABLE_INV_SQRT
    "Use plain C for WWMath::Inv_Sqrt instead of the naked x87 version" OFF)
option(RENEGADE_BUILD_TESTS
    "Build the wwmath/wwlib unit tests" ON)
option(RENEGADE_MILES_STUB
    "Build against the no-op Miles Sound System stub instead of the real SDK" ON)

set(CODE "${CMAKE_SOURCE_DIR}/Code")
set(STUBS "${CODE}/Stubs")

include(cmake/Sources.cmake)
include(cmake/Unported.cmake)

# -----------------------------------------------------------------------------
# Direct3D 9
#
# No DirectX SDK is required. d3d9.h, d3d9types.h and d3d9caps.h all ship in the
# Windows SDK, and Code/Stubs/d3dx supplies the handful of D3DX entry points the
# engine uses (D3DX has never been part of the Windows SDK).
#
# No import library is needed for D3D9 itself either: the engine resolves
# Direct3DCreate9 at runtime through LoadLibrary/GetProcAddress. dinput8 and
# dxguid are linked only for their link-time GUID and data-format constants.
# -----------------------------------------------------------------------------

add_library(d3dx_lite STATIC "${STUBS}/d3dx/d3dx_lite.cpp")
target_include_directories(d3dx_lite PUBLIC "${STUBS}/d3dx")
set_target_properties(d3dx_lite PROPERTIES FOLDER "ThirdParty")

add_library(directx8 INTERFACE)
target_link_libraries(directx8 INTERFACE
    d3dx_lite
    dinput8
    dxguid)

# -----------------------------------------------------------------------------
# Miles Sound System
#
# The real Miles 6 SDK is proprietary and long out of distribution. 91 files
# include mss.h, so without something answering to that name most of WWAudio,
# Combat and Commando cannot compile at all.
#
# RENEGADE_XAUDIO2_AUDIO builds the Miles surface on top of XAudio2 and
# X3DAudio, which the engine then drives without knowing the difference. Both
# are part of the Windows SDK and XAudio2_9.dll ships in Windows itself, so this
# adds no SDK, no redistributable and nothing to download -- the tree still
# builds from itself plus the Windows SDK. Turning it off falls back to the
# silent stub.
# -----------------------------------------------------------------------------

option(RENEGADE_XAUDIO2_AUDIO
    "Implement the Miles audio API on XAudio2 instead of the silent stub" ON)

# RENEGADE_MILES_RUNTIME binds the retail mss32.dll at runtime instead, the way
# Code/Stubs/bink does for binkw32.dll. Renegade shipped Miles 6 with the game,
# so owners already have a licensed copy, and it is the only backend here that
# plays the streamed MP3 music -- the XAudio2 one has no decoder for it. Still
# nothing to build against and nothing redistributed; a missing DLL degrades to
# silence at runtime rather than failing the build or the link.
option(RENEGADE_MILES_RUNTIME
    "Bind the retail mss32.dll at runtime (music included) instead of XAudio2" ON)

if(RENEGADE_MILES_RUNTIME)
    add_library(miles6 STATIC "${STUBS}/miles6/mss_runtime.cpp")
elseif(RENEGADE_XAUDIO2_AUDIO)
    add_library(miles6 STATIC "${STUBS}/miles6/mss_xaudio2.cpp")
    # Media Foundation decodes the MP3 music; shlwapi wraps the archive bytes in
    # an IStream for it. Both ship in Windows, so this stays dependency-free.
    target_link_libraries(miles6 PUBLIC
        xaudio2 mfplat mfreadwrite mfuuid shlwapi ole32)
else()
    add_library(miles6 STATIC "${STUBS}/miles6/mss_stub.cpp")
endif()

target_include_directories(miles6 PUBLIC "${STUBS}/miles6/include")
set_target_properties(miles6 PROPERTIES FOLDER "ThirdParty")

if(NOT RENEGADE_MILES_STUB)
    if(NOT EXISTS "${CODE}/Miles6/lib/win/mss32.lib")
        message(FATAL_ERROR
            "RENEGADE_MILES_STUB=OFF but the real SDK is not at "
            "${CODE}/Miles6/lib/win/mss32.lib.")
    endif()
    target_sources(miles6 PRIVATE)
    target_link_libraries(miles6 INTERFACE "${CODE}/Miles6/lib/win/mss32.lib")
endif()

# -----------------------------------------------------------------------------
# Bink Video
#
# Proprietary and undistributed, like Miles, and only Code/BinkMovie needs it --
# for eight entry points. Nothing is linked at build time: bink_loader.cpp binds
# them to binkw32.dll at runtime, which the retail game ships beside its
# executable, so owners of Renegade already have a licensed decoder. Without the
# DLL, BinkOpen fails and the movie player treats every movie as finished, so
# intros are skipped rather than hung. See Code/Stubs/bink/include/Bink.h.
# -----------------------------------------------------------------------------

add_library(bink STATIC "${STUBS}/bink/bink_loader.cpp")
target_include_directories(bink PUBLIC "${STUBS}/bink/include")
set_target_properties(bink PROPERTIES FOLDER "ThirdParty")

# -----------------------------------------------------------------------------
# GameSpy
#
# The services were shut down in 2014 and the SDK was never redistributed.
# Eleven Commando sources include one of its headers, which alone blocks the
# executable from linking. The used surface is four qr_* and five gcd_* calls,
# so a stub is cheaper than sourcing a preserved SDK; it costs the server
# browser and CD-key validation, not the game. See Code/GameSpy/.
# -----------------------------------------------------------------------------

add_library(gamespy STATIC "${STUBS}/GameSpy/gamespy_stub.cpp")
# The engine includes these as <GameSpy\gqueryreporting.h>, so the parent
# directory has to be searchable as well as the folder itself (packet.h is
# included unqualified).
target_include_directories(gamespy PUBLIC "${STUBS}" "${STUBS}/GameSpy")
set_target_properties(gamespy PROPERTIES FOLDER "ThirdParty")

# -----------------------------------------------------------------------------
# Shared build settings
# -----------------------------------------------------------------------------

add_library(renegade_common INTERFACE)

# Every engine directory is on the include path for every target.
#
# This mirrors what the .dsp files did in aggregate and is not laziness: the
# engine has circular header dependencies (wwmath includes wwsaveload's
# persistfactory.h while wwsaveload includes wwmath's), which cannot be
# expressed as an acyclic graph of per-target include scopes.
#
# It also matters for correctness, not just convenience. Several engine headers
# share a name with a Windows SDK header -- wwsaveload/persist.h vs the SDK's
# um/persist.h being the dangerous one. If the engine directory is missing from
# the search path the include silently resolves to the SDK header instead of
# failing, producing a cascade of unrelated OLE syntax errors.
target_include_directories(renegade_common INTERFACE
    # Some headers are included path-qualified from the tree root, e.g.
    # #include "WWLib\Notify.h", so Code/ itself has to be searchable.
    "${CODE}"
    "${CODE}/wwdebug"
    "${CODE}/wwlib"
    "${CODE}/WWMath"
    "${CODE}/wwsaveload"
    "${CODE}/wwutil"
    "${CODE}/wwbitpack"
    "${CODE}/wwtranslatedb"
    "${CODE}/ww3d2"
    "${CODE}/WWAudio"
    "${CODE}/wwphys"
    "${CODE}/wwnet"
    "${CODE}/wwui"
    "${CODE}/Combat"
    "${CODE}/WWOnline"
    "${CODE}/wolapi")

if(RENEGADE_PORTABLE_INV_SQRT)
    target_compile_definitions(renegade_common INTERFACE RENEGADE_PORTABLE_INV_SQRT)
endif()

target_compile_definitions(renegade_common INTERFACE
    WIN32
    _WINDOWS
    WWDEBUG
    # NB: neither WIN32_LEAN_AND_MEAN nor PARAM_EDITING_ON is global.
    # WIN32_LEAN_AND_MEAN is declared by 8 of the 14 projects; commando is not
    # one of them and needs the full windows.h (shellapi.h's FindExecutable).
    # NB: PARAM_EDITING_ON is NOT defined anywhere in this build, on purpose.
    # Every .dsp declares it only in its "DebugE"/"ProfileE" configurations --
    # the E is for Editor. The three game configurations (Release, Debug,
    # Profile) all leave it undefined, and commando.dsp has no E configuration
    # at all because the editor is a separate executable (Tools/LevelEdit).
    # It is not a debug switch: editable.h turns DECLARE_EDITABLE from nothing
    # into a ParameterListClass member plus two virtuals on every definition
    # class. Defining it for the libraries but not for commando/Scripts, which
    # include the same headers, gives one class two layouts across a single
    # link -- e.g. building.h is pulled in by 10 commando translation units.
    # It also compiles the game out of its own code: the #ifndef blocks that
    # would be lost include fall damage (humanstate.cpp) and mod script
    # unpacking from a PKG (scripts.cpp).
    # The .dsp files ask for WINVER=0x400, which the modern SDK's winnt.h rejects.
    WINVER=0x0601
    _WIN32_WINNT=0x0601
    _CRT_SECURE_NO_WARNINGS
    _WINSOCK_DEPRECATED_NO_WARNINGS
    # VC6's time_t was a 32-bit long and the code stores it in longs. Modern
    # MSVC defaults to a 64-bit time_t, which breaks every ctime/time call site.
    _USE_32BIT_TIME_T
    $<$<CONFIG:Release>:NDEBUG>)

# NB: every one of these is fenced to C/C++. renegade_common is an INTERFACE
# library, so an unfenced option is also handed to the resource compiler, and
# rc.exe fails on /EHsc long before it gets to anything interesting.
target_compile_options(renegade_common INTERFACE
    $<$<COMPILE_LANGUAGE:C,CXX>:/EHsc>
    # this code predates two-phase lookup conformance
    $<$<COMPILE_LANGUAGE:C,CXX>:/permissive>
    # VC6 leaked the for-loop variable into the enclosing scope. MSVC reports
    # this switch as deprecated (D9035) once per translation unit; the noise is
    # the price of not hand-fixing ~170 leaked loop variables.
    $<$<COMPILE_LANGUAGE:C,CXX>:/Zc:forScope->
    # VC6 treated wchar_t as unsigned short
    $<$<COMPILE_LANGUAGE:C,CXX>:/Zc:wchar_t->
    $<$<COMPILE_LANGUAGE:C,CXX>:/Zc:__cplusplus->
    # deprecated CRT names are used throughout
    $<$<COMPILE_LANGUAGE:C,CXX>:/wd4996>
    # The Profile configuration these settings reproduce always built with
    # /Zi. Except.cpp's crash handler resolves symbols through IMAGEHLP, so
    # without a PDB every report is a bare address that moves with ASLR.
    $<$<COMPILE_LANGUAGE:C,CXX>:/Zi>
    $<$<COMPILE_LANGUAGE:C,CXX>:/W0>)

# /Zi is only half of it: the linker has to emit the PDB too. Debug info goes
# into a separate file, so the shipped binaries are unchanged in behaviour.
target_link_options(renegade_common INTERFACE /DEBUG)

# -----------------------------------------------------------------------------
# Target helper
# -----------------------------------------------------------------------------

# renegade_library(<name> SOURCES <var> [DEPS ...] [INCLUDES ...])
#
# Drops any source listed in RENEGADE_UNPORTED_SOURCES when
# RENEGADE_EXCLUDE_UNPORTED is on, and reports how many were skipped.
function(renegade_library NAME)
    cmake_parse_arguments(ARG "" "SOURCES" "DEPS;INCLUDES" ${ARGN})

    set(sources ${${ARG_SOURCES}})
    set(skipped "")
    if(RENEGADE_EXCLUDE_UNPORTED)
        foreach(src IN LISTS RENEGADE_UNPORTED_SOURCES)
            if("${src}" IN_LIST sources)
                list(REMOVE_ITEM sources "${src}")
                list(APPEND skipped "${src}")
            endif()
        endforeach()
    endif()

    list(LENGTH sources kept)
    list(LENGTH skipped dropped)
    if(dropped GREATER 0)
        message(STATUS "  ${NAME}: ${kept} sources (${dropped} not yet ported)")
    else()
        message(STATUS "  ${NAME}: ${kept} sources")
    endif()

    add_library(${NAME} STATIC ${sources})
    if(RENEGADE_ENGINE_WARNINGS)
        # renegade_common contributes /W0 via its INTERFACE, which lands after
        # target options; source-level options come last and so take effect.
        set_source_files_properties(${sources} PROPERTIES COMPILE_OPTIONS "/W4")
    endif()
    target_link_libraries(${NAME} PUBLIC renegade_common ${ARG_DEPS})
    target_include_directories(${NAME} PUBLIC ${ARG_INCLUDES})
    set_target_properties(${NAME} PROPERTIES FOLDER "Engine")
endfunction()

message(STATUS "Renegade targets:")

renegade_library(wwdebug SOURCES WWDEBUG_SOURCES
    INCLUDES "${CODE}/wwdebug" "${CODE}/wwlib")
target_compile_definitions(wwdebug PRIVATE WIN32_LEAN_AND_MEAN)

renegade_library(wwlib SOURCES WWLIB_SOURCES
    DEPS wwdebug
    INCLUDES "${CODE}/wwlib")
target_compile_definitions(wwlib PRIVATE WIN32_LEAN_AND_MEAN)

renegade_library(wwmath SOURCES WWMATH_SOURCES
    DEPS wwlib wwdebug
    INCLUDES "${CODE}/WWMath")
target_compile_definitions(wwmath PRIVATE G_CODE_BASE DIRECTX)
target_compile_definitions(wwmath PRIVATE WIN32_LEAN_AND_MEAN)

renegade_library(wwsaveload SOURCES WWSAVELOAD_SOURCES
    DEPS wwlib wwdebug wwmath
    INCLUDES "${CODE}/wwsaveload")

renegade_library(wwutil SOURCES WWUTIL_SOURCES
    DEPS wwlib wwdebug wwmath
    INCLUDES "${CODE}/wwutil")
target_compile_definitions(wwutil PRIVATE WIN32_LEAN_AND_MEAN)

renegade_library(wwbitpack SOURCES WWBITPACK_SOURCES
    DEPS wwlib wwdebug wwutil
    INCLUDES "${CODE}/wwbitpack")
target_compile_definitions(wwbitpack PRIVATE WIN32_LEAN_AND_MEAN)

renegade_library(wwtranslatedb SOURCES WWTRANSLATEDB_SOURCES
    DEPS wwsaveload wwlib wwdebug wwmath
    INCLUDES "${CODE}/wwtranslatedb")

renegade_library(ww3d2 SOURCES WW3D2_SOURCES
    DEPS wwlib wwmath wwdebug wwsaveload directx8 miles6
    INCLUDES "${CODE}/ww3d2")
target_compile_definitions(ww3d2 PRIVATE VIEW_COMPRESSED)
target_compile_definitions(ww3d2 PRIVATE WIN32_LEAN_AND_MEAN)

renegade_library(wwaudio SOURCES WWAUDIO_SOURCES
    DEPS wwlib wwmath wwdebug wwsaveload ww3d2 directx8 miles6
    INCLUDES "${CODE}/WWAudio")
target_compile_definitions(wwaudio PRIVATE G_CODE_BASE)
target_compile_definitions(wwaudio PRIVATE WIN32_LEAN_AND_MEAN)

renegade_library(wwphys SOURCES WWPHYS_SOURCES
    DEPS wwsaveload wwlib wwmath ww3d2 wwdebug wwbitpack wwutil directx8
    INCLUDES "${CODE}/wwphys")

renegade_library(wwnet SOURCES WWNET_SOURCES
    DEPS wwbitpack wwdebug wwutil wwlib wwmath wwsaveload
    INCLUDES "${CODE}/wwnet" "${CODE}/wolapi" "${CODE}/WWOnline")
target_compile_definitions(wwnet PRIVATE STRICT)
target_compile_definitions(wwnet PRIVATE WIN32_LEAN_AND_MEAN)

renegade_library(wwui SOURCES WWUI_SOURCES
    DEPS wwtranslatedb wwsaveload wwaudio wwmath ww3d2 wwlib wwdebug directx8 miles6
    INCLUDES "${CODE}/wwui")

renegade_library(combat SOURCES COMBAT_SOURCES
    DEPS wwui wwutil wwbitpack wwaudio wwnet wwmath ww3d2 wwphys wwlib
         wwdebug wwsaveload wwtranslatedb directx8 miles6
    INCLUDES "${CODE}/Combat" "${CODE}/wolapi")

renegade_library(binkmovie SOURCES BINKMOVIE_SOURCES
    DEPS wwlib wwdebug wwmath ww3d2 wwsaveload directx8 bink
    INCLUDES "${CODE}/BinkMovie")

renegade_library(scontrol SOURCES SCONTROL_SOURCES
    DEPS wwlib wwdebug wwnet wwutil
    INCLUDES "${CODE}/SControl")

# BandTest is a separate DLL in the original workspace, but nothing needs it to
# be one: it exports a single function (Detect_Bandwidth, called from two places
# in bandwidthcheck.cpp) and its DllMain just returns true. Linking it in makes
# it one fewer file to ship, and leaves renegade.exe importing nothing but
# Windows system DLLs.
#
# BANDTEST_STATIC is PUBLIC so consumers get the same empty BANDTEST_API the
# library was built with -- otherwise they would still expect dllimport.
add_library(bandtest STATIC ${BANDTEST_SOURCES})
target_compile_definitions(bandtest PUBLIC BANDTEST_STATIC)
target_compile_definitions(bandtest PRIVATE WIN32_LEAN_AND_MEAN)
target_link_libraries(bandtest PRIVATE renegade_common wsock32 winmm)
target_include_directories(bandtest PUBLIC "${CODE}/BandTest")
set_target_properties(bandtest PROPERTIES FOLDER "Engine")

# -----------------------------------------------------------------------------
# Scripts.dll -- the mission logic plugin
#
# Standalone by design: it links no engine library, talking to the game only
# through the ScriptCommands function-pointer table, so it can be built and
# checked independently of commando.
# -----------------------------------------------------------------------------

option(RENEGADE_BUILD_SCRIPTS "Build the mission-script DLL" ON)

if(RENEGADE_BUILD_SCRIPTS)
    add_library(renegade_scripts SHARED ${SCRIPTS_SOURCES})
    # BUILDING_DLL flips scriptevents.h's macro from dllimport to
    # 'extern "C" __declspec(dllexport)'. Without it the entry points export
    # under mangled C++ names and the engine's GetProcAddress("Create_Script")
    # returns NULL -- the DLL loads but no script ever runs.
    target_compile_definitions(renegade_scripts PRIVATE BUILDING_DLL)
    target_link_libraries(renegade_scripts PRIVATE renegade_common)
    target_include_directories(renegade_scripts PRIVATE "${CODE}/Scripts")

    # The tree builds at /W0. Turn real diagnostics on for the mission scripts,
    # which have never been compiled by anything but VC6.
    option(RENEGADE_SCRIPT_WARNINGS "Compile the mission scripts with diagnostics on" OFF)
    if(RENEGADE_SCRIPT_WARNINGS)
        # renegade_common contributes /W0 through its INTERFACE, which lands
        # after target options and wins. Source-level options come last, so set
        # them there.
        set_source_files_properties(${SCRIPTS_SOURCES}
            PROPERTIES COMPILE_OPTIONS "/W4")
    endif()
    # ScriptManager::Init picks the DLL name from the preprocessor, and
    # Scripts.dsp names its output to match: Scripts.dll for Release,
    # ScriptsP.dll for Profile (NDEBUG + WWDEBUG), ScriptsD.dll for Debug.
    # renegade_common defines WWDEBUG in every configuration, so a Release
    # build here IS the Profile configuration and loads SCRIPTSP.DLL. Naming
    # this "Scripts" made LoadLibrary fail and silently left the game with no
    # mission scripts at all.
    set_target_properties(renegade_scripts PROPERTIES
        FOLDER "Game"
        OUTPUT_NAME "$<IF:$<CONFIG:Debug>,ScriptsD,ScriptsP>"
        EXCLUDE_FROM_ALL ON)
endif()

# -----------------------------------------------------------------------------
# Unit tests
#
# wwmath and wwlib are the right place for this engine to grow a test suite:
# both are pure, have no device or file dependencies, and everything else is
# built on them. Run with ctest, or invoke wwtest directly with a suite name.
# -----------------------------------------------------------------------------

if(RENEGADE_BUILD_TESTS)
    enable_testing()

    add_executable(wwtest
        "${CODE}/Tests/unit/wwtest.cpp"
        "${CODE}/Tests/unit/test_wwmath.cpp"
        "${CODE}/Tests/unit/test_wwlib.cpp")

    # wwmath's spline/curve classes derive from PersistClass, so the save/load
    # system comes along; winmm supplies timeGetTime for wwlib's timers.
    target_link_libraries(wwtest PRIVATE
        renegade_common wwmath wwlib wwdebug wwsaveload winmm)
    target_include_directories(wwtest PRIVATE "${CODE}/Tests/unit")
    set_target_properties(wwtest PROPERTIES FOLDER "Tests")

    # A map file makes crash offsets from the Windows error log resolvable.
    target_link_options(wwtest PRIVATE /MAP)

    add_test(NAME wwtest COMMAND wwtest)
endif()

# -----------------------------------------------------------------------------
# Commando (the game executable)
#
# commando.dsp compiles the WWOnline and wolapi sources directly rather than
# linking them as a library, so they are part of this target.
# -----------------------------------------------------------------------------

set(commando_sources ${COMMANDO_SOURCES})
set(commando_skipped "")
if(RENEGADE_EXCLUDE_UNPORTED)
    foreach(src IN LISTS RENEGADE_UNPORTED_SOURCES)
        if("${src}" IN_LIST commando_sources)
            list(REMOVE_ITEM commando_sources "${src}")
            list(APPEND commando_skipped "${src}")
        endif()
    endforeach()
endif()
list(LENGTH commando_sources commando_kept)
list(LENGTH commando_skipped commando_dropped)
message(STATUS "  commando: ${commando_kept} sources (${commando_dropped} not yet ported)")

# The menus are Win32 DIALOG templates in chat.rc, which DialogBaseClass reads
# at runtime to create each screen's controls. Without it every dialog is empty
# -- the main menu draws its backdrop and none of its buttons.
add_executable(commando WIN32 ${commando_sources} "${CODE}/Commando/chat.rc")

# chat.rc includes resource.h and dialogresource.h unqualified and pulls in
# Renegade.ico from the same folder.
target_include_directories(commando PRIVATE "${CODE}/Commando")

# The excluded translation units define symbols the rest of the game references
# -- cUserOptions::NetUpdateRate, GameInitMgrClass::End_Game,
# ScriptManager::Create_Script, SoldierGameObj::Perturb_Position and others --
# so while any of them are skipped the executable cannot link, however cleanly
# the libraries build. Keep it out of the default target rather than making
# every build end in a wall of LNK2001, but leave it buildable on request:
#
#   cmake --build build --target commando
#
if(RENEGADE_EXCLUDE_UNPORTED)
    set_target_properties(commando PROPERTIES EXCLUDE_FROM_ALL ON)
endif()

target_compile_definitions(commando PRIVATE OBSIDIAN DIRECTX)

# WINMAIN.CPP wraps Game_Main_Loop in an SEH __try inside a function that also
# holds objects with destructors, which MSVC rejects with C2712 whenever C++
# exception handling is enabled -- /EHa does NOT lift this, only disabling EH
# does. That matches the original intent: the comment above the __try says
# "the compilers exception handling options should be disabled". /EHs-c-
# overrides CMake's default /EHsc for this one translation unit.
set_source_files_properties("${CODE}/Commando/WINMAIN.CPP"
    PROPERTIES COMPILE_OPTIONS "/EHs-c-")

target_include_directories(commando PRIVATE
    "${CODE}/Commando"
    "${CODE}/WWOnline"
    "${CODE}/wolapi"
    "${CODE}/BinkMovie"
    "${CODE}/SControl"
    "${CODE}/BandTest"
    "${STUBS}"
    "${STUBS}/GameSpy")

target_link_libraries(commando PRIVATE
    renegade_common
    combat wwui wwaudio wwsaveload ww3d2 wwdebug wwlib wwmath
    wwnet wwphys wwutil wwtranslatedb wwbitpack
    binkmovie scontrol bandtest
    directx8 miles6 bink gamespy
    user32 gdi32 winspool comdlg32 advapi32 ole32 oleaut32 uuid
    winmm vfw32 wsock32 kernel32 shell32 version)

# The DirectX 8 import libraries were built with VC6 and carry embedded
# default-lib directives for CRTs that no longer ship. Ignore them; the modern
# CRT selected by CMAKE_MSVC_RUNTIME_LIBRARY supplies the same symbols.
target_link_options(commando PRIVATE
    /NODEFAULTLIB:libci.lib
    /NODEFAULTLIB:libc.lib
    /NODEFAULTLIB:libcd.lib
    /NODEFAULTLIB:libcmtd.lib
    /NODEFAULTLIB:msvcrt.lib
    /NODEFAULTLIB:msvcrtd.lib
    # Except.cpp reports a bare EIP whenever IMAGEHLP cannot resolve a
    # symbol. A link map plus a fixed image base makes that address
    # mean the same thing on every run: RVA = EIP - 0x400000.
    /MAP
    /DYNAMICBASE:NO)

set_target_properties(commando PROPERTIES FOLDER "Game" OUTPUT_NAME "renegade")

# -----------------------------------------------------------------------------

message(STATUS "")
message(STATUS "Renegade configured:")
message(STATUS "  architecture     x86 (32-bit)")
message(STATUS "  graphics         Direct3D 9 (Windows SDK headers, no DirectX SDK)")
if(NOT RENEGADE_MILES_STUB)
    message(STATUS "  audio            real Miles SDK")
elseif(RENEGADE_MILES_RUNTIME)
    message(STATUS "  audio            retail mss32.dll at runtime (music included)")
elseif(RENEGADE_XAUDIO2_AUDIO)
    message(STATUS "  audio            XAudio2 + Media Foundation (music included)")
else()
    message(STATUS "  audio            silent stub")
endif()
list(LENGTH RENEGADE_UNPORTED_SOURCES unported_total)
if(RENEGADE_EXCLUDE_UNPORTED)
    message(STATUS "  unported files   ${unported_total} excluded "
                   "(-DRENEGADE_EXCLUDE_UNPORTED=OFF to attempt them)")
endif()
message(STATUS "")
