
![engine_photo](https://user-images.githubusercontent.com/38915815/193379831-2a68ecbe-ef65-4e45-9353-c633f1fc598e.png)

# Automata-Engine

Build system for generic GUI applications.

P.S. those cool examples seen in the photo above will be released soon!
- Raytracer: https://github.com/BluBloos/Raytracer
- MonkeyDemo: https://github.com/BluBloos/MonkeyDemo

# Steps for Building

## Preamble

You must install MS Visual Studio 2022, Community ed. The engine depends on MSVC (Microsoft Visual Studio Compiler). Specifically, the engine will exec the command below,
```
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
```  
So, if you have Visual Studio installed but this repo is still giving you trouble, please note that VS should be at the location specified above.

Rather unfortunately (I'm hoping to change this in the future!), you must also install Python. The engine has been tested with Python v3.10.5. Python is a dependency as some of the source tree is remotely sourced from my public gists on Github.

## How

At the moment, it does not make sense to build this engine without some sort of accompanying project. More specifically, this engine is a toolset for building generic GUI apps.

To build a given project with the engine, simply run the build.bat within the cli\ folder.

Now, before I speak on that, here is **how 
the engine expects the directory structure to be formatted:**
```
<GameFolder>
  src\
  include\
  res\
```

When building a project, the engine will recursively copy the res\ folder into the \<GameFolder\>\\bin\\. Any header files are expepcted to be within include\\, and the source code, in, well, src\\


As promised, here's how to do the build
```
cli\build.bat <backendName> <sourceDirs>
```

Above, \<backendName\> and \<sourceDirs\> are meant to be replaced.

```
<backendName> ::= "GL_BACKEND" | "DX12_BACKEND" | "CPU_BACKEND"

<sourceDirs> ::= "" | '"' <relativePathList> '"'

<relativePathList> ::= <term> | <term> ":" <relativePathList>
```

Above, \<term\> is a folder path, which may be relative to the root app directory. The list \<sourceDirs\> specifies to the engine where source files are located. The engine will compile all *.cpp files it can find (the engine will only search the top level directory). If \<sourceDirs\> is empty, the engine will default to searching within src\\.

## Backends

In the `CPU_BACKEND`, the contract between engine and app is that app must populate `gameMemory->backbufferPixels`.

For `GL_BACKEND`, the engine will init window + OpenGL context. The engine manages when presentation happens, and when the application is called to update logic + make draw calls. The application calls `automata_engine::setUpdateModel()` to specify with detail how this works. Currently, `AUTOMATA_ENGINE_UPDATE_MODEL_ATOMIC` is the only supported. 

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

In the future, the `UpdateAndRender` callback will be split into `Update` and `Render` (to support other update models). The terminology `UpdateAndRender` implicitly refers to the callback given to `ae::registerApp()`, so the actual function name may be anything.

Finally, `DX12_BACKEND` is "in the works".

# Some Notes

The only supported target platform, currently, is Windows. This is not final, and in the future there will be many supported target platforms. The architecture of the codebase is such that these ports to other platforms can be done with ease ... (famous last words, I know).
