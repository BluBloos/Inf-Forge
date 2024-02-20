


# Automata-Engine

Framework for maybe writing games. Maybe also useful for writing general GUI applications.

![vkmonkeydemo](https://github.com/BluBloos/Automata-Engine/assets/38915815/32ad2e66-9517-40c0-95c1-ba309125a2ce)
![atomation](https://github.com/BluBloos/Automata-Engine/assets/38915815/d705874c-a4a9-4e48-97c9-e689bdade0f4)
![pathtracer](https://github.com/BluBloos/Automata-Engine/assets/38915815/cb31b64d-aa6d-4f42-82c4-27c2b1f43cfa)


## Supported Platforms

Windows.

## How to use this project

The project is setup for building via CMake.

### Building the examples

Simply use the CMake project located under the examples\ directory. This is a project with a single target per example.

### Game Setup

This framework is architected where the game is a service to the engine. The game must define callbacks for the engine
to call. The engine and game are statically linked together into a single target executable.

The engine will be added as a subdirectory in the game project's CMakeLists.txt. This will define a target, but will never call `project`.

```CMake
add_subdirectory(Automata-Engine)
```

Here is **how the engine expects the directory structure to be formatted:**

```text
<GameFolder>
  src\
  include\
  res\
```

The include folder is not required, but if present is added to the include path for the build.

When building a project, the engine will recursively copy the items in `res\` folder into another `res\` folder within
the directory where the target .exe resides. The directory structure of the output `res\` folder is flattened - any
subfolders within the original `res\` folder are no longer present in the final one.

Source files are recursively searched for within `src\` and added as sources for the target.

Some of these folder settings are defaults and can be overridden by CMake variables. For e.g., ProjectResourcesExtraPath
can be set as an additional folder used in the search for asset files.

### Selecting backend(s)

To select the backends to use, you must define a CMake variable `ProjectBackend`. It should be a space delimited string of
the following options:

```text
"GL_BACKEND" | "DX12_BACKEND" | "CPU_BACKEND" | "VK_BACKEND"
```

### CPU_BACKEND

In the `CPU_BACKEND`, the contract between engine and app is that app must populate `gameMemory->backbufferPixels`.

### GL_BACKEND

For `GL_BACKEND`, the engine will init window + OpenGL context. The engine manages when presentation happens, and when the application is called to update logic + make draw calls. The application calls `automata_engine::setUpdateModel()` to specify with detail how this works. Currently, `AUTOMATA_ENGINE_UPDATE_MODEL_ATOMIC` is the only supported update model.

Here's how that works:

```C++
// AUTOMATA_ENGINE_UPDATE_MODEL_ATOMIC:
while (_globalRunning) {
  // ... engine polls user input via OS
  // ... engine exec callback from ae::registerApp()
  glFlush();
  glWait();
  SwapBuffers();
}
```

### DX12 / VULKAN

these backends are experimental at the moment. to be honest, this entire framework is experimental :)

i.e., these do not have the same behavior as the gl_backend, where presentation is automatically managed by the engine.
