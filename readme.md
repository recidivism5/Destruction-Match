# Burger Land

### Build:
- Have CMake installed and the Microsoft C/C++ extension pack on VS Code
- Open this folder with VS code
- Configure with CMake
- Debug/Build from the CMake menu

### VSCode tips:
- Hit Ctrl+F5 to automatically launch debug
- Hit Ctrl+Shift+P and type CMake to see all the CMake commands
- Add the following to your .vscode/settings.json to fix terminal output when debugging (otherwise it seems to just go nowhere):
```json
"cmake.debugConfig": {
    "console": "integratedTerminal",
},
```