# Local development notes

`AGENTS.md` is the tool-neutral product and collaboration contract. `DECISIONS.md` records settled design choices, and `ROADMAP.md` tracks known defects and future work. These repository files are the source of truth; keep them aligned with the code and do not copy their project status into external project notes.

## Environment on Oliver's Windows machine

- Qt 6.11.1 is installed at `D:/Qt/6.11.1`, with `msvc2022_64` and `mingw_64` kits.
- The `default` and `mingw` CMake presets currently point at those local Qt kits. The default preset uses Visual Studio 18 (2026).
- CMake 4.3.1 is available. Builds use the presets; `qmake`, `ninja`, and `vcpkg` are not on PATH.
- To build and run the tests from a Git Bash-style shell:

  ```sh
  cmake --preset default
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
