# SDLInput — Implementation Plan

## Goal

Defold native extension that wraps SDL3's gamepad/joystick/HID subsystem for Lua.
Provides raw HID access (gyro calibration, feature reports) and full gamepad
abstraction (all controller types, mapping DB). Works on desktop and WASM.

## Approach: Prebuilt SDL3 static library

Instead of vendoring SDL3 source (requires patching internal headers, managing
C/C++ TU conflicts, tracking moving internal deps), we build SDL3 as a static
library per platform using its own cmake build system. SDL3 handles platform
detection, optional deps (udev via dlopen), and subsystem selection correctly
— we don't fight it.

The extension ships `libSDL3.a` in `lib/<platform>/` and provides a thin
Lua binding layer in `src/`. The extender auto-links the static libs.

### Platform build order

1. **x86_64-linux** — build natively on this machine (first step)
2. x86_64-win32, x86_64-osx, arm64-osx, wasm-web — cross-compilation later

## Directory Structure

```
ext-sdlinput/
├── ext.manifest              # platform libs + frameworks
├── api/
│   └── sdlinput.script_api   # Lua API def
├── include/
│   └── SDL3/                 # SDL3 public headers (from SDL3 source tree)
│       ├── SDL_gamepad.h     # gamepad API
│       ├── SDL_joystick.h    # joystick API
│       ├── SDL_hidapi.h      # HID API
│       ├── SDL_stdinc.h      # basic types
│       ├── SDL_error.h       # error reporting
│       ├── SDL_properties.h  # property system
│       ├── SDL_platform.h    # platform detection
│       ├── SDL_begin_code.h  # API export macros
│       ├── SDL_close_code.h  # API export macros
│       ├── SDL_platform_defines.h
│       └── ...               # minimal transitive includes
├── src/
│   ├── extension.cpp         # DM_DECLARE_EXTENSION entry
│   └── sdlinput_bridge.cpp   # Lua bindings → SDL3 API
├── lib/
│   ├── x86_64-linux/
│   │   └── libSDL3.a         # prebuilt (step 1)
│   ├── x86_64-win32/         # (future)
│   ├── x86_64-osx/           # (future)
│   ├── arm64-osx/            # (future)
│   ├── js-web/               # (future, likely deprecated)
│   └── wasm-web/             # (future)
└── scripts/
    └── build-sdl3.sh         # build + copy for each platform
```

## ext.manifest

```yaml
name: "SDLInput"
platforms:
  x86_64-linux:
    context:
      libs: ["SDL3"]
  x86_64-win32:
    context:
      libs: ["SDL3"]
  x86_64-osx:
    context:
      frameworks: ["IOKit", "CoreFoundation", "GameController", "ForceFeedback"]
      libs: ["SDL3"]
  arm64-osx:
    context:
      frameworks: ["IOKit", "CoreFoundation", "GameController", "ForceFeedback"]
      libs: ["SDL3"]
  wasm-web:
    context:
      libs: ["SDL3"]
```

Note: `libs: ["SDL3"]` refers to `libSDL3.a` in `lib/<platform>/`. The extender
auto-discovers these files — no special path config needed.

## Building SDL3 (step 1: Linux only)

### Prerequisites

```bash
sudo apt install cmake build-essential libudev-dev libdbus-1-dev
```

`libudev-dev` is needed at compile time for SDL3's Linux joystick backend.
SDL3 dlopen's libudev at runtime — if absent, it falls back to polling.

`libusb-1.0-0-dev` is optional. If present, HIDAPI uses libusb as a transport
fallback (needed for some Bluetooth controllers on older kernels). If absent,
HIDAPI uses hidraw exclusively, which works for most controllers.

### CMake configuration

```bash
git clone --depth 1 --branch release-3.2.x https://github.com/libsdl-org/SDL /tmp/SDL
cd /tmp/SDL
cmake -B build \
  -DBUILD_SHARED_LIBS=OFF \
  -DSDL_STATIC=ON \
  -DSDL_STATIC_PIC=ON \        # required: .a linked into .so
  -DSDL_UNIX_CONSOLE_BUILD=ON \ # required: no X11/Wayland video
  \
  # Subsystems to enable
  -DSDL_JOYSTICK=ON \
  -DSDL_HIDAPI=ON \
  -DSDL_SENSOR=ON \
  -DSDL_HAPTIC=ON \
  -DSDL_POWER=ON \
  \
  # Subsystems to disable
  -DSDL_VIDEO=OFF \
  -DSDL_AUDIO=OFF \
  -DSDL_RENDER=OFF \
  -DSDL_GPU=OFF \
  -DSDL_CAMERA=OFF \
  -DSDL_DIALOG=OFF \
  -DSDL_TRAY=OFF \
  -DSDL_PROCESS=OFF \
  -DSDL_STORAGE=OFF \
  \
  # Video platform backends (all off)
  -DSDL_X11=OFF \
  -DSDL_WAYLAND=OFF \
  -DSDL_OPENGL=OFF \
  -DSDL_VULKAN=OFF \
  -DSDL_METAL=OFF \
  -DSDL_COCOA=OFF \
  -DSDL_DIRECTX=OFF \
  \
  # Audio backends (all off)
  -DSDL_ALSA=OFF \
  -DSDL_PULSEAUDIO=OFF \
  -DSDL_PIPEWIRE=OFF \
  -DSDL_JACK=OFF \
  -DSDL_SNDIO=OFF \
  -DSDL_DISKAUDIO=OFF \
  -DSDL_DUMMYAUDIO=OFF \
  \
  # Other features to disable
  -DSDL_IBUS=OFF \
  -DSDL_FCITX=OFF \
  -DSDL_LIBURING=OFF \
  -DSDL_VIRTUAL_JOYSTICK=OFF
```

Build:
```bash
cmake --build build -j$(nproc)
```

Copy result:
```bash
cp build/libSDL3.a ext-sdlinput/lib/x86_64-linux/
```

### Library size

`libSDL3.a` will be ~18MB. SDL3's cmake compiles ALL source files regardless
of subsystem flags — disabled subsystems get stub implementations, not omitted
code. However, the linker only extracts object files referenced by undefined
symbols. The final extension `.so` should be ~1-3MB.

**Note on `SDL_HasWindows` linkage:** `src/joystick/SDL_joystick.c` calls
`SDL_HasWindows()` (defined in `src/video/SDL_video.c`). Even with video
disabled, this symbol exists in the video stub. It pulls in `SDL_video.c.o`.
At runtime the stub returns `false`, so no code path issues — just a small
size overhead. If this becomes a problem, we can upstream a `#ifndef
SDL_VIDEO_DISABLED` guard around the call.

### What SDL3 subsystems we get

| Subsystem | Role |
|-----------|------|
| Joystick | Gamepad/joystick enumeration and state |
| HIDAPI | Controller-specific drivers (DualSense, Switch Pro, Xbox, PS4/5, etc.) |
| Sensor | IMU/gyro data from supported controllers |
| Haptic | Rumble support |
| Power | Battery status |
| Events (internal) | Input event processing — must pump explicitly |
| Timer (internal) | Timing for polling |
| Thread (internal) | Threading for async I/O |
| File I/O (internal) | Config file loading (e.g., gamecontrollerdb.txt) |

What we DON'T include: Video, Audio, Render, GPU, Camera, Dialog, Tray, Storage, Process.

### Important: Event Pump Required

SDL3 gamepad state updates via `SDL_PumpEvents()`. Without it,
`SDL_GetGamepadButton()`, `SDL_GetGamepadAxis()`, and
`SDL_GetGamepadSensorData()` return stale data.

Defold uses SDL2 internally — its event loop does NOT pump SDL3's event queue.
The extension must call `SDL_PumpEvents()` explicitly at the top of every
gamepad state read in the Lua bridge.

### udev handling

SDL3's Linux joystick backend uses udev via dlopen (`libudev.so.1`). At
runtime:
- `libudev.so.1` available → udev-based device enumeration + hotplug
- Not available → falls back to polling `/dev/input/` every 3 seconds

Works on systems without udev, just with polling-based device detection.

### glibc ABI compatibility

`libSDL3.a` must be built on a system with glibc <= the extender's glibc.
If built on a newer glibc, symbol versioning may cause load-time failures on
the extender. Build on Ubuntu 20.04 (extender's likely base) or containerize
the build.

## Lua API Surface

```lua
-- Module: sdlinput

-- Init / quit
sdlinput.init()                           → ok|nil, err
sdlinput.quit()

-- Must call each frame before reading state
sdlinput.pump_events()

-- Gamepad enumeration
sdlinput.num_gamepads()                   → count|nil, err
sdlinput.gamepad_name(index)              → string|nil, err
sdlinput.gamepad_open(index)              → handle|nil, err
sdlinput.gamepad_close(handle)
sdlinput.gamepad_connected(handle)        → bool
sdlinput.gamepad_get_type(handle)         → string  -- e.g. "ps5", "nintendo_switch_pro", "xbox_one"
sdlinput.gamepad_get_vid(handle)          → number
sdlinput.gamepad_get_pid(handle)          → number

-- Gamepad state (call pump_events() first)
sdlinput.gamepad_get_buttons(handle)      → {a, b, x, y, dpad_up, dpad_down, dpad_left, dpad_right,
                                            left_shoulder, right_shoulder, left_stick, right_stick,
                                            start, back, guide, misc1, paddle1, paddle2, paddle3, paddle4,
                                            touchpad}
sdlinput.gamepad_get_axis(handle, axis)   → float  -- -1..1, axis: "leftx","lefty","rightx","righty",
                                                   --        "left_trigger","right_trigger"
sdlinput.gamepad_get_sensor(handle, type) → {x, y, z}  -- type: "gyro" or "accel"
sdlinput.gamepad_rumble(handle, left_motor, right_motor, duration_ms)  → ok|nil, err
sdlinput.gamepad_has_led(handle)          → bool
sdlinput.gamepad_set_led(handle, r, g, b) → ok|nil, err

-- HID (raw report access, needed for custom gyro calibration on unsupported controllers)
sdlinput.hid_enumerate(vid, pid)          → table|nil, err  -- vid/pid 0 = wildcard
sdlinput.hid_open(vid, pid)               → handle|nil, err
sdlinput.hid_open_path(path)              → handle|nil, err
sdlinput.hid_close(handle)
sdlinput.hid_read(handle, len, timeout_ms) → string|nil, err
  -- timeout_ms: -1 = block indefinitely, 0 = non-blocking, >0 = ms timeout
sdlinput.hid_write(handle, data_string)   → count|nil, err
sdlinput.hid_get_feature_report(handle, report_id, len) → string|nil, err
sdlinput.hid_send_feature_report(handle, data_string) → count|nil, err
sdlinput.hid_error(handle)                → string
```

**Error convention:** All functions that can fail return `nil, error_string`
on failure. Functions with no failure mode return `ok` (true) on success.

### Thread safety note

`hid_read`, `hid_write`, and `hid_get_feature_report` are blocking calls.
Calling them from Lua's main update loop will freeze the frame. For v1, keep
them synchronous — document the limitation. Future: defer blocking HID I/O to
a worker thread via `dmThread::Spawn()`.

### Gamepad type strings

Mapped from SDL3 enums:
| SDL_GamepadType | Lua string |
|----------------|------------|
| SDL_GAMEPAD_TYPE_UNKNOWN | `"unknown"` |
| SDL_GAMEPAD_TYPE_STANDARD | `"standard"` |
| SDL_GAMEPAD_TYPE_XBOX360 | `"xbox_360"` |
| SDL_GAMEPAD_TYPE_XBOXONE | `"xbox_one"` |
| SDL_GAMEPAD_TYPE_PS3 | `"ps3"` |
| SDL_GAMEPAD_TYPE_PS4 | `"ps4"` |
| SDL_GAMEPAD_TYPE_PS5 | `"ps5"` |
| SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO | `"nintendo_switch_pro"` |
| SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT | `"nintendo_switch_joycon_left"` |
| SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT | `"nintendo_switch_joycon_right"` |
| SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR | `"nintendo_switch_joycon_pair"` |
| SDL_GAMEPAD_TYPE_SHIELD | `"shield"` |
| SDL_GAMEPAD_TYPE_STADIA | `"stadia"` |
| SDL_GAMEPAD_TYPE_AMAZON_LUNA | `"amazon_luna"` |
| SDL_GAMEPAD_TYPE_GOOGLE_TEAC | `"google_teac"` |
| SDL_GAMEPAD_TYPE_NVIDIA_SHIELD_2017 | `"nvidia_shield_2017"` |
| SDL_GAMEPAD_TYPE_NEXUS_PLAYER | `"nexus_player"` |
| SDL_GAMEPAD_TYPE_RAZER_KISHI | `"razer_kishi"` |

For any SDL_GAMEPAD_TYPE not in this table, returns `"unknown"`.

### Controller mapping database

SDL3 ships a built-in mapping DB. If a controller is not recognized
(SDL_IsGamepad returns false for a known VID/PID), the extension can load a
custom `gamecontrollerdb.txt` via:

```lua
sdlinput.gamepad_add_mapping(mapping_string)  → ok|nil, err
```

In the future, we can bundle a `gamecontrollerdb.txt` from
https://github.com/gabomdq/SDL_GameControllerDB and load it on init.

## Implementation Steps

### Step 1: Build SDL3 static lib for Linux

- Create `scripts/build-sdl3.sh` with the cmake config above
- Clone SDL3 release tag, configure, build
- Copy `libSDL3.a` → `lib/x86_64-linux/`
- Verify file size (~18MB .a, final .so ~1-3MB)

### Step 2: Vendor SDL3 public headers

- Copy minimal `include/SDL3/` set from SDL3 source tree
- Only headers transitively #included by bridge code
- Test compile with the headers to catch missing deps

### Step 3: Write `sdlinput_bridge.cpp`

Implement the Lua API above. Key design decisions:
- Call `SDL_PumpEvents()` at the top of every gamepad state read function
- Track open handles (gamepad + HID) in a list for cleanup on finalize
- Handle conversion between Lua strings/numbers and SDL3 types
- Return `nil, error_string` on all failure paths
- Map SDL3 enum values to Lua string constants for gamepad types

### Step 4: Write `extension.cpp`

Standard Defold entry points:
- `AppInitializeSDLInput` — `SDL_Init(SDL_INIT_GAMEPAD)`
- `InitializeSDLInput` — register Lua module
- `FinalizeSDLInput` — close all tracked handles, `SDL_Quit()`

### Step 5: Write `sdlinput.script_api`

Annotate Lua API for Defold editor autocomplete.

### Step 6: Update `ext.manifest`

Ensure entries match the config above (libs: ["SDL3"], macOS frameworks).

### Step 7: Build test on extender

- Build with Defold, fix any compilation/link errors
- Test gamepad enumeration on desktop
- Verify `nm` on the .so — no unexpected video/audio symbols pulled in

### Step 8: Cross-compilation (future)

Set up build scripts for:
- x86_64-win32: mingw-w64 cross-compiler
- x86_64-osx / arm64-osx: osxcross or macOS CI runner
- wasm-web: emscripten SDK

## Risks

| Risk | Mitigation |
|------|-----------|
| `-DSDL_UNIX_CONSOLE_BUILD` not set → cmake error on Linux | Documented in cmake config |
| `-DSDL_STATIC_PIC=OFF` → linker error on .so build | Set `SDL_STATIC_PIC=ON` |
| Missing `SDL_PumpEvents()` → stale gamepad state | Bridge calls it before reads |
| glibc symbol version mismatch on extender | Build on appropriate base OS |
| `SDL_HasWindows` links video stub into .so | Documented, runtime behavior is safe |
| HID read/write blocks main thread | Documented for v1; async later |
| Controller not recognized by built-in DB | `gamepad_add_mapping()` API for custom DB |
| WASM toolchain not available | Deferred to step 8 |

## Build Script

`scripts/build-sdl3.sh` will be created in step 1. Template:

```bash
#!/bin/bash
# Usage: ./scripts/build-sdl3.sh <platform>
# Platforms: linux (only step 1; win32/osx/wasm later)

set -euo pipefail
PLATFORM=$1
SDL_TAG="release-3.2.x"
SDL_DIR="/tmp/SDL-$PLATFORM"
OUT_DIR="ext-sdlinput/lib/$PLATFORM"

CMAKE_ARGS=(
  -DBUILD_SHARED_LIBS=OFF
  -DSDL_STATIC=ON
  -DSDL_STATIC_PIC=ON
  -DSDL_UNIX_CONSOLE_BUILD=ON
  -DSDL_VIDEO=OFF -DSDL_AUDIO=OFF -DSDL_RENDER=OFF -DSDL_GPU=OFF
  -DSDL_CAMERA=OFF -DSDL_DIALOG=OFF -DSDL_TRAY=OFF
  -DSDL_PROCESS=OFF -DSDL_STORAGE=OFF
  -DSDL_JOYSTICK=ON -DSDL_HIDAPI=ON -DSDL_HIDAPI_LIBUSB=OFF -DSDL_SENSOR=ON -DSDL_HAPTIC=ON -DSDL_POWER=ON
  -DSDL_X11=OFF -DSDL_WAYLAND=OFF -DSDL_OPENGL=OFF -DSDL_VULKAN=OFF
  -DSDL_METAL=OFF -DSDL_COCOA=OFF -DSDL_DIRECTX=OFF
  -DSDL_ALSA=OFF -DSDL_PULSEAUDIO=OFF -DSDL_PIPEWIRE=OFF
  -DSDL_IBUS=OFF -DSDL_FCITX=OFF -DSDL_LIBURING=OFF
  -DSDL_VIRTUAL_JOYSTICK=OFF
)

clone_sdl() {
  git clone --depth 1 --branch "$SDL_TAG" https://github.com/libsdl-org/SDL "$SDL_DIR"
}

case $PLATFORM in
  linux)
    clone_sdl
    cd "$SDL_DIR"
    cmake -B build "${CMAKE_ARGS[@]}"
    cmake --build build -j"$(nproc)"
    mkdir -p "$OUT_DIR"
    cp build/libSDL3.a "$OUT_DIR/"
    echo "Built $OUT_DIR/libSDL3.a"
    ;;
  *)
    echo "Platform $PLATFORM not yet implemented"
    exit 1
    ;;
esac
```
