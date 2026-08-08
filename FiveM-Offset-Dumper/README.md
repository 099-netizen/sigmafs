# 999 SERVICES — FiveM Advanced Offset Dumper v2

A Windows-native FiveM memory pattern scanner built with **C++17**, **Dear ImGui**, and **DirectX 11**.
UI is styled to match the dark / purple hacker-tool aesthetic from the reference screenshot.

![UI preview](preview.png)

## Features
- Sidebar navigation (Scanner / Process Info / Settings / About)
- One-click **Start Dump** button
- Color-coded live console log `[INFO]` / `[ OK ]` / `[FAIL]`
- Automatic FiveM process detection + build number parsing
- Full-module memory caching (one `ReadProcessMemory` pass — fast re-scans)
- Byte-pattern scanner with `?` wildcards, RIP-relative resolution, pointer-deref chaining
- Automatic export of results to `offsets_<build>.hpp`
- Custom purple dark theme

## Requirements
- Windows 10/11 x64
- Visual Studio 2022 (Desktop C++ workload) OR CMake + MSVC
- FiveM running (for live scans)

## Build

### Option A — Visual Studio (recommended)
```bat
1. Double-click setup.bat          # downloads Dear ImGui into lib/
2. Open FiveM-Offset-Dumper.sln    # (see .sln file in vs/ folder)
3. Select Release | x64
4. Build (Ctrl+Shift+B)
5. Run the produced .exe as Administrator
```

### Option B — CMake from a Developer Command Prompt
```bat
setup.bat
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
.\bin\Release\999_Offset_Dumper.exe
```

## Project layout
```
FiveM-Offset-Dumper/
├─ src/
│  ├─ main.cpp        # Win32 + DX11 bootstrap + ImGui UI
│  ├─ scanner.h       # Process helper + pattern scan engine
│  └─ patterns.h      # Default signature database (extend me)
├─ lib/imgui/         # Dear ImGui sources (populated by setup.bat)
├─ CMakeLists.txt
└─ setup.bat
```

## Adding / updating signatures
Edit `src/patterns.h`. Each entry:
```cpp
{ "m_World",  "48 8B 05 ? ? ? ? 45 33 C0",  3 /*offset*/, 1 /*derefs*/, true /*rip-relative*/ }
```
- `signature` — IDA-style string with `??` / `?` wildcards
- `offset`    — bytes from the start of the match to read the displacement / pointer
- `extra`     — number of pointer dereferences to follow after resolution
- `rip`       — `true` if the match is a RIP-relative LEA/CALL/MOV instruction

## Legal
For **educational & reverse-engineering research purposes only**.
Use on software you own / are authorised to analyse.
