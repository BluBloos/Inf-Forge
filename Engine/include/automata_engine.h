// Automata Engine, v1.0.0-alpha WIP
// (headers)

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

// NOTE(Noah): All subnamespaces in automata engine will define their capabilities
// within this file for the purpose of documentation.
// The purpose of any header for a specific subnamespace will be to define relevant
// types for that namespace.
//
// TODO: ^ I don't like this idea. I would prefer for example that we maybe forward declare all the types or something.
// Then we can define them somewhere at the lower end of this file.
// The goal is for a single include style model.

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
#include <cassert>
#include <tuple>
#include <iterator>
#include <string>

#if !defined(AUTOMATA_ENGINE_DISABLE_IMGUI)
#include <imgui.h>
#endif

#include <automata_engine_math.h>

#if defined(AUTOMATA_ENGINE_GL_BACKEND)
#include <automata_engine_gl.h>
#endif

// NOTE(Noah): No need to be adding some sort of CI so that we can be testing all the projects
// compat. All projects compat is responsibility of individual proj.
// They select a version of the engine. If they want the updated engine, maybe there is some incompat.
// But that is fine. That is just some work on behalf of the project. Things are seperated well.

namespace automata_engine {

    // ----------- [SECTION]     Forward declarations and basic types -----------
    struct game_memory_t;
    struct game_window_info_t;
    enum game_window_profile_t;
    enum   game_key_t;
    struct game_state_t; // this is define by custom game that sits on Automata Engine
    struct user_input_t; // TODO: prob change to game_user_input_t;

    // TODO: Types such as loaded_image_t, etc, aught to be scoped in the IO namespace.
    struct loaded_image_t;
    struct loaded_file_t;
    struct loaded_wav_t;
    struct raw_model_t;
    enum   update_model_t;
    // ----------- [END SECTION] Forward declarations and basic types -----------

    // ----------- [SECTION]    PreInit settings -----------
    extern game_window_profile_t defaultWinProfile;
    extern int32_t defaultWidth;
    extern int32_t defaultHeight;
    extern const char *defaultWindowName;
    // ----------- [END SECTION] PreInit settings -----------

    // ----------------- [SECTION] GAME BINDING POINTS -----------------
#pragma region GameBindingPoints
    void PreInit(game_memory_t *gameMemory);
    void Init(game_memory_t *gameMemory);
    void Close(game_memory_t *gameMemory);
    void HandleWindowResize(game_memory_t *gameMemory, int newWdith, int newHeight);

    // ----------------- [SECTION] AUDIO CALLBACKS -----------------
#pragma region AudioCallbacks
    /// @brief Main audio callback.
    void OnVoiceBufferProcess(
        game_memory_t *gameMemory, intptr_t voiceHandle, void *dst, void *src,
        uint32_t samplesToWrite, int channels, int bytesPerSample);
    /// @brief Called when a submitted buffer has finished playing. The implementation
    /// may make state changes to the voice.
    void OnVoiceBufferEnd(game_memory_t *gameMemory, intptr_t voiceHandle);
#pragma endregion  // AudioCallbacks
    // ----------------- [END SECTION] AUDIO CALLBACKS -----------------

#pragma endregion  // GameBindingPoints
    // ----------------- [END SECTION] GAME BINDING POINTS -----------------

    // engine helper funcs.
    game_state_t *getGameState(game_memory_t *gameMemory);
    void setGlobalRunning(bool); // this one enables more of a graceful exit.
    void setFatalExit();
    void setUpdateModel(update_model_t updateModel);
    const char *updateModelToString(update_model_t updateModel);
    void ImGuiRenderMat4(char *matName, math::mat4_t mat);
    void ImGuiRenderVec3(char *vecName, math::vec3_t vec);

#if defined(AUTOMATA_ENGINE_GL_BACKEND)
    namespace GL {
        // state that the engine exposes
        void GLClearError();
        bool GLCheckError(char *, const char *, int);
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
        void setUniformMat4f(GLuint shader, char *uniformName, math::mat4 val);
        // TODO(Noah): is there any reason that we cannot use templates for our createAndSetupVbo?
        vbo_t createAndSetupVbo(uint32_t counts, ...);
        GLuint createAndSetupVao(uint32_t attribCounts, ...);
    }
#endif

    namespace timing {
        float wallClock();
        float epoch();
    }

    namespace math {
        // TODO(Noah): Understand rvalues. Because, I'm a primitive ape, and,
        // they go right over my head, man.
        // TODO(Noah): Is there any way to expose member funcs for our math stuff
        // (declare them here) so that the documentation is there for what is defined?
        vec4_t operator*(mat4_t b, vec4_t a);
        vec4_t operator*=(vec4_t &a, float scalar);
        vec4_t operator+=(vec4_t &, vec4_t);
        vec4_t operator+(vec4_t b, vec4_t a);
        vec4_t operator*(vec4_t b, float a);

        mat4_t operator*(mat4_t a, mat4_t b);

        vec3_t operator+=(vec3_t &, vec3_t);
        vec3_t operator*(mat3_t b, vec3_t a);
        vec3_t operator*(vec3_t b, float a);
        vec3_t operator+(vec3_t b, vec3_t a);
        vec3_t operator-(vec3_t b, vec3_t a);

        float round(float a);
        float magnitude(vec3_t a);
        float dist(vec3_t a, vec3_t b);
        float dot(vec3_t a, vec3_t b);
        float project(vec3_t a, vec3_t b);
        vec3_t normalize(vec3_t a);
        vec3_t lookAt(vec3_t origin, vec3_t target);

        // returns true if wrote to intersectionOut
        bool rayBoxIntersection(
            vec3_t rayOrigin, vec3_t rayDir, float rayLen, const box_t *candidateBoxes,
            uint32_t candidateBoxCount, vec3_t *intersectionOut);

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

        float log10(float a);
        float log2(float a); // base 2
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

        float *value_ptr(vec3_t &);
        float *value_ptr(vec4_t &);
        float *value_ptr(mat3_t &);
        float *value_ptr(mat4_t &);
    }

    namespace super {
        // called by platform
        void init();
        // called by platform
        void close();
        // called by user
        void updateAndRender(game_memory_t * gameMemory);
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

    // Platform space are those functions that relate to doing something with the OS
    // Specifically, if calling the function requires any OS API to be made, that goes
    // in the platform layer. If calling the function uses cached information, but which
    // was initially retrieved by OS API, this too is a platform func. This case can
    // be classified as being temporally dependent: Ex) user input.
    namespace platform {

        static constexpr uint32_t AE_STDERR = 0;
        static constexpr uint32_t AE_STDOUT = 1;
        static constexpr uint32_t AE_STDIN = 2;

        // handle is one of AE_STDERR, AE_STDOUT, AE_STDIN
        void fprintf_proxy(int handle, const char *fmt, ...);

        // NOTE(Noah): Does not sit ontop of readEntireFile, so it's actually
        // a platform thing. BUT, thankfully, we have the impl within the
        // guts of automata, so no need for platform code to impl.
        loaded_image_t stbImageLoad(char *fileName);

        // get functions
        void getUserInput(user_input_t *userInput);
        game_window_info_t getWindowInfo();
        char *getRuntimeExeDirPath(char *pathOut, uint32_t pathSize);

        /// set the mouse position in client pixel coords.
        /// @param yPos y pos in client pixel coords.
        /// @param xPos x pos in client pixel coords.
        void setMousePos(int xPos, int yPos);

        void showMouse(bool);

        extern float lastFrameTime;
        extern float lastFrameTimeTotal;
        extern bool GLOBAL_RUNNING;
        extern bool GLOBAL_VSYNC;
        extern int GLOBAL_PROGRAM_RESULT;
        extern update_model_t GLOBAL_UPDATE_MODEL;


        void free(void *memToFree);
        void *alloc(uint32_t bytes);
        loaded_file_t readEntireFile(const char *fileName);
        bool writeEntireFile(const char *fileName, void *memory, uint32_t memorySize);
        void freeLoadedFile(loaded_file_t file);

        // --------- AUDIO ----------------
#pragma region PlatformAudio
        // TODO(Noah): allow for multiple buffer submissions to a voice. currently
        // we are doing a single buffer model.
        intptr_t createVoice();
        /// @return false on failure.
        bool voiceSubmitBuffer(intptr_t voiceHandle, loaded_wav_t wavFile);
        bool voiceSubmitBuffer(intptr_t voiceHandle, void *data, uint32_t size, bool shoudLoop = false);
        void voicePlayBuffer(intptr_t voiceHandle);
        void voiceStopBuffer(intptr_t voiceHandle);
        void voiceSetBufferVolume(intptr_t voiceHandle, float volume);
        float decibelsToAmplitudeRatio(float db);
        static const intptr_t INVALID_VOICE = UINT32_MAX;
#pragma endregion  // PlatformAudio
        // --------- END AUDIO ------------


        void showWindowAlert(const char *windowTitle, const char *windowMessage);

        // NOTE(Noah): setVsync is in platform layer because Vsync is an OS state
        // and we must make a call to OS to set it.
        void setVsync(bool);
    };
}

namespace ae = automata_engine;

/// Returns pos of last chr in str.
/// https://stackoverflow.com/questions/69121423/could-not-deduce-template-argument-for-const-char-n-from-const-char-6
/// ^ need (&str) to ensure param does not decay and still have type information.
template <std::size_t strLen>
static constexpr const char *__find_last_in_str(const char (&str)[strLen], const char chr) {
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

#if defined(_AUTOMATA_ENGINE_FILE_RELATIVE_)
#error "_AUTOMATA_ENGINE_FILE_RELATIVE_ already defined." \
    "Automata Engine tries to avoid bloat the global namespace, but this is a case where it is unavoidable."
#endif
#define _AUTOMATA_ENGINE_FILE_RELATIVE_ (__find_last_in_str("\\" __FILE__, '\\') + 1)


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

    /// @param sampleData pointer to contiguous chunk of memory corresponding to 16-bit PCA sound samples. When
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
};
// ---------- [END SECTION] Type Definitions ------------