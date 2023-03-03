
![engine_photo](https://user-images.githubusercontent.com/38915815/193379831-2a68ecbe-ef65-4e45-9353-c633f1fc598e.png)

# Automata-Engine

Build system for generic GUI applications.

## Supported Platforms

The only supported target platform currently is Windows.

## How to use this project

### Requirements

- Python + requests lib.
- CMake.

Rather unfortunately (I'm hoping to change this in the future!), you must install Python. The engine has been tested
with Python v3.10.5. Python is a dependency as some of the source tree is remotely sourced from my public gists on
Github.

The engine has been tested to work with CMake generators for Visual Studio 2019 and 2022.

### Game Setup

Currently, the game is a service to the engine. It must define callbacks for the engine to call. The engine and game
are statically linked together into a single target executable.

The game project itself is expected to be a CMake project. The engine will be added as a subdirectory in the game
project's CMakeLists.txt. Like so,

```CMake
add_subdirectory(Automata-Engine)
```

Here is **how the engine expects the directory structure to be formatted:**
```
<GameFolder>
  src\
  include\
  res\
```

When building a project, the engine will recursively copy the `res\` folder into the directory where the target .exe
resides. Source files are recursively searched for within `src\`. And the include path will have `include\` added to it.

Some of these folder settings are defaults and can be overridden by CMake variables. For eg. ProjectResourcesExtraPath
can be set to copy not only from `res\`, but from some additional folder too.

### Selecting a backend

Currently, Automata-Engine only supports OpenGL/CPU and for the forseeable future will only support a single backend at
runtime. To select a backend, you must define a CMake variable `ProjectBackend` to be one of the following:

```
"GL_BACKEND" | "DX12_BACKEND" | "CPU_BACKEND" | "VK_BACKEND"
```

### CPU_BACKEND

In the `CPU_BACKEND`, the contract between engine and app is that app must populate `gameMemory->backbufferPixels`.

### GL_BACKEND

For `GL_BACKEND`, the engine will init window + OpenGL context. The engine manages when presentation happens, and when the application is called to update logic + make draw calls. The application calls `automata_engine::setUpdateModel()` to specify with detail how this works. Currently, `AUTOMATA_ENGINE_UPDATE_MODEL_ATOMIC` is the only supported update model.

Here's how that works:
```C++
// AUTOMATA_ENGINE_UPDATE_MODEL_ATOMIC:
while (GLOBAL_RUNNING) {
  // ... engine polls user input via OS
  // ... engine exec callback from ae::registerApp()
  glFlush();
  glWait();
  SwapBuffers();
}
```
