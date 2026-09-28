# Name to be confirmed
This is a QT program to enable nice electronic schematics to be edited with the focus entirely on presentation rather than EDA

## Build Instructions

### Environment

The project uses Qt6 and CMake, and can easily be built with Qt Creator or VS Code. You will need to install Qt with a Qt6 kit, and if you want to use the MSVC compilers then you will need to install Visual Studio and the Windows SDK (for the debugger).

### Building from the command line

```
cmake --preset default
cmake --build --preset default
```

A `mingw` preset is also provided for building with MinGW instead of MSVC.

