// Automata Engine, v0.3.0-alpha WIP


// -------- [SECTION] Preamble --------

// Library Version
// (Integer encoded as AXYYZZSS for use in #if preprocessor conditionals, e.g. '#if AUTOENGINE_VERSION_NUM > 112345')
// A = alpha/beta/official_release
// alpha = 1, beta = 2, official_release = 3
// X = major version
// Y = minor version
// Z = patch version
// S = snapshot version
#define AUTOMATA_ENGINE_VERSION_STRING        "v0.3.0-alpha WIP"
#define AUTOMATA_ENGINE_VERSION_NUM           11000000

// This file is the primary file for the Automata Engine API.
// All documentation for Automata Engine is within this file.

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


// ----------------- [SECTION] Definable Macros -----------------

// NOTE: these macros must be defined when including this header as well as
// for the .cpp files that are part of the engine.
// If you are using the CMake build system, it will define these macros for you.

#if !defined(AUTOMATA_ENGINE_DISABLE_PLATFORM_LOGGING)
// define AUTOMATA_ENGINE_DISABLE_PLATFORM_LOGGING to disable platform logging.
#endif

#if !defined(AUTOMATA_ENGINE_DISABLE_IMGUI)
// define AUTOMATA_ENGINE_DISABLE_IMGUI to disable ImGui.
#endif

#if !defined(AUTOMATA_ENGINE_GL_BACKEND)
// define AUTOMATA_ENGINE_GL_BACKEND to use the OpenGL backend.
#endif

#if !defined(AUTOMATA_ENGINE_VK_BACKEND)
// define AUTOMATA_ENGINE_VK_BACKEND to use the Vulkan backend.
#endif

#if !defined(AUTOMATA_ENGINE_DX12_BACKEND)
// define AUTOMATA_ENGINE_DX12_BACKEND to use the DirectX 12 backend.
#endif

// ----------------- [END SECTION] Definable Macros -----------------


#pragma once

#include <automata_engine_utils.hpp>

#include <functional>
// TODO: We need to check cases where we assert but ought to replace with runtime logic.
#include <cassert>
#include <tuple>
#include <iterator>
#include <string>
#include <initializer_list>
#include <mutex>

#if !defined(AUTOMATA_ENGINE_DISABLE_IMGUI)
#include <imgui.h>
#endif

#if defined(AUTOMATA_ENGINE_GL_BACKEND)
#include <glew.h> // TODO: can we get rid of glew?
#include <gl/gl.h>
#include <gl/Glu.h>
// NOTE: glew must always be before gl.h
#endif

#if defined(AUTOMATA_ENGINE_DX12_BACKEND)
#define NOMINMAX
#include <D3d12.h>
#include <dxgi1_6.h>

// NOTE: for HLSL compilation.
#include <dxcapi.h>

#endif

#if defined(AUTOMATA_ENGINE_VK_BACKEND)

// TODO: we cannot define WIN32 here, automata_engine.hpp is meant to be platform agnostic.
#define VK_USE_PLATFORM_WIN32_KHR
#define NOMINMAX

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

// TODO: for now, this is the way. but in the future it would be nice to roll our own dynamic loading.
#include <Volk/volk.h>

// NOTE: for HLSL compilation.
#include <dxcapi.h>

#endif // AUTOMATA_ENGINE_VK_BACKEND




// NOTE: The printf and scanf family of functions are now defined inline.
// therefore we link otherwise XAPOBase.lib has an unresolved external symbol.
#pragma comment(lib, "legacy_stdio_definitions.lib")

// Here we trust that if PI and DEGREES_TO_RADIANS are defined that they are defined correctly.
// TODO: we ought to implement some compile-time unit tests to ensure that this is the case.
#if !defined(PI)
#define PI 3.14159f
#endif
#if !defined(DEGREES_TO_RADIANS)
#define DEGREES_TO_RADIANS PI / 180.0f
#endif

namespace automata_engine {


// ----------- [SECTION] Forward declarations and basic types -----------
    struct gpu_info_t;
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
        struct rect_t;
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

#if defined(AUTOMATA_ENGINE_VK_BACKEND)
    namespace VK {
        struct Sampler;
        struct CommandPool;
        struct CommandBuffer;
        struct ImageMemoryBarrier;
        struct ImageView;
    }
#endif
    
// ----------- [END SECTION] Forward declarations and basic types -----------


// ----------- [SECTION] PreInit settings -----------
    // These values are meant to be set by the game during its PreInit() function.
    // Otherwise, they are initialized to default values by the engine source.
    // These values are used as the initial values for what they respectively represent.
    // i.e. defaultWidth is the width used to create the platform window.

    /// @brief the profile used to create the platform window.
    ///
    /// this is used for eg. to set if the window can be resized or not.
    extern game_window_profile_t defaultWinProfile;

    /// @brief the width used to create the platform window.
    extern int32_t defaultWidth;

    /// @brief the height used to create the platform window.
    extern int32_t defaultHeight;

    /// @brief the title used to create the platform window.
    extern const char *defaultWindowName;
// ----------- [END SECTION] PreInit settings -----------


// ----------------- [SECTION] GAME BINDING POINTS -----------------

    // These functions are the binding points for the engine to call the game.
    // The game must implement these functions.

    /// @brief Called before the platform window is created.
    void PreInit(game_memory_t *gameMemory);

    /// @brief Called after the platform window is created and before the main game loop.
    /// Called on the main thread of execution and for eg. OpenGL calls are permitted here.
    /// This function should do the minimal graphics API work needed so that after it is complete,
    /// if the Update function is called the game is able to draw _at least something_ to the screen
    /// _without delay_.
    void Init(game_memory_t *gameMemory);

    /// @brief Called after Init and before the first Update call is made to any registered app.
    /// The execution thread is not the main thread and therefore for e.g. OpenGL calls CANNOT be made.
    /// This callback is useful for the game to do any general setup work that may take some time.
    /// It occurs during the engine intro sequence if any. Once complete, this function should set the
    /// gameMemory to initialized, after which the first call to some Update is permitted to occur.
    void InitAsync(game_memory_t *gameMemory);

    /// @brief Called when the engine has shut down and is requesting the game to release
    /// all resources.
    void Close(game_memory_t *gameMemory);

    /// @brief Called when the platform window is resized. Games typically use this to resize
    /// the swapchain.
    void HandleWindowResize(game_memory_t *gameMemory, int newWdith, int newHeight);


// ----------------- [SECTION] AUDIO CALLBACKS -----------------
    /// @brief Main audio callback.
    ///
    /// This is called by the audio thread. The game must fill
    /// the dst buffer with the requested number of samples using the src buffer. The channels
    /// are interleaved. The implementation may not make state changes to the voice.
    void OnVoiceBufferProcess(
        game_memory_t *gameMemory, intptr_t voiceHandle, float *dst, float *src,
        uint32_t samplesToWrite, int channels, int bytesPerSample);

    /// @brief Called when a submitted buffer has finished playing. The implementation
    /// may make state changes to the voice.
    void OnVoiceBufferEnd(game_memory_t *gameMemory, intptr_t voiceHandle);
// ----------------- [END SECTION] AUDIO CALLBACKS -----------------


// ----------------- [END SECTION] GAME BINDING POINTS -----------------


    /// @brief Sets the global running boolean.
    ///
    /// Providing a value of false signals to the engine that the game is ready to exit. The program will
    /// not exit until the next frame.
    void setGlobalRunning(bool);

    /// @brief Signals to the engine that the game is ready to exit and sets an error exit code.
    ///
    /// The program will not exit until the next frame.
    void setFatalExit();

    /// @brief Sets the update model used by the engine.
    void setUpdateModel(update_model_t updateModel);

    /// @brief Returns the human-readable string representation of the provided update model.
    const char *updateModelToString(update_model_t updateModel);

    /// @brief Helper function to render a math::mat4_t to the ImGui window.
    void ImGuiRenderMat4(const char *matName, math::mat4_t mat);

    /// @brief Helper function to render a math::vec3_t to the ImGui window.
    void ImGuiRenderVec3(const char *vecName, math::vec3_t vec);

#if defined(AUTOMATA_ENGINE_VK_BACKEND)
    void VK_CHECK(VkResult result);

    namespace VK {

        /// @brief create a structure suitable for creation of a VkImageView. this structure
        /// has sane default parameters which can be overriden by member calls.
        ImageView createImageView(VkImage image, VkFormat format);

        /// @brief get the current swapchain image for this frame.
        /// @returns the index of the backbuffer as found in the swapchain.
        uint32_t getCurrentBackbuffer(VkImage *image, VkImageView *view);

        /// TODO: once we add support for more advanced update models, I think this will need
        /// to be called per frame to get the specific fence for that frame.
        /// in the atomic_update, only one fence is required.
        ///
        /// @brief the engine expects that the client architect their frame such that all work
        /// for the frame is known to be complete one they signal this fence.
        VkFence *getFrameEndFence();

        /// @brief call vkCmdPipelineBarrier but submit only VkImageMemoryBarrier.
        void cmdImageMemoryBarrier(VkCommandBuffer cmd,
            VkPipelineStageFlags                   before,
            VkPipelineStageFlags                   after,
            uint32_t                               count,
            VkImageMemoryBarrier                  *pBarriers);

        /// @brief call vkBeginCommandBuffer with default parameters.
        VkResult beginCommandBuffer(VkCommandBuffer cmd);

        /// @brief return a wrapper class for a VkImageMemoryBarrier structure. this
        /// has sane default parameters which can be overriden by member calls.
        ImageMemoryBarrier imageMemoryBarrier(
            VkAccessFlags src, VkAccessFlags dst, VkImageLayout srcLayout, VkImageLayout dstLayout, VkImage img);

        /// @brief create a structure suitable for creation of a VkCommandPool. this structure
        /// has sane default parameters which can be overriden by member calls.
        CommandPool commandPoolCreateInfo(uint32_t queueIndex);

        /// @brief create a structure suitable for allocation of VkCommandBuffers. this structure
        /// has sane default parameters which can be overriden by member calls.
        CommandBuffer commandBufferAllocateInfo(uint32_t count, VkCommandPool pool);

        /// @brief create a structure suitable for creation of a VkSampler. this structure
        /// has sane default parameters which can be overriden by member calls.
        Sampler samplerCreateInfo();

        /// @brief get the memory index for the allocable memory kind that has all of the
        /// positive properties and none of the negative properties.
        uint32_t getDesiredMemoryTypeIndex(
            VkPhysicalDevice gpu, VkMemoryPropertyFlags positiveProp, VkMemoryPropertyFlags negativeProp);

        /// @brief performs a barebones initialization of vulkan.
        /// - sets us up with the first gpu found.
        /// - creates a single queue for that supports graphics and compute.
        /// - sets up the validation layers.
        /// - sets up a debug callback for validation layer errors.
        void doDefaultInit(VkInstance *pInstance,
            VkPhysicalDevice          *pGpu,
            VkDevice                  *pDevice,
            uint32_t                  *pQueueIndex,
            VkQueue                   *pQueue,
            VkDebugUtilsMessengerEXT  *pDebugCallback);

        /// @brief convert the vk result to string.
        const char *VkResultToString(VkResult result);

        /// @brief this is for use as a callback that an engine client can "just use".
        /// doDefaultInit will use this as the debug callback for validation layer error messages.
        VKAPI_ATTR VkBool32 VKAPI_CALL ValidationDebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
            VkDebugUtilsMessageTypeFlagsEXT                                                           messageType,
            const VkDebugUtilsMessengerCallbackDataEXT                                               *pCallbackData,
            void                                                                                     *pUserData);

        /// @brief create a buffer in a dumb way. i.e. allocate a single block of memory as large as the
        ///        buffer and this memory will be solely used for the buffer.
        size_t createBufferDumb(VkDevice device,
            size_t                       size,
            uint32_t                     heapIdx,
            VkBufferUsageFlags           usage,
            VkBuffer                    *bufferOut,
            VkDeviceMemory              *memOut);

        /// @brief same as createBufferDumb with the added utility of automatically mapping the buffer
        ///        so that the CPU can immediately throw the data in there.
        size_t createUploadBufferDumb(VkDevice device,
            size_t                             size,
            uint32_t                           heapIdx,
            VkBufferUsageFlags                 usage,
            VkBuffer                          *bufferOut,
            VkDeviceMemory                    *memOut,
            void                             **pData);

        /// @brief get the virtual address of the buffer. this is not the same as VkDeviceMemory.
        /// that is just an opapque handle to the memory.
        VkDeviceAddress getBufferVirtAddr(VkDevice device, VkBuffer buffer);

        /// @brief flush and unmap the upload VkBuffer that is already mapped from [0, size] and whose
        /// backing memory is mappedThing.
        void flushAndUnmapUploadBuffer(VkDevice device, size_t size, VkDeviceMemory mappedThing);

        /// @brief create a 2D image in a dumb way. many setting are default. this allocates a single block
        /// of memory as large as the image, which be used solely for it.
        size_t createImage2D_dumb(VkDevice device,
            uint32_t                       width,
            uint32_t                       height,
            uint32_t                       heapIdx,
            VkFormat                       format,
            VkImageUsageFlags              usage,
            VkImage                       *imageOut,
            VkDeviceMemory                *memOut);

        VkShaderModule loadShaderModule(
            VkDevice vkDevice, const char *filePathIn, const WCHAR *entryPoint, const WCHAR *profile);
    }  // namespace VK
#endif

#if defined(AUTOMATA_ENGINE_DX12_BACKEND)
    namespace DX {

    /// @brief finds the hardware adapter for use with creating the dx12 device.
    ///
    /// on failure, logs to console and returns nullptr in ppAdapter.
    ///
    /// @param ppAdapter has AddRef called. Caller of this func must do
    ///        ->Release() on adapter.
    ///
    void findHardwareAdapter(IDXGIFactory2 *dxgiFactory,
                             IDXGIAdapter1 **ppAdapter);

    /// @brief allocate a buffer for upload data to the GPU from the CPU.
    /// this buffer will exist in system RAM and this be CPU visible.
    /// the buffer will be automatically mapped after creation
    /// the virtual addr is returned through pData.
    ID3D12Resource *AllocUploadBuffer(ID3D12Device *device, UINT64 size,
                                      void **pData);

    } // namespace DX
#endif

    // TODO: maybe we actually care about HLSL if we aren't doing dx12/vk?
    // but for now we get build errors, so deferring this work until a later time.
#if defined(AUTOMATA_ENGINE_DX12_BACKEND) || defined(AUTOMATA_ENGINE_VK_BACKEND)

    namespace HLSL {

    // TODO: the below prob will not work in isolation (say if we did not define to use
    // any of the graphics API backends) as there is WCHAR.

    /// @brief compile an HLSL shader from file. automata engine bundles
    /// dxcompiler.dll
    ///        to compile shaders.
    bool compileBlobFromFile(const char *filePathIn,
        const WCHAR                     *entryPoint,
        const WCHAR                     *profile,
        IDxcBlob                       **blobOut,
        bool                             emitSpirv = false);

    /// @brief NOT to be called by user.
    void _close();
    }  // namespace HLSL

#endif

#if defined(AUTOMATA_ENGINE_GL_BACKEND)
    namespace GL {
        /// @brief Clears all OpenGL error flags.
        void GLClearError();

        /// @brief Checks all OpenGL error flags and prints them to the console.
        /// @returns false if any error has occurred.
        bool GLCheckError(const char *, const char *, int);

        /// @brief if glew is initialized.
        extern bool glewIsInit;

        /// @brief initialize glew.
        void initGlew();

        /// @brief Returns true if the OpenGL context has been initialized by the engine.
        bool getGLInitialized();

        /// @brief Converts a priorly parsed .OBJ into a VAO (Vertex Array Object).
        void objToVao(raw_model_t rawModel, ibo_t *iboOut, vbo_t *vboOut, GLuint *vaoOut);

        /// @brief Load, compile, and upload to GPU a GLSL shader program from disk.
        GLuint createShader(const char *vertFilePath, const char *fragFilePath, const char *geoFilePath = "\0");

        /// @brief Load, compile, and upload to GPU a GLSL compute shader program from disk.
        GLuint createComputeShader(const char *filePath);

        // TODO(Noah): There's got to be a nice and clean way to get rid of the duplication
        // here with the header of the wrapper.
        /// @brief Load and upload to GPU a texture from disk. This function supports the same file formats as stbi_load.
        GLuint createTextureFromFile(
            const char *filePath,
            GLint minFilter = GL_LINEAR, GLint magFilter = GL_LINEAR,
            bool generateMips = true, GLint wrap = GL_CLAMP_TO_BORDER
        );

        /// @brief Upload to GPU a texture from memory. The pixel data must be in 0xABGR (32bpp) format.
        GLuint createTexture(
            unsigned int *pixelPointer, unsigned int width, unsigned int height,
            GLint minFilter = GL_LINEAR, GLint magFilter = GL_LINEAR,
            bool generateMips = true, GLint wrap = GL_CLAMP_TO_BORDER
        );

        /// @brief Helper function to set a uniform 4x4 matrix in a shader program.
        void setUniformMat4f(GLuint shader, const char *uniformName, math::mat4_t val);

        // TODO(Noah): is there any reason that we cannot use templates for our createAndSetupVbo?
        /// @brief create and setup an OpenGL VBO (Vertex Buffer Object).
        ///
        /// This function takes a variadic list of GL::vertex_attrib_t structs. These are used to specify the layout of
        /// the vertex data in the VBO. The layout is interpreted as the dense concatenation of the structs in memory.
        /// For ex) ae::GL::vertex_attrib_t(GL_FLOAT, 3),
        ///         ae::GL::vertex_attrib_t(GL_FLOAT, 2),
        ///         ae::GL::vertex_attrib_t(GL_FLOAT, 3)
        /// would map to the following C struct: struct { float x, y, z, u, v, nx, ny, nz;
        /// @param counts the number of elements in the variadic argument list.
        vbo_t createAndSetupVbo(uint32_t counts, ...);

        /// @brief create and setup an OpenGL VAO (Vertex Array Object).
        ///
        /// This function takes a variadic list of GL::vertex_attrib_desc_t structs. These are used to specify how the
        /// VAO binds data to the shader program. i.e. this function makes calls to glVertexAttribPointer.
        /// @param attribCounts the number of elements in the variadic argument list.
        GLuint createAndSetupVao(uint32_t attribCounts, ...);
    }
#endif

    namespace timing {

        /// @brief Get the frequency of the timer in ticks.
        uint64_t getTimerFrequency();
        
        /// @brief Get a high resolution (<1us) time stamp that can be used for time-interval measurements. The units of time returned are in ticks.
        uint64_t wallClock();

        /// @brief same as wallClock but the time stamp is relative to the beginning of time for this application.
        uint64_t epoch();
    }

// TODO(Noah): Is there any way to expose member funcs for our math stuff
// (declare them here) so that the documentation is there for what is defined?

    namespace math {

        /// @brief the functions below are operator overloads for operations between two vectors.
        /// 3-dim vectors.
        vec3_t operator+=(vec3_t &, vec3_t);
        vec3_t operator+(vec3_t b, vec3_t a);
        vec3_t operator-(vec3_t b, vec3_t a);
        /// 4-dim vectors.
        vec4_t operator+(vec4_t b, vec4_t a);
        vec4_t operator+=(vec4_t &, vec4_t);

        /// @brief the functions below are operator overloads for scaling vectors.
        /// 3-dim vectors.
        vec3_t operator*(vec3_t b, float a);
        /// 4-dim vectors.
        vec4_t operator*=(vec4_t &a, float scalar);
        vec4_t operator*(vec4_t b, float a);

        /// @brief the functions below are operator overloads for matrix-vector multiplication.
        /// 3-dim vectors.
        vec3_t operator*(mat3_t b, vec3_t a);
        /// 4-dim vectors.
        vec4_t operator*(mat4_t b, vec4_t a);

        /// @brief the functions below are operator overloads for matrix-matrix multiplication.
        mat4_t operator*(mat4_t a, mat4_t b);

        /// @brief the functions below are for retrieving a pointer to the vector/matrix as a contiguous array of floats.
        float *value_ptr(vec3_t &);
        float *value_ptr(vec4_t &);
        float *value_ptr(mat3_t &);
        float *value_ptr(mat4_t &);

        /// @brief compute the magnitude of a vector.
        float magnitude(vec3_t a);

        /// @brief compute the distance between two points.
        float dist(vec3_t a, vec3_t b);

        /// @brief compute the dot product of two vectors.
        float dot(vec3_t a, vec3_t b);

        /// @brief compute the cross product of two vectors.
        vec3_t cross(vec3_t a, vec3_t b);

        /// @brief compute the magnitude of the projected vector a onto vector b.
        float project(vec3_t a, vec3_t b);

        /// @brief compute the angle in radians between two vectors.
        float angle(vec3_t a, vec3_t b);

        /// @brief compute the signed angle in radians between two vectors.
        /// @param N is the normal vector of the plane that A and B are on.
        float signedAngle(vec3_t a, vec3_t b, vec3_t N);

        /// @brief compute the normalized vector of a vector.
        vec3_t normalize(vec3_t a);

        // TODO: what is the expected initial orientation? does this break down and require a quaternion?
        /// @brief compute the euler angles required to rotate some body located at origin to look at target.
        vec3_t lookAt(vec3_t origin, vec3_t target);

        /// @brief compute if the infinite ray located at rayOrigin and pointing in rayDir intersects with the AABB.
        /// DEPRECATED. faceHitIdx is not used.
        /// @param rayDir must be normalized.
        bool doesRayIntersectWithAABB(
            const vec3_t &rayOrigin, const vec3_t &rayDir,
            const aabb_t &candidateBox, bool *exitedEarly=nullptr,
            int *faceHitIdx=nullptr
        );

        /// @brief compute if the infinite ray located at rayOrigin and pointing in rayDir intersects with the AABB.
        /// @param rayDir must be normalized.
        bool doesRayIntersectWithAABB2(const vec3_t &rayOrigin,
                                       const vec3_t &rayDir,
                                       const aabb_t &candidateBox,
                                       bool *exitedEarly = nullptr,
                                       int *faceHitIdx = nullptr);

        /// @brief build a 4x4 transformation matrix from a transform_t struct.
        mat4_t buildMat4fFromTransform(transform_t trans);

        /// @brief build a 4x4 projection matrix from a camera_t struct.
        mat4_t buildProjMat(camera_t cam);

        /// @brief build a 4x4 projection matrix from a camera_t struct for use within Vulkan apps.
        /// this version maps the near plane to NDC_z=0.
        /// this matrix will also work for DX.
        mat4_t buildProjMatForVk(camera_t cam);

        /// @brief build a 4x4 view matrix from a camera_t struct.
        mat4_t buildViewMat(camera_t cam);

        /// @brief build a 4x4 orthographic projection matrix from a camera_t struct.
        mat4_t buildOrthoMat(camera_t cam);

        /// @brief build a 4x4 inverse orthographic projection matrix from a camera_t struct.
        mat4_t buildInverseOrthoMat(camera_t cam);

        /// @brief build a 4x4 rotation matrix from a vec3_t of euler angles.
        /// Euler angles apply in the following rotation order: Z, Y, X.
        mat4_t buildRotMat4(vec3_t eulerAngles);

        /// @brief transpose a 4x4 matrix.
        mat4_t transposeMat4(mat4_t  mat);

        // TODO(Noah): Probably make many of the math funcs below constexpr, inline, templates, FAST intrinsics, etc.

        /// @brief compute the square root of a float.
        float sqrt(float a);

        /// @brief compute the absolute value of a float.
        float abs(float a);

        /// @brief compute the ceiling of a float.
        float ceil(float a);

        /// @brief compute the floor of a float.
        float floor(float a);

        /// @brief round a float to the nearest integer.
        float round(float a);

        /// @brief compute the base 10 logarithm of a float.
        float log10(float a);

        /// @brief compute the base 2 logarithm of a float.
        float log2(float a);

        /// @brief compute the division (a/b) followed by a ceiling operation.
        static uint32_t div_ceil(uint32_t a, uint32_t b) {
            return (a + (b-1))/b;
        }

        /// @brief return a as the next multiple of b. a is returned if it is already a multiple of b.
        static uint32_t align_up(uint32_t a, uint32_t b)
        {
            return div_ceil(a, b) * b;  //TODO: there are bit tricks to do this.
        }

        /// @brief compute the square of a float.
        template <typename T>
        T square(T a) {
            return a * a;
        }

        /// @brief compute the minimum of two values.
        template <typename T>
        T min(T a, T b) {
            return (a < b) ? a : b;
        }

        /// @brief compute the maximum of two values.
        template <typename T>
        T max(T a, T b) {
            return (a > b) ? a : b;
        }

        /// @brief get the sign of X and return -1 or 1.
        int sign(int x);

        /// @brief the functions below are various trig-related and transcendental functions.
        float atan2(float a, float b);
        float acos(float a);
        float deg2rad(float deg);
        float sin(float a);
        float cos(float a);
    }

    // this namespace is for the engine service to the engine itself.
    namespace super {
        void _init();  // NOT to be called by the user.
        void _close(); // NOT to be called by the user.

        /// @brief present an ImGui engine overlay.
        void updateAndRender(game_memory_t * gameMemory);

        /// @brief a bool to control ALL ImGui rendering.
        extern bool g_renderImGui;
    }

    // TODO(Noah): Add GPU adapter device name in updateApp ImGui idea :)
    typedef struct bifrost {
        /// @brief register an app with the ae::bifrost_t engine.
        /// @param appName   the name of the app to be registered.
        /// @param callback  the function to be called every frame by the engine.
        /// @param transInto the function to be called when the game is transitioned into this app.
        /// @param transOut  the function to be called when the game is transitioned out of this app.
        static void registerApp(
            const char *appName,
            void (*callback)(game_memory_t *),
            void (*transInto)(game_memory_t *) = nullptr,
            void (*transOut)(game_memory_t *) = nullptr);

        /// @brief transition into an app.
        /// @param appName the name of the app to transition into.
        static void updateApp(game_memory_t * gameMemory, const char *appName);

        /// @brief get the main update function for the currently selected app. 
        static std::function<void(game_memory_t *)> getCurrentApp();
    } bifrost_t;

    // AE input/output engine.
    namespace io {
        /// @brief load a .WAV file into memory. this must be freed with freeWav.
        loaded_wav_t loadWav(const char *);

        /// @brief free a loaded_wav_t.
        void freeWav(loaded_wav_t wavFile);

        /// @brief load a .BMP file into memory. this must be freed with freeLoadedImage.
        loaded_image_t loadBMP(const char *path);

        /// @brief load a .OBJ file into memory. this must be freed with freeObj.
        raw_model_t loadObj(const char *filePath);

        /// @brief free a raw_model_t.
        void freeObj(raw_model_t obj);

        /// @brief free a loaded_image_t.
        void freeLoadedImage(loaded_image_t img);

        /// @brief any .WAV file loaded must have this many samples per second.
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

        /// @brief a constant representing an error level of logging.
        static constexpr uint32_t AE_STDERR = 0;

        /// @brief a constant representing an info level of logging.
        static constexpr uint32_t AE_STDOUT = 1;

        /// @brief the time in seconds that the last frame took on CPU.
        extern float lastFrameTime;

        /// @brief the time in seconds that the last frame took in total.
        /// For eg. in ATOMIC update mode, this is the total `CPU -> Present-the-frame` time.
        extern float lastFrameTimeTotal;

        extern bool _globalRunning;                      // NOT to be set by the user.
        extern bool _globalVsync;                        // NOT to be set by the user.
        extern int  _globalProgramResult;                // NOT to be set by the user.
        extern void (*_redirectedFprintf)(const char *); // NOT to be set by the user.

        /// @brief the update model currently in use.
        extern update_model_t GLOBAL_UPDATE_MODEL;

        /// @brief prints to the console as if it were a printf call.
        ///
        /// STDERR, STDOUT, etc are not file handles but rather levels at which to print.
        /// This may impact for eg. the color of the text in a terminal.
        /// @param handle is one of AE_STDERR, AE_STDOUT
        void fprintf_proxy(int handle, const char *fmt, ...);

        /// @brief set the additional logger. fprintf_proxy will also print to fn.
        void setAdditionalLogger(void (*fn)(const char *));

        /// @brief read an image from disk into memory using stb_image.
        ///
        /// Must be freed with freeLoadedImage. The pixel data will be in 0xABGR (32bpp) format.
        loaded_image_t stbImageLoad(char *fileName);

        /// @brief get the lastly polled user input.
        void getUserInput(user_input_t *userInput);

        /// @brief get information about the platform window.
        game_window_info_t getWindowInfo();

        /// Set the mouse position in client pixel coords.
        /// See https://github.com/BluBloos/Atomation/wiki for client coords definition.
        /// @param yPos y pos in client pixel coords.
        /// @param xPos x pos in client pixel coords.
        void setMousePos(int xPos, int yPos);

        /// @brief show or hide the mouse cursor.
        void showMouse(bool shouldShow);

        /// @brief show a native OS window alert.
        /// @param bAsync if true the calling thread is not blocked.
        void showWindowAlert(const char *windowTitle, const char *windowMessage, bool bAsync=false);

        /// @brief Set Vsync behaviour.
        /// @param value true to enable Vsync, false to disable.
        void setVsync(bool value);

        /// @brief free memory allocated by platform::alloc.
        void free(void *memToFree);

        /// @brief allocate memory.
        /// @param bytes number of bytes to allocate.
        void *alloc(uint32_t bytes);

        /// @brief Reads an entire file from disk into memory. The memory must be freed later using freeLoadedFile.
        loaded_file_t readEntireFile(const char *fileName);

        /// @brief Writes an entire file to disk from memory.
        /// @returns true on success, false on failure.
        bool writeEntireFile(const char *fileName, void *memory, uint32_t memorySize);

        /// @brief free memory allocated by readEntireFile.
        void freeLoadedFile(loaded_file_t file);

        /// @brief get the path to the directory the executable resides in.
        /// @param pathOut buffer to write path to.
        /// @param pathSize size of pathOut buffer.
        char *getRuntimeExeDirPath(char *pathOut, uint32_t pathSize);

        /// @brief get the path to the directory where the user's data should be stored.
        /// on a platform like Windows for eg. this maps to %APPDATA%.
        std::string getAppDataPath();

        /// @brief check if a path exists.
        bool pathExists(const char *path);

        /// @brief create a directory at the given absolute path.
        /// @returns false on failure, true otherwise.
        bool createDirectory(const char *dirPath);

        /// @brief  get numGpus many GPU infos. the infos _MUST_ be provided back to AE to free the enumerated adapters.
        /// @param pInfo   output array with size numGpus to receive the gpu info into.
        /// @param numGpus the size of the pInfo array.
        void getGpuInfos(gpu_info_t *pInfo, uint32_t numGpus);

        /// @brief  free the priorly enumerated adapters.
        /// @param pInfo 
        /// @param numGpus 
        void freeGpuInfos(gpu_info_t *pInfo, uint32_t numGpus);

        /// @brief  get the current dedicated video memory usage for a GPU.
        /// @param gpuAdapter the GPU to get the memory for.
        size_t getGpuCurrentMemoryUsage(intptr_t gpuAdapter);


        /// @brief  get if the platform window is focused.
        bool isWindowFocused();

// --------- [SECTION] PLATFORM VK ----------------
//
#if defined(AUTOMATA_ENGINE_VK_BACKEND)
        namespace VK {
            /// @brief the client is to call this function to let the engine know what queue that the
            /// engine is to present the swapchain to. the engine also requires knowledge of the
            /// instance,device,and physical device. those parameters are cached for later use.
            /// the engine also initializes the swapchain for the first time through this call.
            void init(VkInstance instance, VkPhysicalDevice gpu, VkDevice device, VkQueue queue);
        };  // namespace VK
#endif
//
// --------- [END SECTION] PLATFORM VK ------------
        

// --------- [SECTION] PLATFORM AUDIO ----------------
//
// TODO(Noah): allow for multiple buffer submissions to a voice. currently
// we are doing a single buffer model.
//
        /// @brief a constant representing an invalid voice handle.
        static const intptr_t INVALID_VOICE = UINT32_MAX;

        /// @brief create a voice for playing audio.
        /// @returns INVALID_VOICE on failure, a handle to the voice on success.
        intptr_t createVoice();

        /// @brief submit a loaded_wav_t of sound data to a voice.
        /// The voice does not begin playing. Once playing, it will play the buffer to completion, then stop.
        /// @returns false on failure.
        bool voiceSubmitBuffer(intptr_t voiceHandle, loaded_wav_t wavFile);

        /// @brief submit a 16-bit LPCM buffer of sound data to a voice.
        /// The voice does not begin playing. Once playing, it will play the buffer to completion, then stop.
        /// @returns false on failure.
        bool voiceSubmitBuffer(intptr_t voiceHandle, void *data, uint32_t size, bool shouldLoop = false);

        /// @brief begin playing a voice.
        void voicePlayBuffer(intptr_t voiceHandle);

        /// @brief stop playing a voice.
        void voiceStopBuffer(intptr_t voiceHandle);

        /// @brief set the volume of a voice.
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
#error "Automata Engine tries to avoid bloat the global namespace, but these cases are decidedly exceptions."
#endif

#define _AUTOMATA_ENGINE_FILE_RELATIVE_ (ae::__details::find_last_in_str("\\" __FILE__, '\\') + 1)

#if defined(AUTOMATA_ENGINE_GL_BACKEND)
    #if !defined(GL_CALL)
        /// @brief Wrapper macro for OpenGL calls to assert on error.
        #define GL_CALL(code) (ae::GL::GLClearError(), code, assert(ae::GL::GLCheckError(#code, _AUTOMATA_ENGINE_FILE_RELATIVE_, __LINE__)))
    #else
        #error "TODO"
    #endif
#endif

#if !defined(AUTOMATA_ENGINE_DISABLE_PLATFORM_LOGGING)
// See this page for color code guide:
// https://stackoverflow.com/questions/4842424/list-of-ansi-color-escape-sequences

/// @brief Log an error message to the console.
#define AELoggerError(fmt, ...) \
    (ae::platform::fprintf_proxy(ae::platform::AE_STDERR, "\033[0;31m" "\n[error] on line=%d in file=%s\n" fmt "\n" "\033[0m", __LINE__, _AUTOMATA_ENGINE_FILE_RELATIVE_, __VA_ARGS__))

/// @brief Log a message to the console.
#define AELoggerLog(fmt, ...) \
    (ae::platform::fprintf_proxy(ae::platform::AE_STDOUT, "\n[log] from line=%d in file:%s\n" fmt "\n", __LINE__, _AUTOMATA_ENGINE_FILE_RELATIVE_, __VA_ARGS__))

/// @brief Log a warning message to the console.
#define AELoggerWarn(fmt, ...) \
    (ae::platform::fprintf_proxy(ae::platform::AE_STDOUT, "\033[0;93m" "\n[warn] on line=%d in file:%s\n" fmt  "\n" "\033[0m", __LINE__, _AUTOMATA_ENGINE_FILE_RELATIVE_, __VA_ARGS__))

/// @brief Log a message to the console without a newline.
#define AELogger(fmt, ...) (ae::platform::fprintf_proxy(ae::platform::AE_STDOUT, fmt, __VA_ARGS__))
#else
#define AELoggerError(fmt, ...)
#define AELoggerLog(fmt, ...)
#define AELoggerWarn(fmt, ...)
#define AELogger(fmt, ...)
#endif


// ---------- [SECTION] Type Definitions ------------
namespace automata_engine {

    /// @brief a structure for information about a GPU.
    /// @param description          the human-readable description of the GPU.
    /// @param vendorId             the PCI ID of the hardware vendor for this GPU.
    /// @param deviceId             the PCI ID of the GPU.
    /// @param dedicatedVideoMemory the total amount of dedicated video memory of the GPU.
    /// @param adapter              the handle to the adapter for this GPU.
    typedef struct gpu_info_t {
        char     description[128]     = {};
        uint32_t vendorId             = 0;
        uint32_t deviceId             = 0;
        size_t   dedicatedVideoMemory = 0;
        intptr_t adapter              = (intptr_t)nullptr;
    } gpu_info_t;

    /// @brief a struct allocated by the engine and passed to the game layer.
    ///
    /// The game layer can use this to store its own data. This struct is persistent across time.
    ///
    /// @param data          pointer to underlying memory provided by the platform layer.
    /// @param dataBytes     amount of bytes that data takes up.
    /// @param isInitialized this is a boolean used by the game to note that
    ///                      the memory is in a valid state, where "valid" is defined by we (the game).
    struct game_memory_t {
        // TODO: data may be written to from multiple threads.
        void    *data;
        uint32_t dataBytes;

        /// @brief the fields below are for CPU_BACKEND.
        uint32_t  *backbufferPixels;
        uint32_t   backbufferWidth;
        uint32_t   backbufferHeight;
        std::mutex m_mutex;
        void       setInitialized(bool newVal)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            isInitialized = newVal;
        }
        bool getInitialized()
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return isInitialized;
        }

    private:
        bool isInitialized=false;
    };

    /// @brief a struct representing information about a window.
    /// @param hWnd         opaque OS handle to the window.
    /// @param hInstance    opaque OS handle to the application instance that created the window.
    /// @param width        width of the window client rect in pixels.
    /// @param height       height of the window client rect in pixels.
    struct game_window_info_t {
        uint32_t width;
        uint32_t height;
        intptr_t hWnd;
        intptr_t hInstance;
    };

    /// @brief a struct representing a file loaded into memory.
    /// @param contents    pointer to unparsed binary data of the loaded file
    /// @param contentSize size of the contents in bytes
    struct loaded_file_t {
      const char *fileName;
      void *contents;
      int contentSize;
    };

    /// @brief a struct representing image data loaded into memory.
    /// @param pixelPointer pointer to contiguous chunk of memory corresponding to image pixels. Each pixel is
    ///                     a 32 bit unsigned integer with the RGBA channels packed each as 8 bit unsigned integers.
    ///                     This gives each channel 0->255 in possible value. Format is typically in 0xABGR order.
    /// @param parentFile   internal storage for corresponding loaded_file that contains the unparsed image data.
    ///                     This is retained so that we can ultimately free the loaded file.
    struct loaded_image_t {
        uint32_t *pixelPointer;
        uint32_t width;
        uint32_t height;
        struct loaded_file_t parentFile;
    };

    /// @brief a struct representing a .WAV file loaded into memory.
    /// @param sampleData pointer to contiguous chunk of memory corresponding to 16-bit LPCM sound samples. When
    ///                   there are two channels, the data is interleaved.
    /// @param parentFile internal storage for corresponding loaded_file that contains the unparsed sound data.
    ///                   This is retained so that we can ultimately free the loaded file.
    struct loaded_wav_t {
        int sampleCount;
        int channels;
        short *sampleData;
        struct loaded_file_t parentFile;
    };

    /// @brief a struct representing a 3D model file loaded into memory.
    /// @param vertexData a stretchy buffer corresponding to 3D model vertices. Each vertex contains
    ///                   (x,y,z, u,v, nx,ny,nz)
    /// @param indexData  a stretchy buffer corresponding to indices specifying triplets of triangle vertices.
    struct raw_model_t {
        char modelName[13];
        float    *vertexData;   // stretchy buf
        uint32_t *indexData;    // stretchy buf
    };

    /// @brief an enum for the different types of keys that can be pressed.
    enum game_key_t {
        GAME_KEY_0 = 0, GAME_KEY_1, GAME_KEY_2, GAME_KEY_3, GAME_KEY_4, GAME_KEY_5, GAME_KEY_6, GAME_KEY_7, GAME_KEY_8, GAME_KEY_9,
        GAME_KEY_A, GAME_KEY_B, GAME_KEY_C, GAME_KEY_D, GAME_KEY_E, GAME_KEY_F, 
        GAME_KEY_G, GAME_KEY_H, GAME_KEY_I, GAME_KEY_J, GAME_KEY_K, GAME_KEY_L,
        GAME_KEY_M, GAME_KEY_N, GAME_KEY_O, GAME_KEY_P, GAME_KEY_Q, GAME_KEY_R,
        GAME_KEY_S, GAME_KEY_T, GAME_KEY_U, GAME_KEY_V, GAME_KEY_W, GAME_KEY_X, 
        GAME_KEY_Y, GAME_KEY_Z,
        GAME_KEY_SHIFT, GAME_KEY_SPACE, GAME_KEY_ESCAPE, GAME_KEY_TAB, GAME_KEY_F5,
        GAME_KEY_COUNT
    };

    /// @brief a struct representing a snapshot of user input.
    /// @param mouseX         x position of the mouse in pixels.
    /// @param mouseY         y position of the mouse in pixels.
    /// @param deltaMouseX    change in x position of the mouse relative to the last snapshot.
    ///                       units are 0->65,535 : top-left -> bottom-right.
    /// @param deltaMouseY    change in y position of the mouse relative to the last snapshot.
    ///                       units are 0->65,535 : top-left -> bottom-right.
    /// @param mouseLBttnDown state of the left mouse button.
    /// @param mouseRBttnDown state of the right mouse button.
    /// @param keyDown        an array of booleans corresponding to the state of each key.
    struct user_input_t {
        int mouseX = 0;
        int mouseY = 0;
        int deltaMouseX = 0;
        int deltaMouseY = 0;
        bool mouseLBttnDown = false;
        bool mouseRBttnDown = false;
        // TODO(Noah): We will prolly want to change how we represent keys.
        bool keyDown[(uint32_t)GAME_KEY_COUNT];
    };

    // TODO: Since everything is already namespaced, we won't need to prefix enum IDs with `AUTOMATA_ENGINE_...`.
    /// @brief an enum for a window profile.
    enum game_window_profile_t {
        AUTOMATA_ENGINE_WINPROFILE_RESIZE,
        AUTOMATA_ENGINE_WINPROFILE_NORESIZE
    };

    /// @brief an enum for the different types of update models.
    /// NOTE: AUTOMATA_ENGINE_UPDATE_MODEL_ATOMIC is the only one that is currently supported.
    enum update_model_t {
        AUTOMATA_ENGINE_UPDATE_MODEL_ATOMIC = 0,
        AUTOMATA_ENGINE_UPDATE_MODEL_FRAME_BUFFERING,
        AUTOMATA_ENGINE_UPDATE_MODEL_ONE_LATENT_FRAME,
        AUTOMATA_ENGINE_UPDATE_MODEL_COUNT
    };

    namespace math {
#pragma pack(push, 4) // align on 4 bytes
        /// @brief a struct for a 2D vector.
        struct vec2_t {
            float x, y;
        };
        struct vec4_t;

        /// @brief a struct for a 3D vector.
        struct vec3_t {
            float x, y, z;
            constexpr vec3_t() : x(0), y(0), z(0) {};
            constexpr vec3_t(float x, float y, float z) : x(x), y(y), z(z) {};
            vec3_t(vec4_t);
            vec3_t operator-();
            float &operator[](int index);

        };

        /// @brief a struct for a 4D vector.
        struct vec4_t {
            float x, y, z, w;
            constexpr vec4_t() : x(0), y(0), z(0), w(0) {};
            constexpr vec4_t(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {};
            vec4_t operator-();            
            vec4_t(vec3_t, float);
            float &operator[](int index);
        };
        struct mat4_t;

        /// @brief a struct for a 3x3 matrix.
        struct mat3_t {
            union {
                float mat[3][3];
                float matp[9];
                vec3_t matv[3];
            };
            mat3_t(std::initializer_list<float>);
            mat3_t(mat4_t);
        };

        /// @brief a struct for a 4x4 matrix.
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

        /// @brief a struct to define a transform.
        /// @param pos         the position of the transform.
        /// @param scale       the scale of the transform.
        /// @param eulerAngles the rotation of the transform in eulerAngles.
        ///                    Euler angles apply in the following rotation order: Z, Y, X.
        struct transform_t {
            vec3_t pos;
            vec3_t eulerAngles;
            vec3_t scale;
        };

        /// @brief a struct to define a rectangle.
        /// @param x      is the bottom-left x position of the rectangle.
        /// @param y      is the bottom-left y position of the rectangle.
        /// @param width  is the width of the rectangle.
        /// @param height is the height of the rectangle.
        struct rect_t {
            float x;
            float y;
            float width;
            float height;
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
        /// @param origin  is the center of the box.
        /// @param halfDim is the half of the dimensions of the box.
        /// @param min     is the bottom left corner of the box.
        /// @param max     is the top right corner of the box.
        struct aabb_t {
            vec3_t origin;
            vec3_t halfDim;
            vec3_t min;
            vec3_t max;
            static aabb_t make(vec3_t origin, vec3_t halfDim);
            static aabb_t fromCube(vec3_t bottomLeft, float width);
            static aabb_t fromLine(vec3_t p0, vec3_t p1);
        };
    }  // namespace math

#if defined(AUTOMATA_ENGINE_VK_BACKEND)
    namespace VK {
        struct Sampler : public VkSamplerCreateInfo {
            Sampler &magFilter(VkFilter filter)
            {
                VkSamplerCreateInfo &ci = *this;
                ci.magFilter            = filter;
                return *this;
            }
        };
        struct CommandBuffer : public VkCommandBufferAllocateInfo {};
        struct CommandPool : public VkCommandPoolCreateInfo {};
        struct ImageMemoryBarrier : public VkImageMemoryBarrier {
            ImageMemoryBarrier &aspectMask(VkImageAspectFlags mask)
            {
                VkImageMemoryBarrier &ci       = *this;
                ci.subresourceRange.aspectMask = mask;
                return *this;
            }
        };

        struct ImageView : public VkImageViewCreateInfo {
            ImageView &aspectMask(VkImageAspectFlags mask)
            {
                VkImageViewCreateInfo &ci      = *this;
                ci.subresourceRange.aspectMask = mask;
                return *this;
            }
        };

    }  // namespace VK

#endif

#if defined(AUTOMATA_ENGINE_GL_BACKEND)
    namespace GL {

        /// @brief A struct to describe a vertex attribute.
        /// @param type     is the data type of the attribute.
        /// @param count    is the number of elements in the attribute.
        struct vertex_attrib_t {
            GLenum type;
            uint32_t count;
            vertex_attrib_t(GLenum type, uint32_t count);
        };

        /// @brief A wrapper struct around an OpenGL VBO (Vertex Buffer Object).
        /// @param attribs is a stretchy buffer of vertex_attrib_t structs describing the VBO.
        struct vbo_t {
            uint32_t glHandle;
            vertex_attrib_t *attribs; // stretchy buffer
        };

        /// @brief A wrapper struct around an OpenGL IBO (Index Buffer Object).
        /// @param count    is the number of indices in the IBO.
        struct ibo_t {
            uint32_t count;
            GLuint glHandle;
        };

        /// @brief A struct to describe a set of vertex attributes from a VBO.
        ///
        /// This is to be used with a call to createAndSetupVao.
        /// @param attribIndex  base slot in shader to bind attributes to.
        /// @param indices      stretchy buffer of indices of the attributes in the VBO to bind.
        /// @param vbo          the VBO to bind the attributes from.
        /// @param iterInstance whether or not to iterate the attributes per instance.
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
    }  // namespace GL
#endif
};
// ---------- [END SECTION] Type Definitions ------------
