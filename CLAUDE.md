# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Hycov is a Hyprland plugin that provides an overview/exposé mode — a grid-based tiling layout showing all windows on a workspace or monitor. Written in C++23, it hooks into Hyprland's internal APIs to intercept layout, input, and window management functions.

## Build Commands

**Meson (primary):**
```bash
meson setup build
meson compile -C build
```

**CMake (alternative):**
```bash
cmake -B build
cmake --build build
```

Both produce `build/libhycov.so`. Load with `hyprctl plugin load <path>/libhycov.so`.

## Architecture

### Core State Machine

The plugin operates as a state machine toggling between normal and overview modes. Global state flags in `globals.hpp` (`g_hycov_isOverView`, `g_hycov_isOverViewExiting`) control behavior throughout.

**Enter overview** (`dispatchers.cpp:dispatch_enteroverview`): Records every window's state (position, size, workspace, floating, fullscreen, group membership) into `SOvGridNodeData`, moves all windows to the active workspace, calculates a grid layout, and hooks Hyprland's layout algorithms to suppress reflow.

**Exit overview** (`dispatchers.cpp:dispatch_leaveoverview`): Restores windows to original workspaces, positions, sizes, floating/fullscreen states, and group membership. Unhooks all intercepted functions.

### File Roles

- **`main.cpp`** — Plugin init/exit lifecycle, config value registration, dispatcher registration
- **`dispatchers.cpp`** (largest file) — Enter/exit overview logic, focus movement, window state backup/restore
- **`globaleventhook.cpp`** — Function hook setup for keyboard, mouse, hotarea, and layout suppression hooks
- **`OvGridLayout.cpp/hpp`** — Grid position calculation: `cols = ceil(sqrt(N))`, `rows = ceil(N/cols)`, with gap/padding handling
- **`globals.hpp`** — All shared state: 26 config variables, 14 function hooks, state flags, layout instance

### Hook System

The plugin uses `HyprlandAPI::createFunctionHook()` to intercept 14 Hyprland functions at runtime. Layout algorithm hooks (`recalculate` for Dwindle/Master) are replaced with no-ops during overview to prevent reflow. Input hooks handle hotarea detection, alt-release exit, and click-to-focus/close.

### Key Implementation Details

- Uses `#define private public` to access Hyprland private members (noted as GCC 15 workaround)
- Uses `setValueAndWarp()` instead of direct assignment to stop in-progress animations when repositioning windows
- Recreates destroyed workspaces on exit if original workspace was removed during overview
- Avoids `moveWindowToWorkspaceSafe()` during restore to prevent crashes from dangling layout pointers

## Dispatchers (User-Facing Commands)

- `hycov:toggleoverview [forceall|forceallinone|onlycurrentworkspace]`
- `hycov:enteroverview [forceall|forceallinone|onlycurrentworkspace]`
- `hycov:leaveoverview`
- `hycov:movefocus [l|r|u|d|leftcross|rightcross|upcross|downcross]`

## Dependencies

Hyprland (v0.45.2+), pixman-1, libdrm, pango, pangocairo. All resolved via pkg-config.
