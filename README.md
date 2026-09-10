# Command & Conquer Renegade

[![build](https://github.com/BirgirR/CnC_Renegade/actions/workflows/build.yml/badge.svg)](https://github.com/BirgirR/CnC_Renegade/actions/workflows/build.yml)
[![license: GPL-3.0 with additional terms](https://img.shields.io/badge/license-GPL--3.0%20with%20additional%20terms-blue)](LICENSE.md)

This is a fork of the [Command & Conquer Renegade source release](https://github.com/electronicarts/CnC_Renegade)
published by Electronic Arts. The upstream tree targets Visual C++ 6.0 and a set
of proprietary SDKs that were never distributed with it, so it does not build as
shipped.

This fork builds. The game compiles with a current MSVC toolchain through CMake,
using nothing beyond Visual Studio and the Windows SDK — no DirectX SDK, no Bink,
no Miles, no GameSpy, nothing to download. The result is `renegade.exe` plus
`ScriptsP.dll`, which run against a retail game install.

To play the result you must own the game. The C&C Ultimate Collection is
available on [EA App](https://www.ea.com/en-gb/games/command-and-conquer/command-and-conquer-the-ultimate-collection/buy/pc)
and [Steam](https://store.steampowered.com/bundle/39394/Command__Conquer_The_Ultimate_Collection/).


## Requirements

- Windows
- Visual Studio 2019 or newer, with the **x86** C++ toolset and the Windows SDK
- CMake 3.20 or newer

32-bit only. Thirty-five files contain x86 inline assembly, so there is no x64
configuration short of a substantial further port.


## Building

With a CMake new enough to know your Visual Studio version:

```
cmake -B build -A Win32 .
cmake --build build --config Release --target commando renegade_scripts
```

Otherwise, from an x86 developer command prompt:

```
vcvarsall.bat x86
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release .
cmake --build build --target commando renegade_scripts
```

Binaries land in `Run/` at the root of the repository.

`commando` and `renegade_scripts` are named explicitly because both are excluded
from the default target: a bare `cmake --build build` builds the engine libraries
and the unit tests, which is the fast loop for working on the engine.

### Running

`Run/` needs the game's data alongside the binaries. Point it at a retail install
— copy the install's `Data/` folder (and its fonts) into `Run/`, or copy the built
`renegade.exe` and `ScriptsP.dll` into the install directory.

The Release configuration produces `ScriptsP.dll`, not `Scripts.dll`. That is
deliberate: `WWDEBUG` is defined in every configuration here, which makes a
Release build the original workspace's *Profile* configuration, and
`ScriptManager::Init` picks the DLL name to match.

### Build options

| Option | Default | Effect |
| --- | --- | --- |
| `RENEGADE_XAUDIO2_AUDIO` | `ON` | Implement the Miles audio API on XAudio2. `OFF` gives the silent stub. |
| `RENEGADE_MILES_STUB` | `ON` | `OFF` links the real Miles 6 SDK from `Code/Miles6/`, if you have it. |
| `RENEGADE_BUILD_SCRIPTS` | `ON` | Build the mission-script DLL. |
| `RENEGADE_BUILD_TESTS` | `ON` | Build the `wwmath`/`wwlib` unit tests. |
| `RENEGADE_EXCLUDE_UNPORTED` | `ON` | Skip the translation units listed in `cmake/Unported.cmake`. |
| `RENEGADE_ENGINE_WARNINGS` | `OFF` | Compile the engine at `/W4`. The tree builds at `/W0`. |
| `RENEGADE_SCRIPT_WARNINGS` | `OFF` | Compile the mission scripts at `/W4`. |
| `RENEGADE_PORTABLE_INV_SQRT` | `OFF` | Substitute plain C for `WWMath::Inv_Sqrt`, a `__declspec(naked)` x87 routine, when debugging. |

### Tests

`Code/Tests/unit` holds a small suite over `wwmath` and `wwlib` — the two
libraries everything else is built on, and the only ones with no device or file
dependencies.

```
ctest --test-dir build            # add -C Release with the Visual Studio generator
```

Or run `Run/wwtest.exe` directly, optionally with a suite name (`wwtest.exe Vector3`).


## How the missing SDKs are handled

The upstream release lists a dozen third-party libraries it cannot ship. For the
game and its engine, each is now either unnecessary or answered by a small
implementation under `Code/Stubs/`:

- **DirectX 8** — the renderer was moved to **Direct3D 9**, whose headers ship in
  the Windows SDK. `Direct3DCreate9` is resolved at runtime, so no import library
  is needed either. `Code/Stubs/d3dx` supplies the handful of D3DX entry points
  the engine uses, since D3DX has never been part of the Windows SDK.
- **Miles Sound System** — 91 files include `mss.h`, so something has to answer
  to it. `Code/Stubs/miles6` implements the API the engine actually uses on top
  of **XAudio2** and X3DAudio, both part of the Windows SDK, with
  `XAudio2_9.dll` shipping in Windows itself. The engine drives it without
  knowing the difference. A silent stub is available as a fallback.
- **Bink** — stubbed. `BinkOpen` fails, which the movie player already treats as
  "movie finished", so intro videos are skipped rather than hanging.
- **GameSpy** — the services shut down in 2014. The used surface is four `qr_*`
  and five `gcd_*` calls, so it is stubbed; the cost is the server browser and
  CD-key validation, not the game.
- **GNU Regex** — absent from the release. `regexpr.cpp` is self-contained and
  nothing in the tree references it, so it is the one file left in
  `cmake/Unported.cmake`.
- **NvDXTLib, Lightscape, Umbra, SafeDisk, Cab, RTPatch, Java headers** — used
  only by the launcher, installer and the asset tools, none of which this build
  covers.

Disc-check code (`Code/Commando/cdverify.*`) was removed rather than stubbed.


## Relationship to the original workspace

The `.dsp` project files remain the source of truth for which translation units
belong to which target. `cmake/gen_sources.cmake` reads them to produce
`cmake/Sources.cmake`:

```
cmake -P cmake/gen_sources.cmake
```

Run it after adding or removing a file, rather than switching CMake to globbing.
It is a script-mode tool, not part of the configure step — `Sources.cmake` is
committed, so an ordinary build never runs it, and it needs no interpreter beyond
the CMake a build already requires.

MSBuild cannot read `.dsp`/`.dsw` at all, so `CMakeLists.txt` replaces the
workspace instead of converting it. The compiler settings it applies —
`/Zc:forScope-`, `/Zc:wchar_t-`, `_USE_32BIT_TIME_T`, `/permissive` — exist to
reproduce VC6 semantics the code depends on; each is commented in place.

The original VC6 route still works for what CMake does not cover:

- **Free Dedicated Server** — uncomment `#define FREEDEDICATEDSERVER` in
  [Combat\specialbuilds.h](Code/Combat/specialbuilds.h) and rebuild the Release
  configuration.
- **Level Edit** — add `PUBLIC_EDITOR_VER` to the LevelEdit project's
  preprocessor defines to build the public release configuration.


## Known issues

The Debug configuration of the game executable will sometimes fail to link,
because Windows Defender incorrectly flags the output as containing a virus
(likely the embedded browser code). Excluding `Run/` in Windows Defender resolves
it.


## Contributing

Contributions are welcome — the upstream EA repository does not accept them, but
this fork does. Issues and pull requests both work.

A few conventions specific to this tree:

**Source lists come from the `.dsp` files.** To add or remove a translation unit,
edit the relevant `Code/*/*.dsp`, run `cmake -P cmake/gen_sources.cmake`, and commit
both it and the regenerated `cmake/Sources.cmake`. Please do not switch CMake to
globbing — the `.dsp` files stay the source of truth so the original workspace
and this one cannot silently disagree.

**Leave upstream code looking like upstream code.** This is a 2002 EA codebase:
tabs, `Upper_Snake_Case` functions, `/* ** */` comment banners. Match whatever
surrounds your change and keep diffs to the lines you actually touched — there is
no `.clang-format` here on purpose, and a reformatting pass buries real changes.

**Comment the whys, not the whats.** The compiler flags in `CMakeLists.txt`
(`/Zc:forScope-`, `/Zc:wchar_t-`, `_USE_32BIT_TIME_T`, `/permissive`) all
reproduce VC6 semantics the code depends on, and each says so in place. Anything
new that exists to work around the age of this code deserves the same treatment.

**Check your changes with warnings on.** The tree builds at `/W0`, which hides a
great deal. Configure with `-DRENEGADE_ENGINE_WARNINGS=ON` (or
`-DRENEGADE_SCRIPT_WARNINGS=ON`) while you work, even though the default stays
off.

**Test what can be tested.** Changes to `wwmath` or `wwlib` should come with
cases in `Code/Tests/unit` — `WWTEST(Suite, Name)` plus `CHECK`, `CHECK_EQ`,
`CHECK_STR_EQ` and `CHECK_NEAR`. Run `ctest --test-dir build` before submitting.
Most of the engine cannot be unit tested without a device or the game's data, so
for anything beyond those two libraries, say in the pull request how you exercised
it in the running game.

**Keep the build self-contained.** The point of `Code/Stubs/` is that the tree
builds from itself plus the Windows SDK. New third-party dependencies that mean a
download, an SDK install or a redistributable defeat that; a stub or a
Windows-SDK-based implementation is strongly preferred.

**It is 32-bit.** Inline assembly and pointer-size assumptions are pervasive, so
new code should not assume otherwise. An actual x64 port would be very welcome,
but as a deliberate piece of work rather than incidentally.

## License

This repository and its contents are licensed under the GPL v3 license, with
additional terms applied. Please see [LICENSE.md](LICENSE.md) for details.
