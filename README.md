<div align="center">
  <img src="EditorFrontend/public/apple-icon.png" width="120" alt="Wraith Engine" />
  <h1>Wraith Engine</h1>
  <p>A distributed game engine with a browser-based collaborative editor</p>
</div>

---

## Overview

Wraith Engine is a C++/Vulkan game engine runtime paired with a browser-based editor shell. The engine runs headless on a server, streams rendered viewports to browser clients via WebRTC with H.264, and processes editing commands through an authoritative command/event model. One shared runtime supports both local native editing and remotely hosted collaborative sessions.

## Architecture

```
Browser (React / Next.js)
  ├─ Editor UI  (outliner, inspector, toolbar, content browser)
  └─ WebRTC Viewport Client
          ↕  commands  /  H.264 video
AxiomRemoteViewportServer  (C++)
  └─ EditorSession  (authoritative scene state)
          └─ Vulkan Renderer  (offscreen, per-client)
```

## Features

**Engine**
- Vulkan rendering backend with MoltenVK on macOS
- Headless offscreen rendering with H.264 encoding (VideoToolbox on macOS)
- Authoritative command/event model for scene mutations
- DataModel scene hierarchy — folders, meshes, lights, cameras, actors
- Transform gizmos (translate / scale / rotate) with server-side hit-testing
- Multi-client rendering: each connected user gets their own viewport
- WebRTC streaming to browser

**Browser editor**
- Dockable panels: outliner, details/property inspector, content browser, toolbar
- Scene outliner with drag-drop reparenting, inline rename, right-click context menus
- Object lifecycle: create, duplicate, delete
- Per-client gizmo mode (Q / E / R shortcuts)
- User presence and camera visualization

## Prerequisites

- CMake 3.10+
- C++20 compiler (Clang or MSVC)
- Vulkan SDK / MoltenVK (macOS)
- Node.js 18+ and [pnpm](https://pnpm.io)
- macOS: Xcode command-line tools (required for VideoToolbox / WebRTC)

## Build & Run

### C++ engine

```bash
cmake --preset debug
cmake --build build/debug
```

### Remote viewport server

```bash
./build/debug/AxiomRemoteViewportServer --host 127.0.0.1 --port 8080 --width 1280 --height 720
```

### Browser editor

```bash
cd EditorFrontend
pnpm install
NEXT_PUBLIC_AXIOM_SERVER_ORIGIN=http://127.0.0.1:8080 pnpm dev
```

Open `http://localhost:3000` in your browser.

### Local native editor (no browser required)

```bash
./build/debug/AxiomEditor
```

### Tests

```bash
cmake --preset debug -DBUILD_TESTING=ON
cmake --build build/debug
ctest --test-dir build/debug
```

## Project Structure

| Path | Contents |
|------|----------|
| `Axiom/` | Engine library — core, session, renderer, remote transport |
| `Editor/` | Native GLFW + ImGui editor executable |
| `Headless/` | Headless runtime and `AxiomRemoteViewportServer` |
| `EditorFrontend/` | React / Next.js browser editor shell |
| `Content/` | Shaders and demo assets (.glb) |
| `Tests/` | Google Test suite |
| `Docs/` | Architecture design documents |
| `ThirdParty/` | Vendored dependencies |

## Tech Stack

| Layer | Technology |
|-------|------------|
| Engine | C++20, CMake, Vulkan |
| Windowing | GLFW, MoltenVK |
| Asset loading | fastgltf (glTF / glb) |
| Streaming | WebRTC, H.264 (VideoToolbox) |
| Browser editor | React 19, Next.js, TypeScript |
| Styling | Tailwind CSS, Radix UI |
| Testing | Google Test |

## License

MIT — see [LICENSE](LICENSE).
