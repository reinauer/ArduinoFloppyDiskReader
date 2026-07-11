# DrawBridge Qt

This is the Qt 6 desktop frontend for DrawBridge. It provides the same disk
reading, writing, diagnostics, cleaning, and EEPROM configuration workflows as
the Windows MFC application on Linux and macOS.

## Requirements

- A C++17 compiler
- CMake 3.21 or newer
- Qt 6.5 or newer with the Core, Gui, and Widgets modules
- On Linux, permission to access the selected serial device (commonly through
  membership in the `dialout` group)
- The optional CAPSImg library at runtime for IPF writing

## Build

From the `ArduinoFloppyReader` directory:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The executable is created below `build/ArduinoFloppyReaderQt`. On macOS it is
created as `DrawBridge.app`.

The application uses the existing platform-specific `SerialIO` implementation
rather than Qt Serial Port because DrawBridge needs custom two-megabaud and
flow-control handling.
