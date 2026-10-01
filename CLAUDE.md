# Local development notes

`AGENTS.md` is the tool-neutral product and collaboration contract. `DECISIONS.md` records settled design choices, and `ROADMAP.md` tracks known defects and future work. These repository files are the source of truth; keep them aligned with the code and do not copy their project status into external project notes.

## Environment on Oliver's Windows machine

- Qt 6.11.1 is installed at `D:/Qt/6.11.1`, with `msvc2022_64` and `mingw_64` kits.
- The checked-in `default` and `mingw` CMake presets are portable and do not name a Qt kit. This machine's Qt paths live in the git-ignored `CMakeUserPresets.json` (`local-msvc` inherits `default`; `local-mingw` inherits `mingw`). Recreate it from the README if it is missing. The default preset uses Visual Studio 18 (2026).
- The VS Code launch configurations and the "Deploy Qt" tasks read the Qt `bin` directory from the `QT_BIN_DIR` environment variable (here `D:/Qt/6.11.1/msvc2022_64/bin`). Set it in the user environment before using them.
- CMake 4.3.1 is available. Builds use the presets; `qmake`, `ninja`, and `vcpkg` are not on PATH.
- To build and run the tests from a Git Bash-style shell:

  ```sh
  cmake --preset local-msvc
  cmake --build build --config Debug
  PATH="/d/Qt/6.11.1/msvc2022_64/bin:$PATH" ctest --test-dir build -C Debug --output-on-failure
  ```

  Add the matching Qt `bin` directory to `PATH` when launching the executable or test binary directly so Qt DLLs can be found.

## Repository conventions

- New UI is hand-written C++; the project has no `.ui` files.
- List headers and sources explicitly in CMake targets.
- The `schematiccore` library depends on Qt Core only. The model tests use Qt Test and CTest without Qt Gui or Widgets.
- `BUILD_TESTING` defaults to `ON`. Use `-DBUILD_TESTING=OFF` for an app-only configuration that does not require Qt Test.
- Focused tests live in `tests/tst_<module>.h/.cpp`; test classes are registered in `tests/main.cpp`. Keep expected values independent of the implementation path under test.
- Run the smallest checks that give confidence in the change. Do not repeat a successful build or test run unless the code changed afterward, the result was inconclusive, or a later change could affect it. For documentation-only changes, check the Markdown diff and links; do not build or run tests.
- Before reporting verification, run relevant checks in the current environment and report exactly what ran. Prior-session build or test results are historical, not evidence for a later change.

## Working efficiently (cost)

Sonnet runs here on pay-as-you-go, so avoid avoidable token use:

- Work on one approved ROADMAP chunk per task. Do not start the next chunk without approval.
- Read the files named in the chunk brief and what they directly depend on. Do not scan the whole repo, and skip LICENSE, build directories and generated files.
- Don't re-read a file already in context unless it has changed.
- Prefer targeted edits to rewriting whole files.
- If the brief is ambiguous or the change would exceed its scope, stop and ask rather than exploring.
- Keep the final report short: what changed, what was run, what was not.

These rules never override the testing and reporting requirements above: run the checks that are relevant to the change, and report exactly what ran.