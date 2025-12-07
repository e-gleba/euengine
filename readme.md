![Preview](.github/preview.png)

# euengine

SDL3 GPU engine with hot-reload architecture.

## Technologies

- **SDL3** with GPU (Vulkan backend)
- **SDL3_mixer** for audio
- **SDL3_shadercross** for shader compilation
- **EnTT** ECS
- **GLM** for math
- **fastgltf** / **tinygltf** for GLTF/GLB support
- **ImGui** for UI
- **spdlog** for logging
- **yaml-cpp** for configuration
- **pybind11** for Python embedding

## Why not Unity/Godot/Unreal?

**Minimalist.** No editor bloat, no hidden complexity. Engine and game are cleanly separated.

**Hot reload.** Edit code, press F5. Game logic reloads without restart. Shaders reload automatically.

**Fast iteration.** `game.so` and `engine.exe` architecture means you rebuild only what changed. Engine stays running.

**SDL3 callbacks.** Modern event-driven design, not legacy game loops.

**Vulkan via SDL3 GPU.** Low-level control without Vulkan boilerplate.

**GLTF workflow.** Model in Godot, export GLTF, load directly. Use Godot as your front-end editor.

## Architecture

```
engine.exe  →  game.so
   ↑              ↓
   └──────────────┘
   hot reload
```

Engine runs as executable. Game logic lives in shared library. Reload game without restarting engine.

## Hot Reload

- **Game code**: Press F5 to reload `game.so`
- **Shaders**: Auto-reload on file change
- **Assets**: Rescan without restart

## GLTF Support

Full GLTF/GLB import. Model in Godot, export, load directly. Engine handles rendering, physics, and game logic.

## Building

```bash
cmake --preset clang
cmake --build --preset clang
```
