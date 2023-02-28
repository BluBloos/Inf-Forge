// Automata Engine, v1.0.0-alpha WIP

// -------- [SECTION] Preamble --------

// Library Version
// (Integer encoded as AXYYZZSS for use in #if preprocessor conditionals, e.g. '#if AUTOENGINE_VERSION_NUM > 112345')
// A = alpha/beta/official_release
// alpha = 1, beta = 2, official_release = 3
// X = major version
// Y = minor version
// Z = patch version
// S = snapshot version
#define AUTOMATA_ENGINE_VERSION               "v1.0.0-alpha WIP"
#define AUTOMATA_ENGINE_VERSION_NUM           11000000

// This file is the primary file for the Automata Engine API.
// The documentation for Automata Engine is effectively this file along with the
// associated doxygen build.

// TODO: make using automata engine mean including this single .h

// -------- [END SECTION] Preamble --------

/*

Index of this file:
// [SECTION] Preamble
// [SECTION] Definable Macros
// [SECTION] Forward declarations and basic types
// [SECTION] PreInit settings
// [SECTION] GAME BINDING POINTS
// [SECTION] AUDIO CALLBACKS
// [SECTION] Platform Layer
// [SECTION] PLATFORM AUDIO
// [SECTION] Type Definitions

*/


// ----------------- [SECTION]     Definable Macros -----------------

// NOTE: these macros must be defined when including this header as well as
// for the .cpp files that are part of the engine.
// If you are using the CMake build system, it will define these macros for you.

// #define AUTOMATA_ENGINE_DISABLE_PLATFORM_LOGGING
// define this macro to disable platform logging.

// #define AUTOMATA_ENGINE_DISABLE_IMGUI
// define this macro to disable imgui.

// #define AUTOMATA_ENGINE_GL_BACKEND
// define this macro to use the OpenGL backend.

// #define AUTOMATA_ENGINE_VK_BACKEND
// define this macro to use the Vulkan backend.

// #define AUTOMATA_ENGINE_DX12_BACKEND
// define this macro to use the DirectX 12 backend.

// ----------------- [END SECTION] Definable Macros -----------------

#pragma once

// TODO: Cleanup this gist stuff.
#include <gist/github/nc_types.h>
#include <gist/github/nc_defer.h>
#include <gist/github/nc_stretchy_buffers.h>

#include <functional>
// TODO: We need to check cases where we assert but ought to replace with runtime logic.
#include <cassert>
#include <tuple>
#include <iterator>
#include <string>
#include <initializer_list>

#if !defined(AUTOMATA_ENGINE_DISABLE_IMGUI)
#include <imgui.h>
#endif

#if defined(AUTOMATA_ENGINE_GL_BACKEND)
#include <glew.h> // TODO: can we get rid of glew?
#include <gl/gl.h>
#include <gl/Glu.h>
// NOTE: glew must always be before gl.h
#endif

// TODO: Here we trust that if PI and DEGREES_TO_RADIANS are defined, they are defined correctly.
// we ought to implement some unit tests to ensure that this is the case.
#if !defined(PI)
#define PI 3.14159f
#endif
#if !defined(DEGREES_TO_RADIANS)
#define DEGREES_TO_RADIANS PI / 180.0f
#endif

namespace automata_engine {

// ----------- [SECTION]     Forward declarations and basic types -----------
    struct game_memory_t;
    struct game_window_info_t;
    enum   game_window_profile_t;
    enum   game_key_t;
    struct user_input_t; // TODO: prob change to game_user_input_t;

    // TODO: Types such as loaded_image_t, etc, ought to be scoped in the IO namespace.
    struct loaded_image_t;
    struct loaded_file_t;
    struct loaded_wav_t;
    struct raw_model_t;
    enum   update_model_t;

    namespace math {
        struct transform_t;
        struct camera_t;
        struct aabb_t;
        struct vec2_t;
        struct vec3_t;
        struct vec4_t;
        struct mat3_t;
        struct mat4_t;        
    };

#if defined(AUTOMATA_ENGINE_GL_BACKEND)
    namespace GL {
        struct vertex_attrib_t;
        struct vbo_t;
        struct ibo_t;
        struct vertex_attrib_desc_t;
    }
#endif

// ----------- [END SECTION] Forward declarations and basic types -----------

// ----------- [SECTION]    PreInit settings -----------
    extern game_window_profile_t defaultWinProfile;
    extern int32_t defaultWidth;
    extern int32_t defaultHeight;
    extern const char *defaultWindowName;
// ----------- [END SECTION] PreInit settings -----------

// ----------------- [SECTION] GAME BINDING POINTS -----------------
    void PreInit(game_memory_t *gameMemory);
    void Init(game_memory_t *gameMemory);
    void Close(game_memory_t *gameMemory);
    void HandleWindowResize(game_memory_t *gameMemory, int newWdith, int newHeight);

// ----------------- [SECTION] AUDIO CALLBACKS -----------------
    /// @brief Main audio callback.
    void OnVoiceBufferProcess(
        game_memory_t *gameMemory, intptr_t voiceHandle, void *dst, void *src,
        uint32_t samplesToWrite, int channels, int bytesPerSample);
    /// @brief Called when a submitted buffer has finished playing. The implementation
    /// may make state changes to the voice.
    void OnVoiceBufferEnd(game_memory_t *gameMemory, intptr_t voiceHandle);
// ----------------- [END SECTION] AUDIO CALLBACKS -----------------

// ----------------- [END SECTION] GAME BINDING POINTS -----------------

    // engine helper funcs.
    void setGlobalRunning(bool); // this one enables more of a graceful exit.
    void setFatalExit();
    void setUpdateModel(update_model_t updateModel);
    const char *updateModelToString(update_model_t updateModel);
    void ImGuiRenderMat4(const char *matName, math::mat4_t mat);
    void ImGuiRenderVec3(const char *vecName, math::vec3_t vec);

#if defined(AUTOMATA_ENGINE_GL_BACKEND)
    namespace GL {
        // state that the engine exposes
        void GLClearError();
        bool GLCheckError(const char *, const char *, int);
        extern bool glewIsInit;
        void initGlew();
        bool getGLInitialized();
        void objToVao(raw_model_t rawModel, ibo_t *iboOut, vbo_t *vboOut, GLuint *vaoOut);
        GLuint createShader(const char *vertFilePath, const char *fragFilePath, const char *geoFilePath = "\0");
        // TODO(Noah): There's got to be a nice and clean way to get rid of the duplication
        // here with the header of the wrapper.
        GLuint createTextureFromFile(
            const char *filePath,
            GLint minFilter = GL_LINEAR, GLint magFilter = GL_LINEAR,
            bool generateMips = true, GLint wrap = GL_CLAMP_TO_BORDER
        );
        GLuint createTexture(
            unsigned int *pixelPointer, unsigned int width, unsigned int height,
            GLint minFilter = GL_LINEAR, GLint magFilter = GL_LINEAR,
            bool generateMips = true, GLint wrap = GL_CLAMP_TO_BORDER
        );
        GLuint compileShader(uint32_t type, char *shader);
        void setUniformMat4f(GLuint shader, const char *uniformName, math::mat4_t val);
        // TODO(Noah): is there any reason that we cannot use templates for our createAndSetupVbo?
        vbo_t createAndSetupVbo(uint32_t counts, ...);
        GLuint createAndSetupVao(uint32_t attribCounts, ...);
    }
#endif

    namespace timing {
        float wallClock();
        float epoch();
    }

// TODO(Noah): Understand rvalues. Because, I'm a primitive ape, and,
// they go right over my head, man.

// TODO(Noah): Is there any way to expose member funcs for our math stuff
// (declare them here) so that the documentation is there for what is defined?

    namespace math {

        vec4_t operator+=(vec4_t &, vec4_t);
        vec3_t operator+=(vec3_t &, vec3_t);
        vec4_t operator+(vec4_t b, vec4_t a);
        vec3_t operator+(vec3_t b, vec3_t a);
        vec3_t operator-(vec3_t b, vec3_t a);

        vec4_t operator*=(vec4_t &a, float scalar);
        vec4_t operator*(vec4_t b, float a);
        vec4_t operator*(mat4_t b, vec4_t a);
        vec3_t operator*(mat3_t b, vec3_t a);
        mat4_t operator*(mat4_t a, mat4_t b);
        vec3_t operator*(vec3_t b, float a);

        float *value_ptr(vec3_t &);
        float *value_ptr(vec4_t &);
        float *value_ptr(mat3_t &);
        float *value_ptr(mat4_t &);

        float magnitude(vec3_t a);
        float dist(vec3_t a, vec3_t b);
        float dot(vec3_t a, vec3_t b);
        float project(vec3_t a, vec3_t b);

        vec3_t normalize(vec3_t a);
        vec3_t lookAt(vec3_t origin, vec3_t target);

        /// @param rayDir must be normalized.
        bool doesRayIntersectWithAABB(
            const vec3_t &rayOrigin, const vec3_t &rayDir, float rayLen,
            const aabb_t &candidateBox,
            float *tOut
        );

        mat4_t buildMat4fFromTransform(transform_t trans);
        mat4_t buildProjMat(camera_t cam);
        mat4_t buildViewMat(camera_t cam);
        mat4_t buildOrthoMat(camera_t cam);
        mat4_t buildInverseOrthoMat(camera_t cam);
        mat4_t buildRotMat4(vec3_t eulerAngles);
        mat4_t transposeMat4(mat4_t  mat);

        // TODO(Noah): Probably make these constexpr, inline, templates and FAST intrinsics.
        float sqrt(float a);
        float atan2(float a, float b);
        float round(float a);
        float log10(float a);
        float log2(float a);
        float deg2rad(float deg);
        float sin(float a);
        float cos(float a);
        template <typename T>
        T square(T a) {
            return a * a;
        }
        template <typename T>
        T min(T a, T b) {
            return (a < b) ? a : b;
        }
        template <typename T>
        T max(T a, T b) {
            return (a > b) ? a : b;
        }
        float abs(float a);
        float ceil(float a);
        float floor(float a);
    }

    namespace super {
        void init();  // not to be called by the user.
        void close(); // not to be called by the user.
        
        /// @brief presents an ImGui engine overlay
        void updateAndRender(game_memory_t * gameMemory);
        
        /// @brief a bool to control ALL ImGui rendering
        extern bool g_renderImGui;
    }

    // TODO(Noah): Add GPU adapter device name in updateApp ImGui idea :)
    typedef struct bifrost {
        static void registerApp(
            const char *appName,
            void (*callback)(game_memory_t *),
            void (*transInto)(game_memory_t *) = nullptr,
            void (*transOut)(game_memory_t *) = nullptr);
        static void updateApp(game_memory_t * gameMemory, const char *appName);
        static std::function<void(game_memory_t *)> getCurrentApp();
    } bifrost_t;

    // function that go here are those that are a layer above the read entire file call
    // these functions do further processing (parse a file format).
    namespace io {
        loaded_wav_t loadWav(const char *);
        void freeWav(loaded_wav_t wavFile);
        loaded_image_t loadBMP(const char *path);
        raw_model_t loadObj(const char *filePath);
        void freeObj(raw_model_t obj);
        void freeLoadedImage(loaded_image_t img);
        constexpr static uint32_t ENGINE_DESIRED_SAMPLES_PER_SECOND = 44100;
    };


// -------------------- [SECTION] Platform Layer --------------------
//
// The platform layer are those functions that do something with the OS.
// Specifically, functions in this section require any OS API call to be made,
// even if that call happened at a different time and we are merely accessing
// cached information.
//
    namespace platform {

        static constexpr uint32_t AE_STDERR = 0;
        static constexpr uint32_t AE_STDOUT = 1;
        static constexpr uint32_t AE_STDIN = 2;

        extern float lastFrameTime;
        extern float lastFrameTimeTotal;
        extern bool GLOBAL_RUNNING;
        extern bool GLOBAL_VSYNC;
        extern int GLOBAL_PROGRAM_RESULT;
        extern update_model_t GLOBAL_UPDATE_MODEL;

        /// @param handle is one of AE_STDERR, AE_STDOUT, AE_STDIN
        void fprintf_proxy(int handle, const char *fmt, ...);

        // NOTE(Noah): Does not sit ontop of readEntireFile, so it's actually
        // a platform thing. BUT, thankfully, we have the impl within the
        // guts of automata, so no need for platform code to impl.
        loaded_image_t stbImageLoad(char *fileName);

        // get functions
        void getUserInput(user_input_t *userInput);

        /// @brief used for getting the width and height of the window.
        game_window_info_t getWindowInfo();

        /// @brief get the path to directory the executable resides in.
        /// @param pathOut buffer to write path to.
        /// @param pathSize size of pathOut buffer.
        char *getRuntimeExeDirPath(char *pathOut, uint32_t pathSize);

        /// set the mouse position in client pixel coords.
        /// @param yPos y pos in client pixel coords.
        /// @param xPos x pos in client pixel coords.
        void setMousePos(int xPos, int yPos);

        /// @brief show or hide the mouse cursor.
        void showMouse(bool shouldShow);

        /// @brief show a window alert.
        void showWindowAlert(const char *windowTitle, const char *windowMessage);

        /// @brief Vsync is an OS state and we must make a call to OS to set it.
        /// @param value true to enable Vsync, false to disable.
        void setVsync(bool value);

        /// @brief free memory allocated by the platform layer.
        void free(void *memToFree);

        /// @brief allocate memory.
        /// @param bytes number of bytes to allocate.
        void *alloc(uint32_t bytes);

        loaded_file_t readEntireFile(const char *fileName);
        bool writeEntireFile(const char *fileName, void *memory, uint32_t memorySize);
        void freeLoadedFile(loaded_file_t file);

// --------- [SECTION]    PLATFORM AUDIO ----------------
//
// TODO(Noah): allow for multiple buffer submissions to a voice. currently
// we are doing a single buffer model.
//
        static const intptr_t INVALID_VOICE = UINT32_MAX;

        /// @return INVALID_VOICE on failure, a handle to the voice on success.
        intptr_t createVoice();

        /// @return false on failure.
        bool voiceSubmitBuffer(intptr_t voiceHandle, loaded_wav_t wavFile);

        /// @return false on failure.
        bool voiceSubmitBuffer(intptr_t voiceHandle, void *data, uint32_t size, bool shoudLoop = false);

        void voicePlayBuffer(intptr_t voiceHandle);
        void voiceStopBuffer(intptr_t voiceHandle);
        void voiceSetBufferVolume(intptr_t voiceHandle, float volume);

        /// @brief given the dB value, return the float multiplier to apply to LPCM float samples.
        float decibelsToAmplitudeRatio(float db);
//
// --------- [END SECTION] PLATFORM AUDIO ------------

    };

// -------------------- [END SECTION] Platform Layer --------------------

    namespace __details {
        /// Returns pos of last chr in str.
        /// https://stackoverflow.com/questions/69121423/could-not-deduce-template-argument-for-const-char-n-from-const-char-6
        /// ^ need (&str) to ensure param does not decay and still have type information.
        template <std::size_t strLen>
        static constexpr const char *find_last_in_str(const char (&str)[strLen], const char chr) {
            const char *lastPos = str;
            // strlen() can be a compile time thing depending on optimization level
            // of compiler.
            // https://stackoverflow.com/questions/67571803/how-to-get-string-length-in-the-array-at-compile-time
            for (uint32_t i = 0; (i < (strLen - 1)) && (str[i]); i++) {
                if (str[i] == chr)
                    lastPos = &(str[i]);
            }
            return lastPos;
        }
    }
}

namespace ae = automata_engine;

#if defined(_AUTOMATA_ENGINE_FILE_RELATIVE_) || defined(AELoggerError) || defined(AELoggerLog) || defined(AELoggerWarn) || defined(AELogger)
#error "Automata Engine tries to avoid bloat the global namespace, but these cases are unavoidable."
#endif

#define _AUTOMATA_ENGINE_FILE_RELATIVE_ (ae::__details::find_last_in_str("\\" __FILE__, '\\') + 1)

#if defined(AUTOMATA_ENGINE_GL_BACKEND)
    #if !defined(GL_CALL)
        #define GL_CALL(code) (ae::GL::GLClearError(), code, assert(ae::GL::GLCheckError(#code, _AUTOMATA_ENGINE_FILE_RELATIVE_, __LINE__)))
    #else
        #error "TODO"
    #endif
#endif

#if !defined(AUTOMATA_ENGINE_DISABLE_PLATFORM_LOGGING)
// See this page for color code guide:
// https://stackoverflow.com/questions/4842424/list-of-ansi-color-escape-sequences
#define AELoggerError(fmt, ...) \
    (ae::platform::fprintf_proxy(ae::platform::AE_STDERR, "\033[0;31m" "[Error on line=%d in file:%s]:\n" fmt "\n" "\033[0m", __LINE__, _AUTOMATA_ENGINE_FILE_RELATIVE_, __VA_ARGS__))
#define AELoggerLog(fmt, ...) \
    (ae::platform::fprintf_proxy(ae::platform::AE_STDOUT, "[Log from line=%d in file:%s]:\n" fmt "\n", __LINE__, _AUTOMATA_ENGINE_FILE_RELATIVE_, __VA_ARGS__))
#define AELoggerWarn(fmt, ...) \
    (ae::platform::fprintf_proxy(ae::platform::AE_STDOUT, "\033[0;93m" "[Warn on line=%d in file:%s]:\n" fmt  "\n" "\033[0m", __LINE__, _AUTOMATA_ENGINE_FILE_RELATIVE_, __VA_ARGS__))
#define AELogger(fmt, ...) (ae::platform::fprintf_proxy(ae::platform::AE_STDOUT, fmt, __VA_ARGS__))
#else
#define AELoggerError(fmt, ...)
#define AELoggerLog(fmt, ...)
#define AELoggerWarn(fmt, ...)
#define AELogger(fmt, ...)
#endif

// ---------- [SECTION] Type Definitions ------------
namespace automata_engine {

    /// @param data pointer to underlying memory provided by the platform layer.
    /// @param dataBytes amount of bytes that data takes up.
    /// @param isInitialized this is a boolean used by the game to note that
    /// the memory has appropriate content
    /// that we would expect to exist after calling EngineInit.
    struct game_memory_t {
        void *data;
        uint32_t dataBytes;
        bool isInitialized;
        uint32_t *backbufferPixels;
        uint32_t backbufferWidth;
        uint32_t backbufferHeight;
    };

    struct game_window_info_t {
        uint32_t width;
        uint32_t height;
        intptr_t hWnd;
        intptr_t hInstance;
    };

    /// @param pixelPointer pointer to contiguous chunk of memory corresponding to image pixels. Each pixel is
    /// a 32 bit unsigned integer with the RGBA channels packed each as 8 bit unsigned integers. This gives
    /// each channel 0->255 in possible value. Format is not definitively in RGBA order...
    struct loaded_image_t {
        uint32_t *pixelPointer;
        uint32_t width;
        uint32_t height;
    };

    /// @param contents pointer to unparsed binary data of the loaded file
    struct loaded_file_t {
        const char *fileName;
        void *contents;
        int contentSize;
    };

    /// @param sampleData pointer to contiguous chunk of memory corresponding to 16-bit LPCM sound samples. When
    /// there are two channels, the data is interleaved.
    /// @param parentFile internal storage for corresponding loaded_file that contains the unparsed sound data.
    /// This is retained so that we can ultimately free the loaded file.
    struct loaded_wav_t {
        int sampleCount;
        int channels;
        short *sampleData;
        struct loaded_file_t parentFile;
    };

    struct raw_model_t {
        char modelName[13];
        float *vertexData; // stretchy buf
        uint32_t *indexData; // stretchy buf
    };

    enum game_key_t {
        GAME_KEY_0 = 0, GAME_KEY_1, GAME_KEY_2, GAME_KEY_3,
        GAME_KEY_4, GAME_KEY_5, GAME_KEY_6, GAME_KEY_7, GAME_KEY_8,
        GAME_KEY_9,
        GAME_KEY_A, GAME_KEY_B, GAME_KEY_C, GAME_KEY_D, GAME_KEY_E,
        GAME_KEY_F, GAME_KEY_G, GAME_KEY_H, GAME_KEY_I, GAME_KEY_J,
        GAME_KEY_K, GAME_KEY_L, GAME_KEY_M, GAME_KEY_N, GAME_KEY_O,
        GAME_KEY_P, GAME_KEY_Q, GAME_KEY_R, GAME_KEY_S, GAME_KEY_T,
        GAME_KEY_U, GAME_KEY_V, GAME_KEY_W, GAME_KEY_X, GAME_KEY_Y,
        GAME_KEY_Z,
        GAME_KEY_SHIFT, GAME_KEY_SPACE, GAME_KEY_ESCAPE, GAME_KEY_F5,
        GAME_KEY_COUNT
    };

    struct user_input_t {
        int mouseX = 0;
        int mouseY = 0;
        int rawMouseX = 0;
        int rawMouseY = 0;
        int deltaMouseX = 0;
        int deltaMouseY = 0;
        bool mouseLBttnDown = false;
        bool mouseRBttnDown = false;
        // TODO(Noah): We will prolly want to change how we represent keys.
        bool keyDown[(uint32_t)GAME_KEY_COUNT];
    };

    // TODO: Since everything is already namespaced, we won't need to prefix enum IDs with AUTOMATA_ENGINE_ ...
    enum game_window_profile_t {
        AUTOMATA_ENGINE_WINPROFILE_RESIZE,
        AUTOMATA_ENGINE_WINPROFILE_NORESIZE
    };

    enum update_model_t {
        AUTOMATA_ENGINE_UPDATE_MODEL_ATOMIC = 0,
        AUTOMATA_ENGINE_UPDATE_MODEL_FRAME_BUFFERING,
        AUTOMATA_ENGINE_UPDATE_MODEL_ONE_LATENT_FRAME,
        AUTOMATA_ENGINE_UPDATE_MODEL_COUNT
    };

    namespace math {
#pragma pack(push, 4) // align on 4 bytes
        struct vec2_t {
            float x, y;
        };
        struct vec4_t;
        struct vec3_t {
            float x, y, z;
            constexpr vec3_t() : x(0), y(0), z(0) {};
            vec3_t(float, float, float);
            vec3_t(vec4_t);
            vec3_t operator-();
            float &operator[](int index);

        };
        struct vec4_t {
            float x, y, z, w;
            constexpr vec4_t() : x(0), y(0), z(0), w(0) {};
            vec4_t operator-();
            vec4_t(float, float, float, float);
            vec4_t(vec3_t, float);
            float &operator[](int index);
        };
        struct mat4_t;
        struct mat3_t {
            union {
                float mat[3][3];
                float matp[9];
                vec3_t matv[3];
            };
            mat3_t(std::initializer_list<float>);
            mat3_t(mat4_t);
        };
        struct mat4_t {
            union {
                float   mat[4][4];
                float matp[16];
                vec4_t matv[4];
            };
            mat4_t(); // TODO(Noah): see if we can get this to be constexpr.
            mat4_t(std::initializer_list<float>);
        };
#pragma pack(pop)

        /// @param pos         camera is located by pos
        /// @param scale       camera is scaled by scale
        /// @param eulerAngles camera is rotated about its own axis by eulerAngles.
        ///                    Euler angles apply in the following rotation order: Z, Y, X.
        struct transform_t {
            vec3_t pos;
            vec3_t eulerAngles;
            vec3_t scale;
        };

        /// @brief a struct to define a frustrum.
        /// The units for distance within this struct are up to the app. Probably world space coords.
        /// @param fov       is the field of vision. Angle is applied in the vertical.
        /// @param nearPlane is the distance from the camera origin to the near clipping plane.
        /// @param farPlane  is the distance from the camera origin to the far clipping plane.
        /// @param height    is the height of the viewport.
        /// @param width     is the width of the viewport.
        struct camera_t {
            transform_t trans;
            float fov;
            float nearPlane;
            float farPlane;
            int height;
            int width;
        };

        /// @brief A struct to define an AABB (axis aligned bounding box).
        /// The extent of the box is defined by the origin and halfDim, i.e.
        /// xMin=origin.x-halfDim.x, xMax=origin.x+halfDim.x, etc.
        ///
        /// @param origin is the center of the box.
        /// @param halfDim is the half of the dimensions of the box.
        /// @param min is the bottom left corner of the box.
        /// @param max is the top right corner of the box.
        struct aabb_t {
            vec3_t origin;
            vec3_t halfDim;
            vec3_t min;
            vec3_t max;
            static aabb_t make(vec3_t origin, vec3_t halfDim);
            static aabb_t fromCube(vec3_t bottomLeft, float width);
            static aabb_t fromLine(vec3_t p0, vec3_t p1);
        };
    }

#if defined(AUTOMATA_ENGINE_GL_BACKEND)
    namespace GL {
        struct vertex_attrib_t {
            GLenum type;
            uint32_t count;
            vertex_attrib_t(GLenum type, uint32_t count);
        };
        struct vbo_t {
            uint32_t glHandle;
            vertex_attrib_t *attribs; // stretchy buffer
        };
        struct ibo_t {
            uint32_t count;
            GLuint glHandle;
        };
        struct vertex_attrib_desc_t {
            uint32_t attribIndex;
            uint32_t *indices; // stretchy buffer
            vbo_t vbo;
            bool iterInstance;
            vertex_attrib_desc_t(
                uint32_t attribIndex,
                std::initializer_list<uint32_t> indices,
                vbo_t vbo,
                bool iterInstance);
        };
    }
#endif
};
// ---------- [END SECTION] Type Definitions ------------