# Windows packaging

The first distribution uses the current Schematic name and version 0.1.0. CMake owns the version; main.cpp exposes it through QApplication. CPack produces an NSIS installer and a ZIP from the same normal install rules and Qt deployment script. Native macOS/Linux installer formats are deferred.

## Build

Requirements: the normal C++/Qt build tools, CPack (included with CMake), and NSIS 3.03 or later. The installer generator locates NSIS from its standard installation. Use the git-ignored local-msvc preset for machine-specific Qt/compiler paths.

From the repository root:

~~~powershell
cmake --preset local-msvc -B build-package -DBUILD_TESTING=OFF
cmake --build build-package --config Release --target Schematic
cpack --config build-package/CPackConfig.cmake -C Release
~~~

VS Code also provides **Package Windows Release**, which runs these steps in order. Packaging uses an isolated build so an application running from the normal development build does not prevent linking.

Artifacts go in build-package/packages: Schematic-0.1.0-Windows-x64.exe and Schematic-0.1.0-Windows-x64.zip for the current MSVC x64 kit. For one format, add -G NSIS or -G ZIP to the cpack command. CPack stages its own clean install tree; do not package the development Release directory, which can contain tests and stale dependencies.

## Contents and behavior

- The executable is in bin, accompanied by Qt DLLs, runtime support and qt.conf; Qt plugins and translations follow Qt's deployment layout. Built-in symbols are already embedded application resources.
- The installer uses the standard CPack NSIS workflow: administrator installation, Start-menu shortcut, installed-app entry and uninstaller. A later release uses the same install-directory and registry identity and offers to uninstall the previous version first. No PATH change is requested.
- Extract the entire ZIP before running bin/Schematic.exe. This is an installation-free distribution, not a separate portable settings profile: preferences and user symbols still use the normal user locations.
- No file associations, automatic updates, custom icon, signing or publishing are added in this first pass. File association support also needs application-side command-line filename handling.
- Uninstall removes installed program files, leaving drawings and per-user settings/symbols alone. No custom user-data cleanup is performed.
- Include the application license, Qt module license texts and available installed Qt SPDX dependency metadata. Qt source license locations are detected from the selected kit; SCHEMATIC_QT_SOURCES_DIR overrides the source directory. Missing texts produce a configure warning and must be resolved before distributing.
- Fonts are selected from the user's installed fonts and are not bundled. SVG references font names.

## Validation boundary

Build both packages, inspect archive contents and dependency deployment, and smoke-test the extracted application with Qt development paths removed. Installer generation is not a substitute for testing install, upgrade and uninstall on a clean Windows VM. That VM test and public-release signing remain separate release checks.

## Verification record — 14 November 2026

The isolated MSVC x64 Release application build passed. ZIP generation passed; the 26,095,319-byte archive was extracted and checked for the application, Qt libraries/platform plugin, qt.conf, app-local Visual C++ runtime DLLs and license/SPDX files, with no test binaries, PDBs, objects or static libraries. The extracted application stayed running during a local startup smoke test with PATH restricted to Windows system directories and Qt plugin environment overrides removed. This is a local smoke test, not clean-VM verification.

NSIS 3.13 was found in its standard installation, but both its launcher and Bin/makensis.exe failed to start from the agent environment with Windows error “Illegal System DLL Relocation”. CPack could not complete its NSIS version check, so no installer executable was generated or installed. Run Package Windows Release from the normal VS Code terminal to try installer generation outside this environment. If the same error occurs there, first check that makensis /VERSION runs successfully in that terminal. The ZIP can also be built independently with -G ZIP.
