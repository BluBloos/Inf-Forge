# Inf-Forge Engine

Inf-Forge is an engine for writing video games and graphical applications on Windows.

![vkmonkeydemo](https://github.com/BluBloos/Automata-Engine/assets/38915815/32ad2e66-9517-40c0-95c1-ba309125a2ce)
![atomation](https://github.com/BluBloos/Automata-Engine/assets/38915815/d705874c-a4a9-4e48-97c9-e689bdade0f4)
![pathtracer](https://github.com/BluBloos/Automata-Engine/assets/38915815/cb31b64d-aa6d-4f42-82c4-27c2b1f43cfa)

# Building the examples

Each example is a single build target, defined within examples\CMakeLists.txt.

# Using the framework to write an application 

Within the CMakeLists.txt for your project, select the backend(s) to use for
your project.

```CMake
# can be space delimited list to select multiple backends.
# the list of backends is: GL_BACKEND, DX12_BACKEND, VK_BACKEND, CPU_BACKEND.
set(ProjectBackend "GL_BACKEND")
```

Finally, add the engine subdirectory. This will define a target, but will never
call `project`.

```CMake
add_subdirectory(Automata-Engine)
```

**Projects written within Inf-Forge Engine must have a standard directory structure**:

```text
<ProjectFolder>
  src\
  include\
  res\
```

The `include\` folder is optional. Items within the `res\` directory tree will
be made available to your project builds. Each source file within the `source\`
directory tree will be used as a translation unit.

# Notes

There is intentionally little documentation for the Inf-Forge Engine. Users are
encouraged to read and experiment with the examples.

When using the `CPU_BACKEND`, the job of the render thread is to populate
`gameMemory->backbufferPixels`.

The application is a service to the engine. The application must define
callbacks for the engine to call. The application is compiled as a DLL and
consequentially can be hot-reloaded after recompilation by the engine.