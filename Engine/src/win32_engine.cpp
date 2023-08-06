
// TODO: the engine layer should not know about the stb_image. we want this to be a thing that
// only the game knows how to do.
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

// TODO(Noah): Understand rvalues. Because, I'm a primitive ape, and,
// they go right over my head, man.

#if defined(AUTOMATA_ENGINE_VK_BACKEND)
#define VOLK_IMPLEMENTATION
#endif

#include <automata_engine.hpp>
#include <win32_engine.h>

#define NOMINMAX
#include <windows.h>
#include <io.h> // TODO(Noah): What is this used for again?
#define XAUDIO2_HELPER_FUNCTIONS
#include <xaudio2.h>
#include <xapobase.h>
#include <mmreg.h> // TODO(Noah): What is this used for again?

// TODO(Noah): Remove dependency on all this crazy
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>     /* for _O_TEXT and _O_BINARY */

// for raw mouse input
#include <hidusage.h>

// for scaling factor (DPI stuff)
#include <winuser.h>
#include <shellscalingapi.h>
#include <shtypes.h>

#include <timeapi.h> // for timeBeginPeriod.

#pragma comment(lib, "Winmm.lib")

// TODO: there is this idea where the engine is doing some logging.
// it just seems like that this shouldn't happen?
// it seems that the game should be responsible for this?
// the "engine" should just be a lightweight thing that the game sits on.
// that way, the game is the same exact thing every time.

// TODO: it looks like there was a reason that the old Win32GameLoad used
// this sort of struct thing. It was I think for reusability between projects.
// so, we should strive to have all the Win32* functions to do _just_ win32 stuff.

// TODO: use a big struct for all the globals and a simple helper func to return a
// reference to that struct. that's good design :)

#define MAX_CONSOLE_LINES 500

static HWND g_hwnd             = NULL;
static HWND g_consoleHwnd      = NULL;
static HWND g_userInputHwnd    = NULL;

// the value of the HANDLE is read from two threads, but not modified.
// of course, the actual handle object itself is very much accessed by
// multiple threads, but Windows OS handles that sort of sync.
static HANDLE g_inputThreadEvent = NULL;

static ae::game_memory_t   g_gameMemory     = {};
static ae::engine_memory_t g_engineMemory   = {};
static win32_backbuffer_t  globalBackBuffer = {};

static bool g_bIsWindowFocused = true;

static ae::engine_memory_t *ae::EM = nullptr;

void (*g_redirectedFprintf)(const char *) = nullptr;

typedef void (*PFN_GameHandleWindowResize)(ae::game_memory_t *, int, int);
typedef ae::PFN_GameFunctionKind (*PFN_GameGetUpdateAndRender)(ae::game_memory_t *);
typedef void (*PFN_GameOnVoiceBufferEnd)(ae::game_memory_t *gameMemory, intptr_t voiceHandle);
typedef void (*PFN_GameOnVoiceBufferProcess)(ae::game_memory_t *gameMemory,
    intptr_t                                                    voiceHandle,
    float *                                                     dst,
    float *                                                     src,
    uint32_t                                                    samplesToWrite,
    int                                                         channels,
    int                                                         bytesPerSample);

/// On the Windows platform, when a WM_SIZE message is recieved, this callback is invoked.
/// the provided width and height are the client dimensions of the window.
static PFN_GameHandleWindowResize   GameHandleWindowResize   = nullptr;
static ae::PFN_GameFunctionKind     GameInit                 = nullptr;
static ae::PFN_GameFunctionKind     GamePreInit              = nullptr;
static ae::PFN_GameFunctionKind     GameHandleInput          = nullptr;
static ae::PFN_GameFunctionKind     GameCleanup              = nullptr;
static PFN_GameGetUpdateAndRender   GameGetUpdateAndRender   = nullptr;
static PFN_GameOnVoiceBufferProcess GameOnVoiceBufferProcess = nullptr;
static PFN_GameOnVoiceBufferEnd     GameOnVoiceBufferEnd     = nullptr;

static ae::PFN_GameFunctionKind GameOnHotload = nullptr;
static ae::PFN_GameFunctionKind GameOnUnload = nullptr;

LARGE_INTEGER Win32GetWallClock();

namespace ae = automata_engine;

static LONGLONG g_PerfCountFrequency64;

void Platform_setMousePos(int xPos, int yPos)
{
    POINT pt = {xPos, yPos};
    ClientToScreen(g_hwnd, &pt);
    SetCursorPos(pt.x, pt.y);
}


// mouse begins as shown.
static std::atomic<bool> g_showMouseRequestedValue = true;
static bool g_showMouseCurrentValue = true;


void Platform_showMouse(bool show) {
    g_showMouseRequestedValue.store(show);
}

// NOTE: the reason for this deferral is because ShowCursor must be called on the same thread that
// created the window.
void Platform_showMouse_deferred(bool show) {
    if (!show)
    {
        while(ShowCursor(FALSE) >= 0);
    }
    else
    {
        while(ShowCursor(TRUE) < 0);
    }
}

void Platform_setAdditionalLogger(void (*fn)(const char *))
{
    g_redirectedFprintf = fn;
}

void Platform_freeLoadedFile(ae::loaded_file_t file)
{
    if (file.contents)
        VirtualFree(file.contents, 0, MEM_RELEASE);
}

static void LogLastError(DWORD lastError, const char *message) {
    char *lpMsgBuf;
    FormatMessage(
        // FORMAT_MESSAGE_FROM_SYSTEM     -> search the system message-table resource(s) for the requested message
        // FORMAT_MESSAGE_ALLOCATE_BUFFER -> allocates buffer, places pointer at the address specified by lpBuffer
        // FORMAT_MESSAGE_IGNORE_INSERTS  -> Insert sequences in the message definition such as %1 are to be
        //                                   ignored and passed through to the output buffer unchanged.
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        lastError,  // dwMessageId
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR) &lpMsgBuf, // lpBuffer
        0, // no need for nSize with FORMAT_MESSAGE_ALLOCATE_BUFFER
        NULL // not using any insert values such as %1
    );
    // TODO(Noah): Remove newline character at the end of lpMsgBuf
    AELoggerError("%s with error=%s", message, lpMsgBuf);
    LocalFree(lpMsgBuf);
}

bool Platform_writeEntireFile(
    const char *fileName, void *memory, uint32_t memorySize)
{
    bool Result = false;

	HANDLE FileHandle = CreateFileA(fileName, GENERIC_WRITE,0,0,CREATE_ALWAYS,0,0);
	if (FileHandle != INVALID_HANDLE_VALUE)
	{
		DWORD BytesWritten;
		if (WriteFile(FileHandle,memory,memorySize,&BytesWritten,0)) 
		{
			//File read succesfully
			Result = ((int)BytesWritten == memorySize);
		}	
		else
		{
			AELoggerError("Could not write to file %s", fileName);
		}		
		CloseHandle(FileHandle);
	}
	else
	{
		AELoggerError("Could not create file %s", fileName);
	}
	return Result;
}

ae::loaded_file_t Platform_readEntireFile(const char *fileName)
{
	void *result = 0;
	int fileSize32 = 0;
	HANDLE fileHandle = CreateFileA(fileName, GENERIC_READ,
		FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
	if (fileHandle != INVALID_HANDLE_VALUE) {
		LARGE_INTEGER fileSize;
		if (GetFileSizeEx(fileHandle, &fileSize)) {
			// TODO(Noah): Add a #define for maximum file size value.
            // Also, is it not the case they we further restrict ourselves to
            // a maximum file size?
			assert(fileSize.QuadPart <= 0xFFFFFFF);
			fileSize32 = (int)fileSize.QuadPart;
			result = VirtualAlloc(0, fileSize32, MEM_COMMIT, PAGE_READWRITE);
			if (result != NULL) {
				DWORD bytesRead;
				if (ReadFile(fileHandle, result, fileSize32, &bytesRead, 0) &&
                    (fileSize32 == (int)bytesRead))
				{
					// File read succesfully!
				}
				else
                {
                    DWORD error = GetLastError();
                    LogLastError(error, "Could not read file");
					VirtualFree(result, 0, MEM_RELEASE);
					result = 0;
				}
			} else {
                AELoggerError("Could not allocate memory for file %s", fileName);
            }
		} else {
            DWORD error = GetLastError();
            LogLastError(error, "Could not read file");
        }
		CloseHandle(fileHandle);
	} else {
        DWORD error = GetLastError();
        LogLastError(error, "Could not read file");
    }
	ae::loaded_file_t fileResult = {};
	fileResult.contents = result;
	fileResult.contentSize = fileSize32;
    fileResult.fileName = fileName;
    if (fileResult.contents) {
        AELoggerLog("File '%s' read successfully", fileName);
    }
	return fileResult;
}

static bool g_isImGuiInitialized = false;
#if !defined(AUTOMATA_ENGINE_DISABLE_IMGUI)
#include "imgui.h"
#include "imgui_impl_win32.h" // includes Windows.h for us

ImGuiContext* Platform_imguiGetCurrentContext()
{
    return ImGui::GetCurrentContext();
}

void Platform_imguiGetAllocatorFunctions(ImGuiMemAllocFunc *af, ImGuiMemFreeFunc *ff, void**pdata){
    ImGui::GetAllocatorFunctions(af, ff, pdata);
}

#endif

static HINSTANCE g_hInstance = NULL;

static inline int sign(int x)
{
    // NOTE: this is branchless. that makes it fast because branch prediction is expensive to fail.
    // it is expensive to fail due to the opportunity cost of the preemptive execution.
    return (x > 0) - (x < 0);
}

ae::game_window_info_t Platform_getWindowInfo(bool useCache)
{
    static ae::game_window_info_t winInfo;
    winInfo.hWnd      = (intptr_t)g_hwnd;
    winInfo.hInstance = (intptr_t)g_hInstance;
    if (g_hwnd == NULL) { AELoggerError("g_hwnd == NULL"); }
    RECT rect;
    if (!useCache && GetClientRect(g_hwnd, &rect)) {
        winInfo.width    = rect.right - rect.left;
        int signedHeight = rect.top - rect.bottom;
        winInfo.height   = sign(signedHeight) * signedHeight;
    }
    winInfo.isFocused = g_bIsWindowFocused;
    return winInfo;
}

#if defined(AUTOMATA_ENGINE_VK_BACKEND)

// NOTE: currently there are some hard limitations on this system but that is OK.
// all work/presentation must go through a single queue/device pair.
// if the app wants to use more queues and devices, it needs to synchronize on its
// end.
uint32_t         g_vkQueueIndex = 0;
VkQueue          g_vkQueue      = VK_NULL_HANDLE;
VkPhysicalDevice g_vkGpu        = VK_NULL_HANDLE;
VkDevice         g_vkDevice     = VK_NULL_HANDLE;
VkInstance       g_vkInstance   = VK_NULL_HANDLE;

VkSwapchainKHR g_vkSwapchain           = VK_NULL_HANDLE;
VkSurfaceKHR   g_vkSurface             = VK_NULL_HANDLE;
VkImage       *g_vkSwapchainImages     = nullptr;  // stretchy buffer
VkImageView   *g_vkSwapchainImageViews = nullptr;  // stretchy buffer
VkFormat       g_vkSwapchainFormat     = VK_FORMAT_UNDEFINED;
VkFence        g_vkPresentFence        = VK_NULL_HANDLE;
uint32_t       g_vkCurrentImageIndex   = 0;

static constexpr uint32_t g_vkDesiredSwapchainImageCount = 2;

const char *VkResultToString(VkResult result)
{
#define STR(r)                                                                                                         \
    case VK_##r:                                                                                                       \
        return #r

    switch (result) {
        STR(NOT_READY);
        STR(TIMEOUT);
        STR(EVENT_SET);
        STR(EVENT_RESET);
        STR(INCOMPLETE);
        STR(ERROR_OUT_OF_HOST_MEMORY);
        STR(ERROR_OUT_OF_DEVICE_MEMORY);
        STR(ERROR_INITIALIZATION_FAILED);
        STR(ERROR_DEVICE_LOST);
        STR(ERROR_MEMORY_MAP_FAILED);
        STR(ERROR_LAYER_NOT_PRESENT);
        STR(ERROR_EXTENSION_NOT_PRESENT);
        STR(ERROR_FEATURE_NOT_PRESENT);
        STR(ERROR_INCOMPATIBLE_DRIVER);
        STR(ERROR_TOO_MANY_OBJECTS);
        STR(ERROR_FORMAT_NOT_SUPPORTED);
        STR(ERROR_SURFACE_LOST_KHR);
        STR(ERROR_NATIVE_WINDOW_IN_USE_KHR);
        STR(SUBOPTIMAL_KHR);
        STR(ERROR_OUT_OF_DATE_KHR);
        STR(ERROR_INCOMPATIBLE_DISPLAY_KHR);
        STR(ERROR_VALIDATION_FAILED_EXT);
        STR(ERROR_INVALID_SHADER_NV);
        default:
            return "UNKNOWN_ERROR";
    }

#undef STR
}

void VK_CHECK(VkResult err)
{
    if (err) {
        AELoggerError("Detected Vulkan error: %s", VkResultToString(err));
        // TODO: there is probably a much better way to handle this...
        abort();
    }
}

#if !defined(AUTOMATA_ENGINE_DISABLE_IMGUI)
#include "imgui_impl_vulkan.h"
VkFramebuffer *g_vkImguiFramebuffers = nullptr;
VkRenderPass   g_vkImguiRenderPass   = VK_NULL_HANDLE;

static VkRenderPass vk_MaybeCreateImguiRenderPass()
{
    if (g_vkImguiRenderPass == VK_NULL_HANDLE) {
        VkAttachmentDescription attachment = {};
        attachment.format                  = g_vkSwapchainFormat;
        attachment.samples                 = VK_SAMPLE_COUNT_1_BIT;
        attachment.loadOp        = VK_ATTACHMENT_LOAD_OP_LOAD;  // imgui should not clear contents that we already have.
        attachment.storeOp       = VK_ATTACHMENT_STORE_OP_STORE;
        attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.stencilStoreOp              = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout               = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachment.finalLayout                 = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        VkAttachmentReference color_attachment = {};
        color_attachment.attachment            = 0;
        color_attachment.layout                = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        VkSubpassDescription subpass           = {};
        subpass.pipelineBindPoint              = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount           = 1;
        subpass.pColorAttachments              = &color_attachment;
        VkRenderPassCreateInfo info            = {};
        info.sType                             = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        info.attachmentCount                   = 1;
        info.pAttachments                      = &attachment;
        info.subpassCount                      = 1;
        info.pSubpasses                        = &subpass;
        VK_CHECK(vkCreateRenderPass(g_vkDevice, &info, nullptr, &g_vkImguiRenderPass));
    }
    return g_vkImguiRenderPass;
}

// TODO: impl other models. here would need an array for frame data!
void PlatformVK_renderAndRecordImGui(VkCommandBuffer cmd)
{
    assert(g_engineMemory.g_updateModel == ae::AUTOMATA_ENGINE_UPDATE_MODEL_ATOMIC);
    if (g_engineMemory.g_updateModel == ae::AUTOMATA_ENGINE_UPDATE_MODEL_ATOMIC) {

        ImGui::Render();

        VkCommandBufferBeginInfo info = {};
        info.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VK_CHECK(vkBeginCommandBuffer(cmd, &info));

        ae::game_window_info_t winInfo = Platform_getWindowInfo(false);

        VkRenderPassBeginInfo rpInfo    = {};
        rpInfo.sType                    = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpInfo.renderPass               = vk_MaybeCreateImguiRenderPass();
        rpInfo.framebuffer              = g_vkImguiFramebuffers[g_vkCurrentImageIndex];
        rpInfo.renderArea.extent.width  = winInfo.width;
        rpInfo.renderArea.extent.height = winInfo.height;
        vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

        // Record dear imgui primitives into command buffer
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

        vkCmdEndRenderPass(cmd);

        VK_CHECK(vkEndCommandBuffer(cmd));
    } else {
        // TODO:
        assert(false);
    }
}

#endif

VkFormat PlatformVK_getSwapchainFormat() { return g_vkSwapchainFormat; }

uint32_t PlatformVK_getCurrentBackbuffer(VkImage *image, VkImageView *view)
{
    *image = g_vkSwapchainImages[g_vkCurrentImageIndex];
    *view  = g_vkSwapchainImageViews[g_vkCurrentImageIndex];

    return g_vkCurrentImageIndex;
}

VkFence *PlatformVK_getFrameEndFence()
{
    return &g_vkPresentFence;
}

static LARGE_INTEGER vk_WaitForAndResetFence(VkDevice device, VkFence *pFence, uint64_t waitTime = 1000 * 1000 * 1000)
{
    VkResult result = (vkWaitForFences(device,
        1,
        pFence,
        // wait until all the fences are signaled. this blocks the CPU thread.
        VK_TRUE,
        waitTime));
    // TODO: there was an interesting bug where if I went 1ms on the timeout, things were failing,
    // where the fence was reset too soon. figure out what what going on in that case.

    // grab a timestamp.
    LARGE_INTEGER timestamp = Win32GetWallClock();

    if (result == VK_SUCCESS) {
        // reset fences back to unsignaled so that can use em' again;
        vkResetFences(device, 1, pFence);
    } else {
        AELoggerError("some error occurred during the fence wait thing., %s", VkResultToString(result));
    }

    return timestamp;
}

// TODO: I'm noticing that there are some functions that are pure (don't touch globals), and other
// functions do touch globals. so, how can we make it clear which are which? is it a naming convention?
static LARGE_INTEGER vk_getNextBackbuffer()
{
#if _DEBUG
    if (!(g_vkDevice && g_vkSwapchain && g_vkPresentFence)) {
        AELoggerError("vk_getNextBackbuffer called with invalid state.");
    }
#endif

    // Retrieve the index of the next available presentable image
    VK_CHECK(vkAcquireNextImageKHR(g_vkDevice,
        g_vkSwapchain,
        UINT64_MAX /* UINT64_MAX,timeout */,
        nullptr,
        g_vkPresentFence
        /* fence to signal */,
        &g_vkCurrentImageIndex));

    if (g_engineMemory.g_updateModel == ae::AUTOMATA_ENGINE_UPDATE_MODEL_ATOMIC) {
        return vk_WaitForAndResetFence(g_vkDevice, &g_vkPresentFence);
    }

    return Win32GetWallClock();
}

// TODO: this function is meant to be reentrant, so that we may recreate the swapchain when
// we want to render to a different image size. however, we do all this work that seems like
// we could just cache it.
static bool vk_initSwapchain(uint32_t desiredWidth, uint32_t desiredHeight)
{
    VkSurfaceCapabilitiesKHR surface_properties;
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_vkGpu, g_vkSurface, &surface_properties));

    uint32_t            format_count;
    VkSurfaceFormatKHR *formats = nullptr;
    defer(StretchyBufferFree(formats));
    vkGetPhysicalDeviceSurfaceFormatsKHR(g_vkGpu, g_vkSurface, &format_count, nullptr);
    StretchyBufferInitWithCount(formats, format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(g_vkGpu, g_vkSurface, &format_count, formats);

    VkSurfaceFormatKHR format;
    // we prefer sRGB for display
    // sRGB is a color space.
    // and there is also the idea of the nonlinear gamma transform of the colors
    // from this linear space before it hits the monitor.
    // so we need to apply the inverse transform before that on our end,
    // putting our colors into a nonlinear space.
    if (format_count == 0) {
        AELoggerError("Surface has no formats.");
        return false;
    }
    if ((format_count == 1) && (formats[0].format == VK_FORMAT_UNDEFINED)) {
        format        = formats[0];
        format.format = VK_FORMAT_B8G8R8A8_SRGB;
    } else {
        format.format = VK_FORMAT_UNDEFINED;
        for (uint32_t i = 0; i < StretchyBufferCount(formats); i++) {
            auto candidate = formats[i];
            switch (candidate.format) {
                case VK_FORMAT_R8G8B8A8_SRGB:
                case VK_FORMAT_B8G8R8A8_SRGB:
                case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
                                        format = candidate;
                                        break;
                default:
                                        break;
            }
            if (format.format != VK_FORMAT_UNDEFINED) { break; }
        }
        if (format.format == VK_FORMAT_UNDEFINED) { format = formats[0]; }
    }
    g_vkSwapchainFormat = format.format;

    VkExtent2D swapchain_size = surface_properties.currentExtent;
    if (surface_properties.currentExtent.width == 0xFFFFFFFF) {
        swapchain_size.width  = desiredWidth;
        swapchain_size.height = desiredHeight;
    } else if ((swapchain_size.width != desiredWidth) || (swapchain_size.height != desiredHeight)) {
        AELoggerError("swapchain extent different from window client area");
    }

    // TODO: for atomic model, we should not require two swapchain images?
    // there will never be two in flight, so these seem like wasted surfaces.
    uint32_t desired_swapchain_images = ae::math::max(g_vkDesiredSwapchainImageCount, surface_properties.minImageCount);
    if ((surface_properties.maxImageCount > 0) && (desired_swapchain_images > surface_properties.maxImageCount)) {
        desired_swapchain_images = surface_properties.maxImageCount;
    }

    // Find a supported composite type.
    VkCompositeAlphaFlagBitsKHR composite = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    if (surface_properties.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) {
        composite = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    } else if (surface_properties.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR) {
        composite = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
    } else if (surface_properties.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR) {
        composite = VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
    } else if (surface_properties.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR) {
        composite = VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR;
    }

    VkSwapchainKHR oldSwapchain = g_vkSwapchain;

    VkSwapchainCreateInfoKHR info = {};
    info.sType                    = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    info.surface                  = g_vkSurface;
    info.minImageCount            = desired_swapchain_images;
    info.imageFormat              = format.format;
    info.imageColorSpace          = format.colorSpace;
    info.imageExtent.width        = swapchain_size.width;
    info.imageExtent.height       = swapchain_size.height;
    info.imageArrayLayers         = 1;  // not a multi-view/stereo surface
    info.imageUsage               = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    info.imageSharingMode         = VK_SHARING_MODE_EXCLUSIVE;

    // this is a transform that is applied before presentation.
    info.preTransform   = (surface_properties.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
                              ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR
                              : surface_properties.currentTransform;
    info.compositeAlpha = composite;

    info.presentMode = VK_PRESENT_MODE_FIFO_KHR;  // FIFO is supported by all implementations.

    // try to find the present mode that we actually want.

    uint32_t presentModeCount;
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(g_vkGpu, g_vkSurface, &presentModeCount, NULL));
    VkPresentModeKHR *presentModes = nullptr;
    defer(StretchyBufferFree(presentModes));
    StretchyBufferInitWithCount(presentModes, presentModeCount);
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(g_vkGpu, g_vkSurface, &presentModeCount, presentModes));

    // TODO: consider vsync ON/OFF setting by user. or maybe consider not having that
    // setting at all.
    bool             bHasFallback = false;
    VkPresentModeKHR desiredMode  = VK_PRESENT_MODE_IMMEDIATE_KHR;
    VkPresentModeKHR fallbackMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    for (size_t i = 0; i < presentModeCount; i++) {
        if (presentModes[i] == desiredMode) {
            info.presentMode = desiredMode;
            break;
        }

        if ((presentModes[i] == fallbackMode)) { bHasFallback = true; }
    }

    if ((info.presentMode == VK_PRESENT_MODE_FIFO_KHR) && (bHasFallback)) { info.presentMode = fallbackMode; }

    // (clipped = true) is allow for more efficient presentation methods. It is
    // a don't care on our part for what pixels vals are stored in surface, for example,
    // on regions where another window is overlapping our window.
    info.clipped      = true;
    info.oldSwapchain = oldSwapchain;

    VK_CHECK(vkCreateSwapchainKHR(g_vkDevice, &info, nullptr, &g_vkSwapchain));

    // need to remove the prior swapchain + other objects after now creating a new one
    if (oldSwapchain != VK_NULL_HANDLE) {
        uint32_t image_count = StretchyBufferCount(g_vkSwapchainImageViews);
        for (uint32_t i = 0; i < image_count; i++) {
            auto view = g_vkSwapchainImageViews[i];
            vkDestroyImageView(g_vkDevice, view, nullptr);
        }
#if !defined(AUTOMATA_ENGINE_DISABLE_IMGUI)
        for (uint32_t i = 0; i < image_count; i++) {
            auto fb = g_vkImguiFramebuffers[i];
            vkDestroyFramebuffer(g_vkDevice, fb, nullptr);
        }
        StretchyBufferFree(g_vkImguiFramebuffers);
        g_vkImguiFramebuffers = nullptr;
#endif
        StretchyBufferFree(g_vkSwapchainImageViews);
        g_vkSwapchainImageViews = nullptr;
        StretchyBufferFree(g_vkSwapchainImages);
        g_vkSwapchainImages = nullptr;
        vkDestroySwapchainKHR(g_vkDevice, oldSwapchain, nullptr);
    }

    uint32_t image_count;
    VK_CHECK(vkGetSwapchainImagesKHR(g_vkDevice, g_vkSwapchain, &image_count, nullptr));
    StretchyBufferInitWithCount(g_vkSwapchainImages, image_count);
    VK_CHECK(vkGetSwapchainImagesKHR(g_vkDevice, g_vkSwapchain, &image_count, g_vkSwapchainImages));

    // Create all additional objects for associated with the swapchain.
    for (size_t i = 0; i < image_count; i++) {
        VkImageViewCreateInfo view_info       = {};
        view_info.sType                       = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.viewType                    = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format                      = format.format;
        view_info.image                       = g_vkSwapchainImages[i];
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.layerCount = 1;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

        VkImageView image_view;
        VK_CHECK(vkCreateImageView(g_vkDevice, &view_info, nullptr, &image_view));
        StretchyBufferPush(g_vkSwapchainImageViews, image_view);

#if !defined(AUTOMATA_ENGINE_DISABLE_IMGUI)
        VkFramebufferCreateInfo ci = {};
        ci.sType                   = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        ci.renderPass              = vk_MaybeCreateImguiRenderPass();
        ci.attachmentCount         = 1;
        ci.pAttachments            = &g_vkSwapchainImageViews[i];
        ci.width                   = swapchain_size.width;
        ci.height                  = swapchain_size.height;
        ci.layers                  = 1;
        VkFramebuffer framebuffer;
        vkCreateFramebuffer(g_vkDevice, &ci, nullptr, &framebuffer);
        StretchyBufferPush(g_vkImguiFramebuffers, framebuffer);
#endif
    }

    return true;
}

void PlatformVK_init(
    VkInstance instance, VkPhysicalDevice gpu, VkDevice device, VkQueue queue, uint32_t queueIndex)
{
    g_vkQueueIndex = queueIndex;
    g_vkQueue      = queue;
    g_vkDevice     = device;
    g_vkGpu        = gpu;
    g_vkInstance   = instance;

    // get the fn pointers using Volk. the engine needs to make VK calls.
    // TODO: remove need for volk.
    if (volkInitialize()) {
        AELoggerError("Failed to initialize volk.");
        return;
    }
    volkLoadInstance(g_vkInstance);
    volkLoadDevice(g_vkDevice);

    // create the window surface
    if (instance != VK_NULL_HANDLE) {
        VkWin32SurfaceCreateInfoKHR surfaceCreateInfo = {};

        surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        surfaceCreateInfo.pNext = nullptr;
        surfaceCreateInfo.flags = 0;

        // NOTE: this function should always be called by the user post window init.
        // and therefore we can expect that hwnd/hinstance are valid.
        surfaceCreateInfo.hinstance = g_hInstance;
        surfaceCreateInfo.hwnd      = g_hwnd;

        if (g_hInstance == NULL) AELoggerError("ae::platform::VK::init called at an unexpected time");

        VK_CHECK(vkCreateWin32SurfaceKHR(instance, &surfaceCreateInfo, nullptr, &g_vkSurface));
    }

    // create the swapchain.
    auto winInfo = Platform_getWindowInfo(false);
    vk_initSwapchain(winInfo.width, winInfo.height);
}

#endif  // AUTOMATA_ENGINE_VK_BACKEND

#if defined(AUTOMATA_ENGINE_GL_BACKEND)

#if !defined(AUTOMATA_ENGINE_DISABLE_IMGUI)
#include "imgui_impl_opengl3.h"
#endif

#include <glew.h>
#include <gl/gl.h>
#include <gl/wglext.h>
// TODO(Noah): Pretty sure glew and gl must be after imgui headers here?
static HDC gHdc;
typedef BOOL WINAPI wgl_swap_interval_ext(int interval);
static wgl_swap_interval_ext *wglSwapInterval;
HGLRC glContext;

static bool CreateOpenGLContext(HWND windowHandle, HDC dc) {
    HGLRC tempContext = wglCreateContext(dc);
    if(wglMakeCurrent(dc, tempContext)) {

        // Load the extension function pointers
        PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");
        
        // Destroy the temporary context
        wglDeleteContext(tempContext);

        // Create the new OpenGL context with version 3.3
        int attribs[] = {
            WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
            WGL_CONTEXT_MINOR_VERSION_ARB, 3,
            0
        };
        if (wglCreateContextAttribsARB)
        {
            glContext = wglCreateContextAttribsARB(dc, 0, attribs);
            if (glContext)
            {
                return wglMakeCurrent(dc, glContext);
            }
        }
    }

    return false;
}

static void InitOpenGL(HWND windowHandle, HDC dc) {
    PIXELFORMATDESCRIPTOR desiredPixelFormat = {};
    desiredPixelFormat.nSize = sizeof(desiredPixelFormat);
    desiredPixelFormat.nVersion = 1;
    desiredPixelFormat.dwFlags = PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW | PFD_DOUBLEBUFFER;
    desiredPixelFormat.cColorBits = 32;
    desiredPixelFormat.cAlphaBits = 8;
    desiredPixelFormat.iLayerType = PFD_MAIN_PLANE;
    int suggestedPixelFormatIndex = ChoosePixelFormat(dc, &desiredPixelFormat);
    PIXELFORMATDESCRIPTOR suggestedPixelFormat = {};
    DescribePixelFormat(dc, suggestedPixelFormatIndex,
        sizeof(suggestedPixelFormat), &suggestedPixelFormat);
    SetPixelFormat(dc, suggestedPixelFormatIndex, &suggestedPixelFormat);
    g_engineMemory.bOpenGLInitialized = CreateOpenGLContext(windowHandle, dc);
    if(g_engineMemory.bOpenGLInitialized) {
        // TODO: vsync?
        wglSwapInterval = (wgl_swap_interval_ext *)wglGetProcAddress("wglSwapIntervalEXT");
        wglSwapInterval(0);

        // NOTE: init openGL func params for the engine side.
        glewInit();
    } else {
        //TODO(Noah): openGL did not initialize, what the heck do we do?
        // AELoggerError("OpenGL did not initialize");
        (ae::EM->pfn.fprintf_proxy(ae::platform::AE_STDERR,
            "\033[0;31m"
            "\n[error] on line=%d in file=%s\n"
            "OpenGL did not initialize"
            "\n"
            "\033[0m",
            __LINE__,
            _AUTOMATA_ENGINE_FILE_RELATIVE_));
        assert(false);
    }
}
#endif

bool ae::platform::pathExists(const char *path) {
    // TODO: ensure path is less than MAX_PATH
    DWORD dwAttrib = GetFileAttributesA((LPCSTR)path);
    return (dwAttrib != INVALID_FILE_ATTRIBUTES);
}

bool ae::platform::createDirectory(const char *dirPath) {
    // create directory
    if (CreateDirectoryA((LPCSTR)dirPath, NULL/*TODO: curr using default security descriptor*/) ||
        ERROR_ALREADY_EXISTS == GetLastError()) {
        return true;
    }
    return false;
}

#include <Knownfolders.h>
#include <shlobj_core.h>

#include <combaseapi.h>

// for multi-byte / wide string conversions.
#include <stringapiset.h>

std::string ae::platform::getAppDataPath() {
  // TODO: for prev OS support need use another call.
  // this only works for Windows 10, version 1709 and newer.
  std::string r = "";
  WCHAR *path = nullptr;
  char path2[MAX_PATH] = {};  // TODO:
  if (S_OK == SHGetKnownFolderPath(FOLDERID_RoamingAppData,
                                   0,  // no flags.
                                   NULL, (PWSTR*)&path)) {
    // TODO: WideCharToMultiByte is failing.
      if (WideCharToMultiByte(CP_ACP,  // TODO: codepage.
                            0,       // no flags.
                            path,
                            -1,  // path is null terminated.
                            (LPSTR)path2, MAX_PATH,
                            NULL,  // function uses system default char.
                            NULL   // TODO: maybe we care about this info.
                            ))
      r += std::string((const char *)path2) + '\\';
  }
  CoTaskMemFree(path);  // always (as per win32 docs).
  return r;
}

char *ae::platform::getRuntimeExeDirPath(char *pathOut, uint32_t pathSize)
{
    DWORD size = GetModuleFileNameA(nullptr, pathOut, pathSize);
    if (size == 0 || size == pathSize) {
        return nullptr;
    }
    char* lastSlash = strrchr(pathOut, L'\\');
    if (lastSlash && ((lastSlash - pathOut + 1) < pathSize)) {
        *(lastSlash + 1) = L'\0';
        return pathOut;
    }
    return nullptr;
}

void Platform_free(void *data)
{
    if (data)
        VirtualFree((void *)data, 0, MEM_RELEASE);
}

void *Platform_alloc(uint32_t bytes) {
    return VirtualAlloc(0, bytes, MEM_COMMIT, PAGE_READWRITE);
}

#include <dxgi1_4.h>
#pragma comment(lib, "dxgi.lib")

// TODO: make _these_ work for when the adapter has multiple nodes attached.
// i.e. https://learn.microsoft.com/en-us/windows/win32/direct3d12/multi-engine#:~:text=Multi%2Dadapter%20APIs,single%20IDXGIAdapter3%20object.
void Platform_getGpuInfos(ae::gpu_info_t *pInfo, uint32_t numGpus)
{
    uint32_t currGpu = 0;
    if (pInfo == nullptr) return;
    IDXGIFactory4 *dxgiFactory = nullptr;
    if (S_OK != CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory))) return;
    defer(dxgiFactory->Release());

    IDXGIAdapter1 *dxgiAdapter = nullptr;
    IDXGIAdapter3 *dxgiAdapter3 = nullptr;
    for (UINT adapterIndex = 0; S_OK == dxgiFactory->EnumAdapters1(adapterIndex, &dxgiAdapter); ++adapterIndex) {
        DXGI_ADAPTER_DESC1 desc;
        if (S_OK != dxgiAdapter->GetDesc1(&desc)) {
            dxgiAdapter->Release();
            continue;
        }
        defer(dxgiAdapter->Release());
        if(!SUCCEEDED(dxgiAdapter->QueryInterface(IID_PPV_ARGS(&dxgiAdapter3)))) {
            continue;
        }
        auto &info                = pInfo[currGpu++];
        info.adapter              = intptr_t(dxgiAdapter);
        info.dedicatedVideoMemory = desc.DedicatedVideoMemory;
        info.deviceId             = desc.DeviceId;
        info.vendorId             = desc.VendorId;
        WideCharToMultiByte(CP_ACP,  // TODO: codepage.
            0,                       // no flags.
            desc.Description,
            -1,  // path is null terminated.
            (LPSTR)info.description,
            sizeof(info.description),
            NULL,  // function uses system default char.
            NULL   // TODO: maybe we care about this info.
        );
        if(currGpu==numGpus) break; //don't go over what user said.
    }
}

void Platform_freeGpuInfos(ae::gpu_info_t *pInfo, uint32_t numGpus)
{
    // TODO: don't we need to free the string that we allocated?
    for (uint32_t i = 0; i < numGpus; i++) {
        if (pInfo[i].adapter) ((IDXGIAdapter3 *)pInfo[i].adapter)->Release();
    }
}

size_t ae::platform::getGpuCurrentMemoryUsage(intptr_t gpuAdapter)
{
    IDXGIAdapter3               *dxgiAdapter = (IDXGIAdapter3 *)gpuAdapter;
    DXGI_QUERY_VIDEO_MEMORY_INFO info        = {};
    if (S_OK == dxgiAdapter->QueryVideoMemoryInfo(
                    // TODO:
                    0,
                    DXGI_MEMORY_SEGMENT_GROUP_LOCAL,  // local is the dedicated VRAM.
                    &info)) {
        return info.CurrentUsage;
    }
    // TODO: implement error checking if above does not work.
    assert(false);
    return 0;
}

#if !defined(AUTOMATA_ENGINE_DISABLE_IMGUI)
void ScaleImGui_Impl()
{
    if (g_isImGuiInitialized) {
        HMONITOR            hMonitor = MonitorFromWindow(g_hwnd, MONITOR_DEFAULTTONEAREST);
        DEVICE_SCALE_FACTOR scaleFactor;
        if (SUCCEEDED(GetScaleFactorForMonitor(hMonitor, &scaleFactor))) {
            float SCALE = (int)scaleFactor / 100.f;
            //ImFontConfig cfg; // = {};
            float size_in_pixels = float(uint32_t(16 * SCALE));
            ImGui::GetIO().Fonts->Clear();
            ImGui::GetStyle() = ImGuiStyle();  // reset
            ImGui::GetIO().Fonts->AddFontFromFileTTF("ProggyVector Regular.ttf", size_in_pixels);
            //#if 0
            if (SCALE >= 1.f)
            {
                float adjustedScale = 1.f + (SCALE - 1.f) * 0.5f;
                ImGui::GetStyle().ScaleAllSizes(adjustedScale);
            }
            //#endif
            //ImGui::GetStyle().ScaleAllSizes(SCALE);
        }
    }
}

#if defined(AUTOMATA_ENGINE_GL_BACKEND)
void ScaleImGuiForGL()
{
    ScaleImGui_Impl();

    ImGui_ImplOpenGL3_DestroyFontsTexture();
    ImGui_ImplOpenGL3_CreateFontsTexture();
}
#endif

#if defined(AUTOMATA_ENGINE_VK_BACKEND)
void ScaleImGuiForVK(VkCommandBuffer cmd, VkCommandPool cmdPool)
{
    ScaleImGui_Impl();

    ImGui::GetIO().Fonts->Build();

    VK_CHECK(vkResetCommandBuffer(cmd, 0));
    VK_CHECK(vkResetCommandPool(g_vkDevice, cmdPool, 0));

    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(cmd, &begin_info));

    // TODO: each time we do this, it creates data that is not freed. fix that.
    ImGui_ImplVulkan_CreateFontsTexture(cmd);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkSubmitInfo end_info       = {};
    end_info.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    end_info.commandBufferCount = 1;
    end_info.pCommandBuffers    = &cmd;
    VK_CHECK(vkQueueSubmit(g_vkQueue, 1, &end_info, VK_NULL_HANDLE));

    VK_CHECK(vkDeviceWaitIdle(g_vkDevice));
    ImGui_ImplVulkan_DestroyFontUploadObjects();
}
#endif

#endif

static void Win32ResizeBackbuffer(win32_backbuffer_t *buffer, int newWidth, int newHeight)
{
	if(buffer->memory) {
		VirtualFree(buffer->memory, 0, MEM_RELEASE);
	}
	// NOTE(Noah): set bitmap heights and widths for global struct
	buffer->width = newWidth;
	buffer->height = newHeight;
	// NOTE(Noah): set values in the bitmapinfo
	buffer->info.bmiHeader.biSize = sizeof(buffer->info.bmiHeader);
	buffer->info.bmiHeader.biWidth = newWidth;
	buffer->info.bmiHeader.biHeight = -newHeight;
	buffer->info.bmiHeader.biPlanes = 1;
	buffer->info.bmiHeader.biBitCount = 32;
	buffer->info.bmiHeader.biCompression = BI_RGB;
	// NOTE(Noah): Here we are allocating memory to our bitmap since we resized it
	buffer->bytesPerPixel = 4;
	int bitmapMemorySize = newWidth * newHeight * buffer->bytesPerPixel;
	buffer->memory = VirtualAlloc(0, bitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);
	// NOTE(Noah): Setting pitch shit, ahah.
	buffer->pitch = newWidth * buffer->bytesPerPixel;
	// TODD(Noah): Probably want to clear to black each time we resize the window?
    // Or is VirtualAlloc already doing this?

    // Update g_gameMemory too
    g_gameMemory.backbufferPixels = (uint32_t *)buffer->memory;
    g_gameMemory.backbufferWidth = newWidth;
    g_gameMemory.backbufferHeight = newHeight;
}

static float Win32GetSecondsElapsed(
    LARGE_INTEGER Start, LARGE_INTEGER End, LONGLONG PerfCountFrequency64
) {
	float Result = float(End.QuadPart - Start.QuadPart) / float(PerfCountFrequency64);
	return (Result);
}

LARGE_INTEGER Win32GetWallClock()
{
	LARGE_INTEGER Result;
	QueryPerformanceCounter(&Result);
	return (Result);
}

void Win32DisplayBufferWindow(HDC deviceContext,
                              ae::game_window_info_t winInfo) {
    int OffsetX = (int(winInfo.width) - globalBackBuffer.width) / 2;
    int OffsetY = (int(winInfo.height) - globalBackBuffer.height) / 2;
    
    // TODO: Not all devices support the PatBlt function. For more information, see the description of the RC_BITBLT capability in the GetDeviceCaps function.
    
    PatBlt(deviceContext, 0, 0, winInfo.width, OffsetY, BLACKNESS);
    PatBlt(deviceContext, 0, OffsetY + globalBackBuffer.height, winInfo.height, winInfo.height, BLACKNESS);
    PatBlt(deviceContext, 0, 0, OffsetX, winInfo.height, BLACKNESS);
    PatBlt(deviceContext, OffsetX + globalBackBuffer.width, 0, winInfo.width, winInfo.height, BLACKNESS);
    //TODO(Noah): Correct aspect ratio. Also, figure out what this TODO means.
    StretchDIBits(deviceContext,
        OffsetX, OffsetY, globalBackBuffer.width, globalBackBuffer.height,
        0, 0, globalBackBuffer.width, globalBackBuffer.height,
        globalBackBuffer.memory,
        &globalBackBuffer.info,
        DIB_RGB_COLORS,
        SRCCOPY);
}

// NOTE: we use the frameIndex to decouple the input polling from the game update.
// we poll the input at a faster rate so that if the app wants it can supersample
// the game world simulation to prevent temporal aliasing artifacts.
uint32_t g_frameIndex = 0;


static void ProccessKeyboardMessage(unsigned int vkCode, bool down)
{
    ae::user_input_t &userInput = g_engineMemory.userInput;

    if (vkCode >= 'A' && vkCode <= 'Z') {
        userInput.keyDown[(uint32_t)ae::GAME_KEY_A + (vkCode - 'A')] = down;
    } else if (vkCode >= '0' && vkCode <= '9') {
        userInput.keyDown[(uint32_t)ae::GAME_KEY_0 + (vkCode - '0')] = down;
    } else {
        switch (vkCode) {
            // TODO: this looks like copy-pasta. can be this be more expressive ???
            case VK_SPACE:
                userInput.keyDown[ae::GAME_KEY_SPACE] = down;
                break;
            case VK_SHIFT:
                userInput.keyDown[ae::GAME_KEY_SHIFT] = down;
                break;
            case VK_ESCAPE:
                userInput.keyDown[ae::GAME_KEY_ESCAPE] = down;
                break;
            case VK_F5:
                userInput.keyDown[ae::GAME_KEY_F5] = down;
                break;
            case VK_TAB:
                userInput.keyDown[ae::GAME_KEY_TAB] = down;
                break;
        }
    }
}

#if !defined(AUTOMATA_ENGINE_DISABLE_IMGUI)
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

struct Win32WindowProcPayload {
    HWND     window;
    UINT     message;
    WPARAM   wParam;
    LPARAM   lParam;
    LRESULT *pResult;
    BOOL     bIsModal;
};

enum Win32ModalLoopKind {
    WIN32_MODAL_LOOP_KIND_NONE = 0,
    WIN32_MODAL_LOOP_KIND_DRAGWINDOW
};

//static std::atomic<BOOL> g_bModalRunning = FALSE;
static std::atomic<Win32ModalLoopKind> g_currModalLoopKind = WIN32_MODAL_LOOP_KIND_NONE;

DWORD WINAPI Win32WindowProcImpl(_In_ LPVOID lpParameter)
{
    Win32WindowProcPayload *payload = (Win32WindowProcPayload *)lpParameter;

    // unpack the payload.
    HWND     &window = payload->window;
    UINT     &message = payload->message;
    WPARAM   &wParam = payload->wParam;
    LPARAM   &lParam = payload->lParam;
    LRESULT  &result = *payload->pResult;

    PAINTSTRUCT ps;

    const bool bAllowInput = (g_currModalLoopKind.load() == WIN32_MODAL_LOOP_KIND_NONE);

#if !defined(AUTOMATA_ENGINE_DISABLE_IMGUI)
    if (ImGui_ImplWin32_WndProcHandler(window, message, wParam, lParam))
    {
        result = true;
        goto Win32WindowProcImpl_exit;
    }
#endif

    switch(message) {
        case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT)
        {
            SetCursor(::LoadCursorA(g_hInstance, "MonkeyDemoIconCursor"));
            return TRUE;
        }
        break;
        case WM_ENTERSIZEMOVE: {
            g_currModalLoopKind.store(WIN32_MODAL_LOOP_KIND_DRAGWINDOW);// = ;
            PostMessageA( g_userInputHwnd, message, wParam, lParam );
        } break;
        case WM_EXITSIZEMOVE:
        {
            g_currModalLoopKind.store(WIN32_MODAL_LOOP_KIND_NONE);// = ;
        } break;
        case WM_DPICHANGED: {
#if !defined(AUTOMATA_ENGINE_DISABLE_IMGUI)
#if defined(AUTOMATA_ENGINE_GL_BACKEND)
            ScaleImGuiForGL();
#endif
// TODO: for the VK case, this is kind of a concern.
// when we look at updating the imgui font, it means that we need to stall the entire device.
// but, couldn't we keep the current frame in flight any only stall the future frames?
#endif
            RECT *const prcNewWindow = (RECT *)lParam;
            SetWindowPos(window,
                NULL,
                prcNewWindow->left,
                prcNewWindow->top,
                prcNewWindow->right - prcNewWindow->left,
                prcNewWindow->bottom - prcNewWindow->top,
                SWP_NOZORDER | SWP_NOACTIVATE);
        } break;
        case WM_MOUSEMOVE:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_KEYUP:
        case WM_KEYDOWN:
            if (bAllowInput) PostMessageA(g_userInputHwnd, message, wParam, lParam);
            break;
        case WM_CREATE: {
#if defined(AUTOMATA_ENGINE_GL_BACKEND)
            gHdc = GetDC(window);
            InitOpenGL(window, gHdc);
#endif
            /*CREATESTRUCTA *cstra = (CREATESTRUCTA *)lParam;
            if(cstra != NULL) {
                // TODO(Noah): There's this pattern here where we are checking
                // if the fptr is non-null. Can we abstract this?
                if (GameHandleWindowResize != nullptr) {
                    GameHandleWindowResize(&g_gameMemory, cstra->cx, cstra->cy);
                } else {
                    AELoggerLog("WARN: GameHandleWindowResize == nullptr");
                }

            }*/
        } break;
        case WM_SIZE: {
            uint32_t width = LOWORD(lParam);
            uint32_t height = HIWORD(lParam);
            if ((GameHandleWindowResize != nullptr)
            ) {
                GameHandleWindowResize(&g_gameMemory, width, height);
            }
            Win32ResizeBackbuffer(&globalBackBuffer, width,height);
        } break;
        case WM_PAINT:
        {
            bool bRenderFallback = !g_gameMemory.getInitialized(); 
            if ( bRenderFallback )
            {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(window, &ps);
                ae::game_window_info_t winInfo = Platform_getWindowInfo(false);
                // TODO: just use rcPaint.
                Win32DisplayBufferWindow(hdc, winInfo);
                EndPaint(window, &ps);
            }
            else
            {
                result =  DefWindowProc(window, message, wParam, lParam);
            }

        } break;
        case WM_DESTROY: {
            // TODO(Noah): Handle as error?
            PostQuitMessage(0);
        } break;
        case WM_CLOSE: {
            //TODO(Noah): Handle as message to user?
            PostQuitMessage(0);
        } break;
        case WM_NCCREATE: {
            EnableNonClientDpiScaling(window);
            result =  DefWindowProc(window, message, wParam, lParam);
        } break;
        default:
        result =  DefWindowProc(window, message, wParam, lParam);
    }

    Win32WindowProcImpl_exit:

    return 0;
}

LRESULT CALLBACK Win32WindowProc_UserInput(HWND window,
  UINT message,
  WPARAM wParam,
  LPARAM lParam)
{
    ae::user_input_t &userInput = g_engineMemory.userInput;
    RAWINPUT    rawInput = {};

    constexpr UINT rawInputHeaderSize = sizeof(RAWINPUTHEADER);
    constexpr UINT rawInputSize       = sizeof(RAWINPUT);

    const bool bAllowInput = (g_currModalLoopKind.load() == WIN32_MODAL_LOOP_KIND_NONE);

    LRESULT result = 0;

    switch (message) {
        case WM_ENTERSIZEMOVE: {
            // clear all user input.
            userInput = {};
        } break;
        case WM_INPUT: {            
            GetRawInputData((HRAWINPUT)lParam, RID_INPUT, &rawInput, (PUINT)&rawInputSize, sizeof(RAWINPUTHEADER));
            auto &mouseData = rawInput.data.mouse;
            if (rawInput.header.dwType == RIM_TYPEMOUSE && bAllowInput) {
                // TODO: handle MOUSE_VIRTUAL_DESKTOP.
                bool isVirtualDesktop = !!(mouseData.usFlags & MOUSE_VIRTUAL_DESKTOP);

                if (!!(mouseData.usFlags & MOUSE_MOVE_ABSOLUTE)) {
                } else if ((mouseData.lLastX != 0) || (mouseData.lLastY != 0)) {
                                        userInput.deltaMouseX += mouseData.lLastX;
                                        userInput.deltaMouseY += mouseData.lLastY;
                }
            }
            // TODO: I don't think that this check actually matters, since we register with RIM_INPUTSINK.
            if (GET_RAWINPUT_CODE_WPARAM(wParam) == RIM_INPUT) result = DefWindowProc(window, message, wParam, lParam);
        } break;
        case WM_MOUSEMOVE: {
            int x            = (int)lParam & 0x0000FFFF;
            int y            = ((int)lParam & 0xFFFF0000) >> 16;
            userInput.mouseX = x;
            userInput.mouseY = y;
        } break;
        // left mouse button
        case WM_LBUTTONDOWN: {
            userInput.mouseLBttnDown = true;
        } break;
        case WM_LBUTTONUP: {
            userInput.mouseLBttnDown = false;
        } break;
        // right mouse button
        case WM_RBUTTONDOWN: {
            userInput.mouseRBttnDown = true;
        } break;
        case WM_RBUTTONUP: {
            userInput.mouseRBttnDown = false;
        } break;
        //keyboard messages
        case WM_KEYUP: {
        // TODO: argument conversion warning from WPARAM to unisgned int.
            ProccessKeyboardMessage(wParam, false);
        } break;
        case WM_KEYDOWN: {
            ProccessKeyboardMessage(wParam, true);
        } break;
        // NOTE: these four messages get at window create time. can ignore, but note that they are expected messages.
        case WM_GETMINMAXINFO: // sent when size is about to change.
        case WM_NCCREATE:      // sent before wm_create.
        case WM_NCCALCSIZE:    // sent when need to determine size of client area.
        case WM_CREATE: {      // sent on window create.
            result = DefWindowProc(window, message, wParam, lParam);
        } break;
        default:
#if defined(_DEBUG)
        AELoggerError("got unexpected message 0x%x in Win32WindowProc_UserInput", message);
#endif
        result = DefWindowProc(window, message, wParam, lParam);
    }
    return result;
}


LRESULT CALLBACK Win32WindowProc(HWND window,
  UINT message,
  WPARAM wParam,
  LPARAM lParam)
{
    LRESULT     result = 0;

    Win32WindowProcPayload payload = {
        .window = window, .message = message, .wParam = wParam, .lParam = lParam, .pResult = &result};
    Win32WindowProcImpl(&payload);

	return result;
}

static HMODULE g_gameCodeDLL = NULL;

static inline FILETIME Win32GetLastWriteTime(const char *FileName)
{
	FILETIME LastFileWrite = {};
	WIN32_FIND_DATA FindData; 
	HANDLE FindHandle = FindFirstFileA(FileName, &FindData);
	if (FindHandle != INVALID_HANDLE_VALUE)
	{
		FindClose(FindHandle);
		LastFileWrite = FindData.ftLastWriteTime;
	}
	return LastFileWrite;
}

static void Win32LoadGameCode(const char *SourceDLLName, const char *TempDLLName)
{	
	CopyFile(SourceDLLName, TempDLLName, FALSE); // NOTE: this is a win32 API call, although it doesn't look it.

    // NOTE: should never be override the dll thing here. where, this func is expected to be called
    // after some sort of "unloadcode" call.
    assert(g_gameCodeDLL == NULL);

	g_gameCodeDLL = LoadLibraryA(TempDLLName);
	
	if(g_gameCodeDLL)
	{
        // TODO: we could likely greatly simplify this stuff, where the game gets to decide
        // that it is going to do async stuff if it wants to. or maybe the game is ready to go
        // ASAP.
		GameInit = (ae::PFN_GameFunctionKind)GetProcAddress(g_gameCodeDLL, "GameInit");
        GamePreInit   = (ae::PFN_GameFunctionKind)GetProcAddress(g_gameCodeDLL, "GamePreInit");

        GameOnVoiceBufferEnd     = (PFN_GameOnVoiceBufferEnd)GetProcAddress(g_gameCodeDLL, "GameOnVoiceBufferEnd");
        GameOnVoiceBufferProcess =
            (PFN_GameOnVoiceBufferProcess)GetProcAddress(g_gameCodeDLL, "GameOnVoiceBufferProcess");
        GameCleanup              = (ae::PFN_GameFunctionKind)GetProcAddress(g_gameCodeDLL, "GameClose");

        // TODO: prolly don't need this. window resize happens at a predictable time.
        // we can just have HandleInput() thing and the fact that the window resized is part
        // of the input, because it's a windows message. like literally we just have a dirty flag.
        GameHandleWindowResize = (PFN_GameHandleWindowResize)GetProcAddress(g_gameCodeDLL, "GameHandleWindowResize");

        GameGetUpdateAndRender = (PFN_GameGetUpdateAndRender)GetProcAddress(g_gameCodeDLL, "GameGetUpdateAndRender");

        GameOnHotload   = (ae::PFN_GameFunctionKind)GetProcAddress(g_gameCodeDLL, "GameOnHotload");
        GameOnUnload    = (ae::PFN_GameFunctionKind)GetProcAddress(g_gameCodeDLL, "GameOnUnload");
        GameHandleInput = (ae::PFN_GameFunctionKind)GetProcAddress(g_gameCodeDLL, "GameHandleInput");
    }
}

static inline void Win32UnloadGameCode()
{
    if (g_gameCodeDLL) {
        ::FreeLibrary(g_gameCodeDLL);
        g_gameCodeDLL = NULL;
    }

    GameInit                 = NULL;
    GamePreInit              = NULL;
    GameOnVoiceBufferEnd     = NULL;
    GameOnVoiceBufferProcess = NULL;
    GameCleanup              = NULL;
    GameOnHotload            = NULL;
    GameOnUnload             = NULL;
    GameHandleWindowResize   = NULL;
    GameGetUpdateAndRender   = NULL;
    GameHandleInput          = NULL;
}

static void AssertSanePlatform(void) {
    assert(sizeof(uint8_t) == 1);
    assert(sizeof(uint16_t) == 2);
    assert(sizeof(uint32_t) == 4);
    assert(sizeof(uint64_t) == 8);
    assert(sizeof(int8_t) == 1);
    assert(sizeof(int16_t) == 2);
    assert(sizeof(int32_t) == 4);
    assert(sizeof(int64_t) == 8);
    assert(sizeof(float32_t) == 4);
    assert(sizeof(float64_t) == 8);
}

namespace automata_engine {
    class IXAudio2VoiceCallback : public ::IXAudio2VoiceCallback  {
    public:
        IXAudio2VoiceCallback() = delete;
        IXAudio2VoiceCallback(intptr_t voiceHandle) : m_voiceHandle(voiceHandle) {}
        void OnLoopEnd(void *pBufferContext) {
            AELoggerLog("voice: %d, OnLoopEnd", m_voiceHandle);
        }
        void OnBufferEnd(void *pBufferContext) {
            AELoggerLog("voice: %d, OnBufferEnd", m_voiceHandle);
        }
        void OnBufferStart(void *pBufferContext) {
            AELoggerLog("voice: %d, OnBufferStart", m_voiceHandle);
        }
        void OnStreamEnd() {
            AELoggerLog("voice: %d, OnStreamEnd", m_voiceHandle);
            if (GameOnVoiceBufferEnd)
                GameOnVoiceBufferEnd((game_memory_t *)&g_gameMemory, m_voiceHandle);
        }
        void OnVoiceError(void    *pBufferContext, HRESULT Error) {
            AELoggerLog("voice: %d, OnVoiceError", m_voiceHandle);
        }
        // these ones exec like each frame.
        void OnVoiceProcessingPassEnd() {
            //AELoggerLog("OnVoiceProcessingPassEnd");
        }
        void OnVoiceProcessingPassStart(UINT32 BytesRequired) {
            //AELoggerLog("OnVoiceProcessingPassStart");
        }
    private:
        intptr_t m_voiceHandle;
    };
}

// TODO(Noah): Do something safer with the ref to global mem here.
// i.e. what if global mem goes out of scope whilst this object is
// still alive?
class __declspec( uuid("{F5948348-445A-442C-BD86-27DD99A431B5}"))
AutomataXAPO : public ::CXAPOBase {    
public:
    AutomataXAPO() = delete; // no default constructor.
    AutomataXAPO(void *pContext, intptr_t voiceHandle) :
        m_pContext(pContext), m_voiceHandle(voiceHandle), CXAPOBase(&m_regProps) {}
    STDMETHOD(LockForProcess) (
        UINT32 InputLockedParameterCount,
        _In_reads_opt_(InputLockedParameterCount) const XAPO_LOCKFORPROCESS_BUFFER_PARAMETERS *pInputLockedParameters,
        UINT32 OutputLockedParameterCount,
        _In_reads_opt_(OutputLockedParameterCount) const XAPO_LOCKFORPROCESS_BUFFER_PARAMETERS *pOutputLockedParameters
    ) override {
        assert(!IsLocked());
        assert(InputLockedParameterCount == 1);
        assert(OutputLockedParameterCount == 1);
        assert(pInputLockedParameters != NULL);
        assert(pOutputLockedParameters != NULL);
        assert(pInputLockedParameters[0].pFormat != NULL);
        assert(pOutputLockedParameters[0].pFormat != NULL);
        HRESULT hr = CXAPOBase::LockForProcess(
            InputLockedParameterCount,
            pInputLockedParameters,
            OutputLockedParameterCount,
            pOutputLockedParameters );
        if( SUCCEEDED( hr ) )
        {
            if ( !pInputLockedParameters )
                return E_POINTER;
            memcpy( &m_wfx, pInputLockedParameters[0].pFormat, sizeof( WAVEFORMATEX ) );
            //m_uChannels = pInputLockedParameters[0].pFormat->nChannels;
            //m_uBytesPerSample = (pInputLockedParameters[0].pFormat->wBitsPerSample >> 3);
        }
        return hr;
        //return CXAPOBase::LockForProcess(InputLockedParameterCount,
         //   pInputLockedParameters, OutputLockedParameterCount, pOutputLockedParameters);
    }
    // NOTE(Noah): it is important to note XAudio2 audio data is interleaved.
    STDMETHOD_(void, Process) (
        UINT32 InputProcessParameterCount,
        _In_reads_opt_(InputProcessParameterCount) const XAPO_PROCESS_BUFFER_PARAMETERS *pInputProcessParameters,
        UINT32 OutputProcessParameterCount,
        _Inout_updates_opt_(OutputProcessParameterCount) XAPO_PROCESS_BUFFER_PARAMETERS *pOutputProcessParameters,
        BOOL IsEnabled
    ) override {
        
        // NOTE(Noah): For a lot of this stuff I have stolen from the direct-x-samples from Chuck Walbourn.
        // thanks, Chuck.
        _ASSERT( IsLocked() );
        _ASSERT( InputProcessParameterCount == 1 );
        _ASSERT( OutputProcessParameterCount == 1 );
        _ASSERT( pInputProcessParameters != nullptr && pOutputProcessParameters != nullptr);
        _Analysis_assume_( pInputProcessParameters != nullptr && pOutputProcessParameters != nullptr);
        _ASSERT( pInputProcessParameters[0].pBuffer == pOutputProcessParameters[0].pBuffer );

        UNREFERENCED_PARAMETER( OutputProcessParameterCount );
        UNREFERENCED_PARAMETER( InputProcessParameterCount );
        UNREFERENCED_PARAMETER( pOutputProcessParameters );
        UNREFERENCED_PARAMETER( IsEnabled );

        //ParameterClass* pParams;
        //pParams = (ParameterClass*)BeginProcess();
        
        if ( pInputProcessParameters[0].BufferFlags == XAPO_BUFFER_SILENT )
        {
            memset( pInputProcessParameters[0].pBuffer, 0,
                    pInputProcessParameters[0].ValidFrameCount * m_wfx.nChannels * sizeof(FLOAT32) );
        }
        else if( pInputProcessParameters[0].BufferFlags == XAPO_BUFFER_VALID )
        {
            if (GameOnVoiceBufferProcess) {
                GameOnVoiceBufferProcess((ae::game_memory_t *)m_pContext,
                    m_voiceHandle,
                    (float *)pInputProcessParameters[0].pBuffer,
                    (float *)pOutputProcessParameters[0].pBuffer,
                    pInputProcessParameters[0].ValidFrameCount,
                    m_wfx.nChannels,
                    m_wfx.wBitsPerSample >> 3);
            }
        }
        
        //EndProcess();
    }
private:
    // Registration properties defining this xAPO class.
    static XAPO_REGISTRATION_PROPERTIES m_regProps;
    //WORD m_uChannels;
    //WORD m_uBytesPerSample;
    // Format of the audio we're processing
    WAVEFORMATEX m_wfx = {};
    void *m_pContext;
    intptr_t m_voiceHandle;
};

typedef struct {
    IXAudio2SourceVoice *voice;
    ae::IXAudio2VoiceCallback *callback;
    AutomataXAPO *xapo;
} win32_voice_t;

static XAUDIO2_BUFFER g_xa2Buffer = {0};

/// stretchy buffer.
static win32_voice_t* g_ppSourceVoices = nullptr;

// Xaudio2 callbacks.

__declspec(selectany) XAPO_REGISTRATION_PROPERTIES AutomataXAPO::m_regProps = {
    __uuidof(AutomataXAPO),
    L"automata_engine::XAPO",
    L"Copyright (c) 2022 Hmnxty Studios. All rights reserved.",
    1, 0, XAPO_FLAG_INPLACE_REQUIRED
        | XAPO_FLAG_CHANNELS_MUST_MATCH
        | XAPO_FLAG_FRAMERATE_MUST_MATCH
        | XAPO_FLAG_BITSPERSAMPLE_MUST_MATCH
        | XAPO_FLAG_BUFFERCOUNT_MUST_MATCH
        | XAPO_FLAG_INPLACE_SUPPORTED, 1, 1, 1, 1
};

// TODO(Noah): What happens if there is no buffer to play?? -_-
void Platform_voicePlayBuffer(intptr_t voiceHandle) {
    auto pSourceVoice = (IXAudio2SourceVoice *)g_ppSourceVoices[voiceHandle].voice;
    if (pSourceVoice != nullptr) {
        pSourceVoice->Start( 0 );
    }
}

void automata_engine::platform::voiceStopBuffer(intptr_t voiceHandle) {
    auto pSourceVoice = (IXAudio2SourceVoice *)g_ppSourceVoices[voiceHandle].voice;
    if (pSourceVoice != nullptr) {
        pSourceVoice->Stop( 0 );
    }
}

void automata_engine::platform::voiceSetBufferVolume(intptr_t voiceHandle, float volume) {
    auto pSourceVoice = (IXAudio2SourceVoice *)g_ppSourceVoices[voiceHandle].voice;
    if (pSourceVoice != nullptr) {
        pSourceVoice->SetVolume( volume );
    }
}

float automata_engine::platform::decibelsToAmplitudeRatio(float db) {
    return XAudio2DecibelsToAmplitudeRatio(db);
}

constexpr WAVEFORMATEX initGlobalWaveFormat() {
    WAVEFORMATEX waveFormat = {};
    waveFormat.wFormatTag = WAVE_FORMAT_PCM; // going with 2-channel PCM data.
    waveFormat.nChannels = 2;
    waveFormat.nSamplesPerSec = ae::io::ENGINE_DESIRED_SAMPLES_PER_SECOND; // 48 kHz
    waveFormat.wBitsPerSample = sizeof(short) * 8;
    waveFormat.nBlockAlign = waveFormat.nChannels * waveFormat.wBitsPerSample / 8;
    waveFormat.nAvgBytesPerSec = waveFormat.nBlockAlign * waveFormat.nSamplesPerSec;
    waveFormat.cbSize = 0;
    return waveFormat;
}
constexpr WAVEFORMATEX  g_waveFormat = initGlobalWaveFormat();

static IXAudio2* g_pXAudio2 = nullptr;

// TODO(Noah): We should prob figure something intelligent
// when voice creation fails.
intptr_t Platform_createVoice() {

    uint32_t newVoiceIdx = StretchyBufferCount(g_ppSourceVoices);
    IXAudio2SourceVoice* newVoice = nullptr;
    
    auto voiceCallback = new ae::IXAudio2VoiceCallback(newVoiceIdx); 
    auto atoXAPO = new AutomataXAPO(&g_gameMemory, newVoiceIdx);
    // TODO(Noah): Handle case where XAPO init fails.
    HRESULT hr = atoXAPO->Initialize(nullptr, 0);

    if (!FAILED(hr)) {
        g_pXAudio2->CreateSourceVoice(&newVoice, (WAVEFORMATEX*)&g_waveFormat, 0, 
        XAUDIO2_DEFAULT_FREQ_RATIO, voiceCallback, NULL, NULL);

        if (newVoice) {
            // create effect chain for DSP on voice.
            XAUDIO2_EFFECT_DESCRIPTOR xapoDesc[1] = {};
            xapoDesc[0].pEffect = static_cast<IXAPO *>(atoXAPO);
            xapoDesc[0].InitialState = true;  // default enable.
            xapoDesc[0].OutputChannels = 2;
            XAUDIO2_EFFECT_CHAIN chain = {};
            chain.EffectCount = 1;
            chain.pEffectDescriptors = xapoDesc;
            
            HRESULT hResult;
            if (S_OK == (hResult = newVoice->SetEffectChain(&chain))) {
                // atoXAPO->Release();
            } else {
                AELoggerError("newVoice->SetEffectChain() returned with code (0x%x)", hResult);
            }

    #if defined(_DEBUG)
            // small sanity check.
            float amplitude = XAudio2DecibelsToAmplitudeRatio(0.f);
            assert(amplitude == 1.f);
    #endif
        }
    } else {
        AELoggerError("atoXAPO->Initialize() returned with code (0x%x)", hr);
    }

    win32_voice_t w32NewVoice = {newVoice, voiceCallback, atoXAPO};
    StretchyBufferPush(g_ppSourceVoices, w32NewVoice);
    return (intptr_t)newVoiceIdx;
}

// TODO(Noah): Make our platform audio things more low-level.
// I don't like the fact that we are using loaded_wav here.
// What if I want to submit a full-blown buffer? maybe I generated
// programatically or something.
//
// Another TODO would be ... make this NEVER fail! WHY would I
// voluntarily release that sort of paradigm over to the game?
// It's my job to put Xaudio in check. Why would you fail? Like ...
// nah man. None of that shit!
//
// the paradigm should be like so. If things fail that should normally
// work no problem, it means that something is catasrophically wrong.
// Like idk, the sound card was removed? In these cases,
// our engine should handle it entirely. Game, when making requests,
// will have failures happen silently. This model is fine because
// it will not be too long after that request failure that the engine
// shuts itself down, or does SOMETHING.
//
// Now, there are also things that WILL fail. Like file reads and shader
// compilation. i.e. file might not exist and shader could have syntax errs.
// in these cases, the game has to deal with such errors.
static bool Platform_voiceSubmitBuffer2(intptr_t voiceHandle, void *data, uint32_t size, bool shouldLoop) {
    auto pSourceVoice = (IXAudio2SourceVoice *)g_ppSourceVoices[voiceHandle].voice;
    if (pSourceVoice != nullptr) {
        if (FAILED(pSourceVoice->Stop(0))) { return false; }
        if (FAILED(pSourceVoice->FlushSourceBuffers())) { return false; }
    } else {
        return false; // no source voice...
    }
    g_xa2Buffer.AudioBytes = size;//myWav.sampleCount * myWav.channels * sizeof(short);
    g_xa2Buffer.pAudioData = (const BYTE *)data;//myWav.sampleData;
    if (shouldLoop) {
        g_xa2Buffer.LoopCount = XAUDIO2_LOOP_INFINITE;
    } else {
        g_xa2Buffer.LoopCount = 0;
    }
    // NOTE: The pBuffer pointer can be reused or freed immediately after calling this method
    // so the concern of, "do we need to keep this pointer live"?
    // is a non-issue.
    return !FAILED(pSourceVoice->SubmitSourceBuffer(&g_xa2Buffer));
}

bool automata_engine::platform::voiceSubmitBuffer(intptr_t voiceHandle, void *data, uint32_t size, bool shouldLoop) {
    return Platform_voiceSubmitBuffer2(voiceHandle, data, size, shouldLoop);
}

static bool Platform_voiceSubmitBuffer(intptr_t voiceHandle, ae::loaded_wav_t wavFile) {
    return Platform_voiceSubmitBuffer2(voiceHandle, 
        wavFile.sampleData, 
        wavFile.sampleCount * wavFile.channels * sizeof(short), false);
}


#include <thread>

void ae::platform::showWindowAlert(const char *windowTitle, const char *windowMessage, bool bAsync) {
    auto job=[=]{MessageBoxA(NULL, windowMessage, windowTitle, MB_OK);};
    if (bAsync) {
        std::thread( job ).detach();
    } else {
        MessageBoxA(g_hwnd ? g_hwnd : NULL, windowMessage, windowTitle, MB_OK);
    }
}

uint64_t Platform_getTimerFrequency() { return g_PerfCountFrequency64; }

uint64_t Platform_wallClock() {
    LARGE_INTEGER counter = Win32GetWallClock();
    return counter.QuadPart;
}

void Platform_fprintf_proxy(int h, const char *fmt, ...)
{
    static std::mutex mut={};
    // only one person printing at a time. very important...
    std::lock_guard<std::mutex> lock(mut);

    constexpr auto maxSize = 4096;
    char           _buf[maxSize];

    va_list args;
    va_start(args, fmt);
    auto written = 1 + vsnprintf(_buf, maxSize, fmt, args);
    va_end(args);

    // TODO: in cases like this, alloc dynamic buffer to print with.
    assert(written != maxSize);

    HANDLE handle;
    switch (h) {
        case ae::platform::AE_STDERR:
            handle = GetStdHandle(STD_ERROR_HANDLE);
            break;
        case ae::platform::AE_STDOUT:
            handle = GetStdHandle(STD_OUTPUT_HANDLE);
            break;
        default:
            handle = GetStdHandle(STD_OUTPUT_HANDLE);
    }

    //LARGE_INTEGER begin = Win32GetWallClock();

    // NOTE: here we do this cool workaround where we send keyboard input to the console
    // window. we do this so that we can clear any sort of state where we are pending for
    // input. if we enter in this state somehow (e.g. the user clicks anywhere in the console,
    // or marks a region of the console), any call to WriteFile will block until that get input
    // operation is complete.
    {
        // Send the WM_KEYDOWN and WM_KEYUP messages to the console window
        SendMessage(g_consoleHwnd, WM_KEYDOWN, 'C', 0);
        SendMessage(g_consoleHwnd, WM_KEYUP, 'C', 0);
    }

    WriteConsoleA(handle, (void *)_buf, strlen(_buf), NULL, NULL);

    //LARGE_INTEGER after = Win32GetWallClock();

    /*float timeElapsed = Win32GetSecondsElapsed(begin, after, g_PerfCountFrequency64);

    // print timeElapsed, but don't do it recursively.
    {
        auto written = 1 + snprintf(_buf, maxSize, "WriteConsoleA timeElapsed: %.3f", timeElapsed);
        HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
        WriteConsoleA(handle, (void *)_buf, strlen(_buf), NULL, NULL);
    }*/


    if (g_redirectedFprintf) { g_redirectedFprintf(_buf); }
}

HWINEVENTHOOK g_windowEventHookProc = {};
void          Wineventproc(HWINEVENTHOOK hWinEventHook,
             DWORD                       event,
             HWND                        hwnd,
             LONG                        idObject,
             LONG                        idChild,
             DWORD                       idEventThread,
             DWORD                       dwmsEventTime)
{
    if (hwnd != g_hwnd) {
        // TODO: it would be cool to have differnt print contexts. Print from either the game or the engine.
        //AELoggerLog("focus leaving _this_ window.");
        g_bIsWindowFocused = false;
    } else if (hwnd == g_hwnd) {
        //AELoggerLog("focus entering _this_ window.");
        g_bIsWindowFocused = true;
    }
}

// wait until this "slice" of time has reached some amount of wallclock time.
bool Win32SliceWait(bool SleepGranular, LARGE_INTEGER SliceBegin, float endFrameTarget, const char *warnMsg)
{
    float SecondsElapsedForFrame = Win32GetSecondsElapsed(SliceBegin, Win32GetWallClock(), g_PerfCountFrequency64);
    if (SecondsElapsedForFrame < endFrameTarget) {
        if (SleepGranular) { // TODO: revisit this "SleepGranular" stuff.
            DWORD SleepMS =
                (DWORD)ae::math::max(int32_t(0), int32_t(1000.0f * (endFrameTarget - SecondsElapsedForFrame)) - 1);
            if (SleepMS > 0) { Sleep(SleepMS); }
        }
        while ((SecondsElapsedForFrame < endFrameTarget)) {
            SecondsElapsedForFrame = Win32GetSecondsElapsed(SliceBegin, Win32GetWallClock(), g_PerfCountFrequency64);
        }
    } else {
        AELoggerWarn("missed Win32SliceWait by %f ms with warnMsg: %s", (SecondsElapsedForFrame - endFrameTarget) * 1000.f, warnMsg);
        return true;
    }
    return false;
}

const char  *g_SourceDLLName         = AUTOMATA_ENGINE_PROJECT_NAME ".dll";
const char  *g_TempDLLName           = AUTOMATA_ENGINE_PROJECT_NAME "_temp.dll";
FILETIME     g_gameCodeLastWriteTime = {};
bool         g_SleepGranular         = false;


DWORD WINAPI Win32GameUpdateAndRenderHandlingLoop(_In_ LPVOID lpParameter) {
    IDXGIOutput   *gameMonitor = nullptr;
    IDXGIAdapter1 *gameAdapter   = nullptr;

    defer(gameMonitor ? (void)gameMonitor->Release() : (void)0);
    defer(gameAdapter ? (void)gameAdapter->Release() : (void)0);

    // TODO: consider multiple monitor setups.
    // TODO: consdier multiple GPU(adapter) setups.
    // get information about the monitor connected.
    {
        IDXGIFactory4 *dxgiFactory = nullptr;
        if (S_OK != CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory))) {
            AELoggerError("could not create factory");
            ExitThread(-1);
        }
        defer(dxgiFactory->Release());
        dxgiFactory->EnumAdapters1(0, &gameAdapter);
        HRESULT result = gameAdapter->EnumOutputs(0, &gameMonitor);
        if (result != S_OK) {
            AELoggerError("could not enumerate DXGI output 0");
            ExitThread(-1);
        }
    }

    // get the monitor refresh rate
    int MonitorRefreshRateHz = 60; // guess.
    {
#if 1
        HDC RefreshDC = GetDC(g_hwnd);

        int Win32RefreshRate = GetDeviceCaps(RefreshDC, VREFRESH);
        if (Win32RefreshRate > 1) {
            MonitorRefreshRateHz = Win32RefreshRate;
        } else {
            AELoggerError("failed to query monitor refresh, assuming 60 Hz");
        }
        ReleaseDC(g_hwnd, RefreshDC);
#endif
    }

    std::atomic<bool> &globalRunning = g_engineMemory.globalRunning;

    LARGE_INTEGER LastCounter = Win32GetWallClock();
    
    while (globalRunning.load()) {

        // TODO: could this have better placement in the frame?
        FILETIME NewDLLWriteTime = Win32GetLastWriteTime(g_SourceDLLName);
        if (CompareFileTime(&NewDLLWriteTime, &g_gameCodeLastWriteTime)) {
            if (GameOnUnload) GameOnUnload(&g_gameMemory);
            Win32UnloadGameCode();
            g_gameCodeLastWriteTime = Win32GetLastWriteTime(g_SourceDLLName);
            Win32LoadGameCode(g_SourceDLLName, g_TempDLLName);
            if (GameOnHotload) GameOnHotload(&g_gameMemory);
            AELoggerLog("did the hotload.");
        }

        g_frameIndex++;

        bool bRenderFallback = !g_gameMemory.getInitialized();

        bool bRenderImGui              = g_engineMemory.g_renderImGui.load();
        g_engineMemory.bCanRenderImGui = bRenderImGui;

#if !defined(AUTOMATA_ENGINE_DISABLE_IMGUI)
        if (bRenderImGui && g_isImGuiInitialized && !bRenderFallback) {
#if defined(AUTOMATA_ENGINE_GL_BACKEND)
            ImGui_ImplOpenGL3_NewFrame();
#endif
#if defined(AUTOMATA_ENGINE_VK_BACKEND)
            ImGui_ImplVulkan_NewFrame();
#endif
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();
        }
#endif

        {
            bool bFoundUpdate = false;
            if (GameGetUpdateAndRender) {
                auto gameUpdateAndRender = GameGetUpdateAndRender(&g_gameMemory);
                if ((gameUpdateAndRender != nullptr)) {
                    bFoundUpdate = true;
                    gameUpdateAndRender(&g_gameMemory);
                }
            }
            if (!bFoundUpdate) AELoggerWarn("gameUpdateAndRender == nullptr");
        }

        // TODO: maybe imgui even works in the fallback mode?
#if !defined(AUTOMATA_ENGINE_DISABLE_IMGUI)
        if (bRenderImGui && g_isImGuiInitialized && !bRenderFallback) {
#if defined(AUTOMATA_ENGINE_GL_BACKEND)
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
#endif
#if defined(AUTOMATA_ENGINE_VK_BACKEND)
            // NOTE: app responsible to record imgui into its own command buffers.
#endif
        }

#endif

        ae::engine_memory_t *EM = &g_engineMemory;

        LARGE_INTEGER WorkCounter         = Win32GetWallClock();
        EM->timing.lastFrameUpdateEndTime = WorkCounter.QuadPart;

        assert(EM->g_updateModel == ae::AUTOMATA_ENGINE_UPDATE_MODEL_ATOMIC);
        bool bUsingAtomicUpdate = true;

#if defined(AUTOMATA_ENGINE_GL_BACKEND)
        if (!bRenderFallback) {
            if (bUsingAtomicUpdate) {
                glFlush();   // push all buffered commands to GPU
                glFinish();  // block until GPU is complete
                EM->timing.lastFrameGpuEndTime = Win32GetWallClock().QuadPart;
            }
            // TODO: we might want to rethink how we do vsync on the GL side. there is some major oddness with
            // the double wait idea that we are doing here.
            SwapBuffers(gHdc);
        }
#endif

        // NOTE: we know this number through observation. this is an upper bound. this number includes cpu update time, gpu work + present
        // via mailbox mode (does not include vblank, ends after the call to vkQueuePresentKHR returns).
        //
        // TODO: both these constants are adhoc. what if the monitor has a different refresh rate?
        // what if the round trip frame time is different?
        static constexpr float GuaranteedFrameTime          = 0.013f;
        const float TargetSecondsElapsedPerFrame = 1.f / float(MonitorRefreshRateHz);

        float endFrameTarget         = TargetSecondsElapsedPerFrame;
        bool  doEndFrameWaitToTarget = true;

        // TODO: consider other update models.
        EM->timing.lastFrameVisibleTime = TargetSecondsElapsedPerFrame;

#if defined(AUTOMATA_ENGINE_VK_BACKEND)
        if (!bRenderFallback) {
            LARGE_INTEGER gpuEnd;  // TODO: consider other update models.

            if (bUsingAtomicUpdate) {
                gpuEnd                             = vk_WaitForAndResetFence(g_vkDevice, &g_vkPresentFence);
                ae::EM->timing.lastFrameGpuEndTime = gpuEnd.QuadPart;
            }

            // TODO: looks like the vkqueue present KHR / vkgetnextbackbuffer are taking extra long some frames
            // and causing us to miss the vblank?

            {
                uint32_t swapchainCount = 1;

                VkPresentInfoKHR present = {};
                present.sType            = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
                present.swapchainCount   = swapchainCount;
                present.pSwapchains      = &g_vkSwapchain;
                present.pImageIndices    = &g_vkCurrentImageIndex;

                vkQueuePresentKHR(g_vkQueue, &present);
            }

            vk_getNextBackbuffer();
        }
#endif

        if (bRenderFallback) {
            // NOTE(Noah): Here we are going to call our custom windows platform layer function that
            // will write our custom buffer to the screen.
            HDC                    deviceContext = GetDC(g_hwnd);
            ae::game_window_info_t winInfo       = Platform_getWindowInfo(false);
            Win32DisplayBufferWindow(deviceContext, winInfo);
            ReleaseDC(g_hwnd, deviceContext);
        }

        {
            LARGE_INTEGER beforeVblankCall = Win32GetWallClock();

            // TODO: need to handle WM_DISPLAYCHANGE, else the HMONITOR that we are relying on here may simply become
            // invalid. its entirely possible for the user to hotswap the monitor.
            //
            // its also the case that we generally need to handle multiple monitor setups. there can be two monitors that
            // have different refresh rates and our window is spanning across both of them.
            // in such a case, our app should wait on the vblank of the monitor with the lower refresh rate.
            gameMonitor->WaitForVBlank();

            LARGE_INTEGER after  = Win32GetWallClock();
            LARGE_INTEGER before = {.QuadPart = LONGLONG(EM->timing.lastFrameMaybeVblankTime)};

            // is this measured vblank greater than forecasted time based on last blank?
            float fromLastVblank = Win32GetSecondsElapsed(before, after, g_PerfCountFrequency64);
            if ((TargetSecondsElapsedPerFrame * 1.5f) < fromLastVblank) {

                float fromLastAndBeforeWait    = Win32GetSecondsElapsed(before, beforeVblankCall, g_PerfCountFrequency64);
                float fromLastAndBeforePresent = Win32GetSecondsElapsed(before, {.QuadPart = LONGLONG(ae::EM->timing.lastFrameGpuEndTime)}, g_PerfCountFrequency64);

#if defined(_DEBUG)
                AELoggerWarn(
                    "missed the vertical blank"
                    ". elapsed times:"
                    "\n\tfrom last vblank until just before presenting this frame: %f"
                    "\n\tfrom last vblank until after present and before vblank wait: %f"
                    "\n\telapsed time from last vblank until now: %f",
                    fromLastAndBeforePresent,
                    fromLastAndBeforeWait,
                    fromLastVblank
                );
#else
                AELoggerWarn("missed the vertical blank");                
#endif
                // TODO: putting this line here seems to cause the monkey to stop spinning entirely. what the fuck is going on?
                // is this a compiler bug? this value is read correctly during the ImGui update. so what gives?
                //EM->timing.lastFrameVisibleTime = fromLastVblank;
            }

            EM->timing.lastFrameMaybeVblankTime = after.QuadPart;

            // do the frame pacing stuff.
            doEndFrameWaitToTarget = true;

            // wait until the next input poll.
            LastCounter = Win32GetWallClock();  // begin wait from this point.

            // wait after vblank for the "frame pacing".
            endFrameTarget = (TargetSecondsElapsedPerFrame - GuaranteedFrameTime);
        }

        if (doEndFrameWaitToTarget) {
            Win32SliceWait(g_SleepGranular, LastCounter, endFrameTarget, "missed frame target");
        }

        // EM->timing.thisFrameBeginTime = 0;  // TODO.

        LARGE_INTEGER EndCounter = Win32GetWallClock();
        LastCounter              = EndCounter;

        EM->timing.lastFrameBeginTime = EM->timing.thisFrameBeginTime;
        EM->timing.thisFrameBeginTime            = EndCounter.QuadPart;

    }  // while(globalrunning)

    // global running is false, quit the main loop.
    PostMessageA(g_hwnd, WM_QUIT, 0, 0);

    ExitThread(0);
}

DWORD WINAPI Win32InputHandlingLoop(_In_ LPVOID lpParameter) {

    // in order to recieve messages, this thread needs a queue, and therefore
    // a window.
    //
    // this will be a hidden window.

    // a disabled window recieves no keyboard/mouse input from user but can still get messages from other windows.
    // a hidden window is simply not drawn. a hidden window can still process messages from other windows.
    // a message only window may only recieve or send messages.

    WNDCLASSA wc     = {};
    wc.lpfnWndProc   = Win32WindowProc_UserInput;  // Set callback
    wc.hInstance     = g_hInstance;
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor       = LoadCursor(0, IDC_ARROW);
    wc.lpszClassName = "Win32InputHandlingLoop";

    ATOM classAtom = RegisterClassA(&wc);
    if(classAtom == 0) {
        AELoggerError("Unable to register window class \"%s\"", wc.lpszClassName);
        ExitThread(-1);
    }
    
    defer((classAtom != 0) ? (void)UnregisterClassA(wc.lpszClassName, g_hInstance) : (void)0);

    const DWORD windowStyle = WS_DISABLED & (~WS_VISIBLE);

    g_userInputHwnd = CreateWindowExA(
            0, // dwExStyle
            wc.lpszClassName,
            "Win32InputHandlingLoop",
            windowStyle,
            CW_USEDEFAULT, // init X
            CW_USEDEFAULT, // init Y
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            HWND_MESSAGE, // message only window.
            NULL, // menu
            g_hInstance,
            NULL // structure to be passed to WM_CREATE message
        );

    // Register mouse for raw input capture
    RAWINPUTDEVICE Rid[1];
    Rid[0].usUsagePage = HID_USAGE_PAGE_GENERIC;
    Rid[0].usUsage     = HID_USAGE_GENERIC_MOUSE;
    Rid[0].dwFlags     = RIDEV_INPUTSINK;  // still get input when not in foreground.
    Rid[0].hwndTarget  = g_userInputHwnd;
    RegisterRawInputDevices(Rid, 1, sizeof(Rid[0]));
    // TODO: unregister?

    SetEvent(g_inputThreadEvent); // signal to the main thread that it can begin the main message loop.        

    // TODO: this thread runs from the main thread, i.e. the thread that created the window.
    //
    // which means that our game update+render needs to be on a different thread.
    //
    // for OpenGL we are going to need to throw the following in the Hotload thing.
    // BOOL wglMakeCurrent(HDC unnamedParam1, HGLRC unnamedParam2);
    // and we could have some em->gl_pfn.getContext to get the HGLRC;

    LARGE_INTEGER SliceBegin = Win32GetWallClock();

    auto             &globalRunning = g_engineMemory.globalRunning;
    ae::user_input_t &userInput     = g_engineMemory.userInput;

    // TODO: expose that the game can modify the poll rate.
    constexpr uint32_t inputPollRate = 500;
    constexpr float endFrameTarget = 1.f / float(inputPollRate);

    // NOTE: the idea behind using the std::atomic<T> data type is to ensure that we have atomic loads and stores to
    // this global running variable. multiple threads are reading and writing. we don't want to observe a half-complete
    // operation.
    while (globalRunning.load()) {

        MSG message;

        // reset accumulation state.
        userInput.deltaMouseX = 0;
        userInput.deltaMouseY = 0;

        // NOTE: this means there may have been more messages to Peek. yet, we did no such peeking and exited early
        // because or "time was up".
        bool earlyExit = false;

        float estimatedPeekMessageHandlingTime = 1.f/1000.f;

        LARGE_INTEGER beforeTranslate = {};
        LARGE_INTEGER beforeDispatch = {};
        LARGE_INTEGER afterDispatch = {};
        UINT lastMessage = 0;

        while (PeekMessage(&message, NULL, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) {

                // NOTE: in this case, this input thread will stop running.
                // if this message is recieved before the 60hz sample point,
                // then we won't run the next frame, and everything will quit pretty much immediately.
                //
                // however, this setting of global running to false might happen
                // during the update+render loop. in this case, the game would continue
                // to render that frame. but the next frame won't begin and the game will
                // stop before then.
                //
                // this is OK afaik.
                //
                // the game won't know while its rendering that its going to quit next frame.
                // and that's OK. it has the chance to exit "gracefully" via GameClose.
                //
                // there is the idea of if the user presses the X button at the top right to close
                // window, that maybe the game keep rendering and its like, "do you really want to quit?".
                //
                // in this case, TODO: maybe we have a thing in the pre-init where the game can request
                // to be "part of this process". in this case, we'd keep calling the update+render loop
                // even after WM_QUIT.

                globalRunning.store(false);
                break;
            }

            beforeTranslate = Win32GetWallClock();

            lastMessage = message.message;

            beforeDispatch = Win32GetWallClock();

            DispatchMessage(&message);

            afterDispatch =  Win32GetWallClock();

            // check if we have gathered messages for enough time.
            float SecondsElapsedForFrame = Win32GetSecondsElapsed(SliceBegin, Win32GetWallClock(), g_PerfCountFrequency64);
            if (earlyExit = (SecondsElapsedForFrame + estimatedPeekMessageHandlingTime > endFrameTarget))
                break;
        }

        // wait until this slice is "done".
        bool didMiss = Win32SliceWait(g_SleepGranular, SliceBegin, endFrameTarget,
            "missed input poll target"
        );

#if defined(_DEBUG)
        if (didMiss)
        {
            LARGE_INTEGER now = Win32GetWallClock();
            AELoggerWarn(
                "missed input poll target extra info:"
                "\t\nelapsed time from before translate: %.3f s"
                "\t\nelapsed time from before dispatch: %.3f s"
                "\t\nelapsed time from after dispatch: %.3f s"
                "\t\nlast message code: 0x%x",
                Win32GetSecondsElapsed(beforeTranslate, now, g_PerfCountFrequency64),
                Win32GetSecondsElapsed(beforeDispatch, now, g_PerfCountFrequency64),
                Win32GetSecondsElapsed(afterDispatch, now, g_PerfCountFrequency64),
                lastMessage);
        }
#endif

        LARGE_INTEGER ThisTimer  = Win32GetWallClock();

        // NOTE: the packet time live indicates how long the state should apply.
        if (!didMiss)
        {
            userInput.packetLiveTime = endFrameTarget;
        }
        else
        {
            userInput.packetLiveTime = Win32GetSecondsElapsed(SliceBegin, Win32GetWallClock(), g_PerfCountFrequency64);
        }

        // iter the timer.
        SliceBegin = ThisTimer;

        // NOTE: there is a concern where we could be running the main render loop, set globalRunning to false,
        // but in this loop, we do the load JUST before that. so, we miss it and read the "wrong value".
        //
        // however, this concern is a non-issue. the guarantee of the engine is only that we won't call another
        // update and render. it's not an "exit immediately". therefore, to have a potentially extra HandleInput call
        // get made isn't an issue.

        if (globalRunning.load()) {
            // allowed to call the handle input code.
            if (GameHandleInput) GameHandleInput(&g_gameMemory);
        }
    }

        ExitThread(0);

}

int CALLBACK WinMain(HINSTANCE instance,
  HINSTANCE prevInstance,
  LPSTR cmdLine,
  int showCode)
{
    AssertSanePlatform();

    // setup the engine memory PFNs + context.
    // NOTE: no need to do the below this is clear proper with the decl.
    // g_engineMemory = {};

    ae::EM                          = &g_engineMemory;
    ae::EM->pfn.getWindowInfo       = Platform_getWindowInfo;
    ae::EM->pfn.fprintf_proxy       = Platform_fprintf_proxy;
    ae::EM->pfn.setMousePos         = Platform_setMousePos;
    ae::EM->pfn.showMouse           = Platform_showMouse;
    ae::EM->pfn.getTimerFrequency   = Platform_getTimerFrequency;
    ae::EM->pfn.wallClock           = Platform_wallClock;
    ae::EM->pfn.free                = Platform_free;
    ae::EM->pfn.alloc               = Platform_alloc;
    ae::EM->pfn.readEntireFile      = Platform_readEntireFile;
    ae::EM->pfn.writeEntireFile     = Platform_writeEntireFile;
    ae::EM->pfn.freeLoadedFile      = Platform_freeLoadedFile;
    ae::EM->pfn.setAdditionalLogger = Platform_setAdditionalLogger;
    ae::EM->pfn.voicePlayBuffer     = Platform_voicePlayBuffer;
    ae::EM->pfn.voiceSubmitBuffer   = Platform_voiceSubmitBuffer;
    ae::EM->pfn.createVoice         = Platform_createVoice;
    ae::EM->pfn.getGpuInfos         = Platform_getGpuInfos;
    ae::EM->pfn.freeGpuInfos        = Platform_freeGpuInfos;

#if !defined(AUTOMATA_ENGINE_DISABLE_IMGUI)
    ae::EM->pfn.imguiGetCurrentContext     = Platform_imguiGetCurrentContext;
    ae::EM->pfn.imguiGetAllocatorFunctions = Platform_imguiGetAllocatorFunctions;
#endif

#if defined(AUTOMATA_ENGINE_VK_BACKEND)
#if !defined(AUTOMATA_ENGINE_DISABLE_IMGUI)
    ae::EM->vk_pfn.renderAndRecordImGui = PlatformVK_renderAndRecordImGui;
#endif
    ae::EM->vk_pfn.getSwapchainFormat   = PlatformVK_getSwapchainFormat;
    ae::EM->vk_pfn.getCurrentBackbuffer = PlatformVK_getCurrentBackbuffer;
    ae::EM->vk_pfn.getFrameEndFence     = PlatformVK_getFrameEndFence;
    ae::EM->vk_pfn.init                 = PlatformVK_init;
#endif

    UINT DesiredSchedularGranularity = 1;
	g_SleepGranular = (timeBeginPeriod(DesiredSchedularGranularity) == TIMERR_NOERROR);

    // NOTE(Noah): There is a reason that when you read C code, all the variables are defined
    // at the top of the function. I'm just guessing here, but maybe back in the day people
    // liked to use "goto" a lot. It's a dangerous play because you might skip over the initialization
    // of a variable. Then the contents are unknown!
    ATOM classAtom = 0;
    HWND windowHandle = NULL;
    HRESULT comResult = S_FALSE;
    WNDCLASSA windowClass = {};

    // Before doing ANYTHING, we alloc memory.
    g_gameMemory.pEngineMemory = &g_engineMemory;
    g_gameMemory.setInitialized(false);
    g_gameMemory.dataBytes = 67108864; // will allocate 64 MB
    g_gameMemory.data = Platform_alloc(g_gameMemory.dataBytes); 

    if (g_gameMemory.data == nullptr) {
        AELoggerError("unable to allocate the %u bytes required to run the game", g_gameMemory.dataBytes);
        return -1;
    }

    // register event hook for checking when window is focused or not.
    g_windowEventHookProc = SetWinEventHook(
    // [eventMin, eventMax]
// NOTE: these two work only for the OLD windows alt+tab thing.
// you need to hold LEFT_ALT, button press RIGHT_ALT, then finally while still holding LEFT_ALT tap TAB.
#if 0
        EVENT_SYSTEM_SWITCHSTART,
        EVENT_SYSTEM_SWITCHEND,
#endif
        EVENT_SYSTEM_FOREGROUND,
        EVENT_SYSTEM_FOREGROUND,

        NULL,  // hook function is not located in a DLL.
        Wineventproc,
        0,  // recieve events from all process on desktop.
        0,  // associated with     all threads on desktop.
        WINEVENT_OUTOFCONTEXT);
    defer(UnhookWinEvent(g_windowEventHookProc));

    // Create a console.
#if !defined(AUTOMATA_ENGINE_DISABLE_PLATFORM_LOGGING)
    {
        int hConHandle;
        //intptr_t lStdHandle;
        CONSOLE_SCREEN_BUFFER_INFO coninfo;
        FILE *fp;

        // allocate a console for this app
        // https://docs.microsoft.com/en-us/windows/console/allocconsole
        AllocConsole();
        SetConsoleTitleA(AUTOMATA_ENGINE_NAME_STRING " Console");

        // set the screen buffer to be big enough to let us scroll text
        // https://docs.microsoft.com/en-us/windows/console/getconsolescreenbufferinfo
        GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &coninfo);
        coninfo.dwSize.Y = MAX_CONSOLE_LINES;
        SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE), coninfo.dwSize);

        // Set console mode for supporting color escape sequences.
        {
            HANDLE stdOutHandle = GetStdHandle(STD_OUTPUT_HANDLE);
            DWORD stdOutMode;
            GetConsoleMode(stdOutHandle, &stdOutMode);
            // NOTE(Noah): See https://docs.microsoft.com/en-us/windows/console/setconsolemode for
            // meaning of 0x0004. The macro was not being resolved...
            SetConsoleMode(stdOutHandle, stdOutMode | 0x0004);
        }

        AELoggerLog("stdout initialized");
        // TODO(Noah): Would be nice to have unicode support with our platform logger. Emojis are awesome!
        AELoggerWarn("Please note that the below error is expected and is NOT an error");
        AELoggerError("testing stderr out");
        // TODO(Noah): Make this print version from a manifest or something...
        AELoggerLog("\"Hello, World!\" from " AUTOMATA_ENGINE_NAME_STRING " %s", AUTOMATA_ENGINE_VERSION_STRING);
    }
#endif

    // Query performance counter
    {
        LARGE_INTEGER PerfCountFrequency;
        QueryPerformanceFrequency(&PerfCountFrequency);
        g_PerfCountFrequency64 = PerfCountFrequency.QuadPart;
    }

    windowClass.style         = CS_VREDRAW | CS_HREDRAW;  // Set window to redraw after being resized
    windowClass.lpfnWndProc   = Win32WindowProc;          // Set callback
    windowClass.hInstance     = instance;
    windowClass.hCursor       = LoadCursorA(instance, "MonkeyDemoIconCursor");
    windowClass.lpszClassName = AUTOMATA_ENGINE_NAME_STRING;
    windowClass.hIcon         = LoadIconA(instance, "MonkeyDemoIcon");

    int &globalProgramResult = g_engineMemory.globalProgramResult;

    // NOTE: we need to init enough of ImGui before we hotload the game since
    // the game will steal our imgui context on hotload.
#if !defined(AUTOMATA_ENGINE_DISABLE_IMGUI)
        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();
        {
            ImGuiIO& io = ImGui::GetIO();
            io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
        }
#endif

    // NOTE: we call get last write time first because the load game code does a copy to the
    // temp DLL, and maybe that copy is considered a write and thus modifies the last write time.
    g_gameCodeLastWriteTime = Win32GetLastWriteTime(g_SourceDLLName);
    Win32LoadGameCode(g_SourceDLLName, g_TempDLLName);
    if (GameOnHotload) GameOnHotload(&g_gameMemory);

    HANDLE renderThread = NULL;
    HANDLE inputThread = NULL;

    // NOTE: this event is used to wait for when the input thread window has completed create.
    g_inputThreadEvent     = CreateEventA(NULL,
        TRUE,                   //[in]           BOOL                  bManualReset,
        FALSE /*nonsignaled*/,  // [in]           BOOL                  bInitialState,
        NULL);

    do {
        // TODO(Noah): Here would be a nice instance for the defer statement.
        classAtom = RegisterClassA(&windowClass);
        if(classAtom == 0) {
            AELoggerError("Unable to create window class \"%s\"", windowClass.lpszClassName);
            globalProgramResult = -1;
            break;
        }

        if (GamePreInit != nullptr) {
            GamePreInit(&g_gameMemory);
        }
#if defined(_DEBUG)
        else {
            AELoggerWarn("GamePreInit == nullptr");
        }
#endif

        const DWORD windowStyle = (WS_OVERLAPPEDWINDOW | WS_VISIBLE) &
            ((g_engineMemory.defaultWinProfile == ae::AUTOMATA_ENGINE_WINPROFILE_NORESIZE)
                ?
                (~WS_MAXIMIZEBOX & ~WS_THICKFRAME & ~WS_MINIMIZEBOX) : (DWORD)(0xFFFFFFFF));
        
        // TODO: currently, beginMaximized works. but, it still allows for the window to resize if it is
        // dragged. this conflict with WINPROFILE_NORESIZE. so, we won't be using this for now.
        const bool beginMaximized = g_engineMemory.requestMaximize;

        windowHandle = CreateWindowExA(
            0, // dwExStyle
            windowClass.lpszClassName,
            g_engineMemory.defaultWindowName,
            windowStyle,
            CW_USEDEFAULT, // init X
            CW_USEDEFAULT, // init Y
            (g_engineMemory.defaultWidth == UINT32_MAX) ? CW_USEDEFAULT : g_engineMemory.defaultWidth,
            (g_engineMemory.defaultHeight == UINT32_MAX) ? CW_USEDEFAULT : g_engineMemory.defaultHeight,
            NULL, // parent handle
            NULL, // menu
            instance,
            NULL // structure to be passed to WM_CREATE message
        );

        // TODO(Noah): Can abstract this type of windows error print code.
        if (windowHandle == NULL) {
            DWORD resultCode = GetLastError();
            LogLastError(resultCode, "Unable to create window");
            globalProgramResult = -1;
            break;
        }

        g_hwnd        = windowHandle;
        g_hInstance   = instance;
        g_consoleHwnd = ::GetConsoleWindow();

        if (!beginMaximized && GameHandleWindowResize) {
            // get height and width of window
            RECT rect;
            GetWindowRect(g_hwnd, &rect);
            GameHandleWindowResize(&g_gameMemory, rect.right - rect.left, rect.bottom - rect.top);
        }

        ShowWindow(windowHandle, (beginMaximized) ? SW_MAXIMIZE : showCode);
        UpdateWindow(windowHandle);

        // Create the globalBackBuffer
        {
            ae::game_window_info_t winInfo = Platform_getWindowInfo(false);
            Win32ResizeBackbuffer(&globalBackBuffer, winInfo.width, winInfo.height);
        }

        // Initialize XAudio2 !!!
        // NOTE(Noah): When we free the xAudio2 object, that frees all subordinate objects.
        IXAudio2MasteringVoice* pMasterVoice = nullptr;
        {
            // Initializes the COM library for use by the calling thread.
            if (FAILED(comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED))) {
                AELoggerError("Unable to initialize COM library (XAudio2)");
                globalProgramResult = -1;
                break;
            }

            UINT32 flags = 0;
    #if defined(USING_XAUDIO2_7_DIRECTX) && defined(_DEBUG)
            flags |= XAUDIO2_DEBUG_ENGINE;
    #endif
            if (FAILED(XAudio2Create(&g_pXAudio2, 0, flags))) {
                AELoggerError("Unable to to create an instance of the XAudio2 engine");
                globalProgramResult = -1;
                break;
            }

    #if !defined(USING_XAUDIO2_7_DIRECTX) && defined(_DEBUG)
            // NOTE(Noah): This logging seems useless, empirically after trying to see if
            // it would help to debug some issue.
            //
            // To see the trace output, you need to view ETW logs for this application:
            //    Go to Control Panel, Administrative Tools, Event Viewer.
            //    View->Show Analytic and Debug Logs.
            //    Applications and Services Logs / Microsoft / Windows / XAudio2. 
            //    Right click on Microsoft Windows XAudio2 debug logging, Properties, then Enable Logging, and hit OK 
            XAUDIO2_DEBUG_CONFIGURATION debug = {};
            debug.TraceMask =
                XAUDIO2_LOG_ERRORS | XAUDIO2_LOG_WARNINGS;
            debug.BreakMask = XAUDIO2_LOG_ERRORS;
            g_pXAudio2->SetDebugConfiguration( &debug, 0 );
    #endif

            if (FAILED(g_pXAudio2->CreateMasteringVoice(&pMasterVoice))) {
                AELoggerError("Unable to create a mastering voice (XAudio2)");
                globalProgramResult = -1;
                break;
            }

            g_xa2Buffer.Flags = XAUDIO2_END_OF_STREAM;
            g_xa2Buffer.AudioBytes = 0;
            g_xa2Buffer.pAudioData = nullptr;
            g_xa2Buffer.PlayBegin = 0;
            g_xa2Buffer.PlayLength = 0; // play entire buffer
            g_xa2Buffer.LoopBegin = 0;
            g_xa2Buffer.LoopLength = 0; // entire sample should be looped.
            g_xa2Buffer.LoopCount = 0; // no looping
            g_xa2Buffer.pContext = (void *)&g_gameMemory;
        }

        if (GameInit != nullptr) {
            GameInit(&g_gameMemory);
        }
#if defined(_DEBUG)
        else {
            AELoggerWarn("GameInit == nullptr");
        }
#endif

#if defined(AUTOMATA_ENGINE_VK_BACKEND)

        // create the present fence.
        VkFenceCreateInfo ci = {};
        ci.sType             = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        VK_CHECK(vkCreateFence(g_vkDevice, &ci, nullptr, &g_vkPresentFence));

        // TODO:
        /*
        there is this idea where I have a ton of globals in the backend here.
        I do this to make it easy for me to write functions where the game can query
        something from the engine side.

        however, we could do something different.
        we could make an "engine private" region of the gameMemory structure, and throw all
        the engine into there.

        then we can remove all the globals!!
         */

        // NOTE: this won't block since nothing has been presented yet.
        vk_getNextBackbuffer();
#endif

#if !defined(AUTOMATA_ENGINE_DISABLE_IMGUI)
        // some more ImGUI initialization code :)
        // Setup Platform/Renderer backends
        ImGui_ImplWin32_Init(windowHandle);
#if defined(AUTOMATA_ENGINE_GL_BACKEND)
        {
            const char *glsl_version = "#version 330";
            ImGui_ImplOpenGL3_Init(glsl_version);
        }
#endif

#if defined(AUTOMATA_ENGINE_VK_BACKEND)

        ImGui_ImplVulkan_LoadFunctions(
            [](const char *function_name, void *) { return vkGetInstanceProcAddr(g_vkInstance, function_name); });

        VkCommandPool   vkImguiCommandPool   = VK_NULL_HANDLE;
        VkCommandBuffer vkImguiCommandBuffer = VK_NULL_HANDLE;

        VkCommandPoolCreateInfo poolInfo = {};
        poolInfo.sType                   = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex        = g_vkQueueIndex;
        poolInfo.flags                   = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        VK_CHECK(vkCreateCommandPool(g_vkDevice, &poolInfo, nullptr, &vkImguiCommandPool));
        VkCommandBufferAllocateInfo cmdInfo = {};
        cmdInfo.sType                       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmdInfo.commandPool                 = vkImguiCommandPool;
        cmdInfo.level                       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmdInfo.commandBufferCount          = 1;
        VK_CHECK(vkAllocateCommandBuffers(g_vkDevice, &cmdInfo, &vkImguiCommandBuffer));

        {
            static VkDescriptorPool imguiDescPool = VK_NULL_HANDLE;

            // Create Descriptor Pool for imgui.
            {
                VkDescriptorPoolSize pool_sizes[]    = {{.type = VK_DESCRIPTOR_TYPE_SAMPLER, .descriptorCount = 1000},
                       {.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1000},
                       {.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .descriptorCount = 1000},
                       {.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = 1000},
                       {.type = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, .descriptorCount = 1000},
                       {.type = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, .descriptorCount = 1000},
                       {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 1000},
                       {.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1000},
                       {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, .descriptorCount = 1000},
                       {.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, .descriptorCount = 1000},
                       {.type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, .descriptorCount = 1000}};
                VkDescriptorPoolCreateInfo pool_info = {};
                pool_info.sType                      = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
                pool_info.flags                      = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
                pool_info.maxSets                    = 1000 * IM_ARRAYSIZE(pool_sizes);
                pool_info.poolSizeCount              = (uint32_t)IM_ARRAYSIZE(pool_sizes);
                pool_info.pPoolSizes                 = pool_sizes;
                VK_CHECK(vkCreateDescriptorPool(g_vkDevice, &pool_info, nullptr, &imguiDescPool));
            }

            ImGui_ImplVulkan_InitInfo init_info = {};
            init_info.Instance                  = g_vkInstance;
            init_info.PhysicalDevice            = g_vkGpu;
            init_info.Device                    = g_vkDevice;
            init_info.QueueFamily               = g_vkQueueIndex;
            init_info.Queue                     = g_vkQueue;
            init_info.PipelineCache             = VK_NULL_HANDLE;
            init_info.DescriptorPool            = imguiDescPool;
            init_info.Subpass                   = 0;  // TODO: why?
            init_info.MinImageCount             = 2;
            // TODO: it is currently the case that the count of swapchain images is static.
            // but, how can we have out program statically assert that?
            uint32_t swapCount = StretchyBufferCount(g_vkSwapchainImages);
            IM_ASSERT(swapCount >= g_vkDesiredSwapchainImageCount);
            init_info.ImageCount      = swapCount;
            init_info.MSAASamples     = VK_SAMPLE_COUNT_1_BIT;
            init_info.Allocator       = nullptr;
            init_info.CheckVkResultFn = VK_CHECK;
            ImGui_ImplVulkan_Init(&init_info, vk_MaybeCreateImguiRenderPass());
        }
#endif

        g_isImGuiInitialized = true;

#if defined(AUTOMATA_ENGINE_GL_BACKEND)
        ScaleImGuiForGL();
#endif
#if defined(AUTOMATA_ENGINE_VK_BACKEND)
        ScaleImGuiForVK(vkImguiCommandBuffer, vkImguiCommandPool);
#endif
#endif
        // TODO(Noah): Look into what the imGUI functions are going to return on failure!

        g_bIsWindowFocused = true;//TODO: is this needed?

        renderThread = CreateThread(
            nullptr,  // lp thread attributes.
            0,        // default stack size.
            Win32GameUpdateAndRenderHandlingLoop,
            nullptr,  // lpParameter
            0,        // thread runs immediately after creation.
            nullptr   // pointer to a thing that recieves the thread identifier.
                     );

        inputThread = CreateThread(
            nullptr,  // lp thread attributes.
            0,        // default stack size.
            Win32InputHandlingLoop,
            nullptr,  // lpParameter
            0,        // thread runs immediately after creation.
            nullptr   // pointer to a thing that recieves the thread identifier.
                     );

    } while(0); // WinMainEnd

    // NOTE: here we wait for the input window to exist so that we can immediately begin sending messages to it.
    WaitForSingleObject(g_inputThreadEvent, INFINITE);

    BOOL bRet;
    MSG msg;
    
    // enter into the main message pump.
    while( (bRet = GetMessage( &msg, NULL, 0, 0 )) != 0)
    {
        if (bRet == -1)
        {
            // handle the error and possibly exit
        }
        else
        {
            TranslateMessage(&msg); 
            DispatchMessage(&msg); 
        }

        // TODO: is it possible that we may not
        // proper handle the show mouse request in time
        // due to there being zero message on the queue,
        // and thus this loop blocking?
        
        // check for request to show mouse.
        bool &currValue = g_showMouseCurrentValue;
        bool request = g_showMouseRequestedValue.load();
        if (currValue != request) {
            Platform_showMouse_deferred(request);
            g_showMouseCurrentValue = request;
        }
    }

    // communicate to the other threads that they should close and join with them here
    // before terminate the application.
    g_engineMemory.globalRunning.store(false);
    if (renderThread) WaitForSingleObject(renderThread, INFINITE);
    if (inputThread) WaitForSingleObject(inputThread, INFINITE);

    // TODO(Noah): Can we leverage our new nc_defer.h to replace this code below?
    {
        // before kill Xaudio2, stop all audio and flush, for all voices.
        for (uint32_t i = 0; i < StretchyBufferCount(g_ppSourceVoices); i++) {
            if (g_ppSourceVoices[i].voice != nullptr) {
                // wait for audio thread to finish.
                g_ppSourceVoices[i].voice->DestroyVoice();
                delete g_ppSourceVoices[i].callback;
                delete g_ppSourceVoices[i].xapo;
            }
        }

        // Free XAudio2 resources.
        if (g_pXAudio2 != nullptr) { g_pXAudio2->Release(); }
        if (!FAILED(comResult)) { CoUninitialize(); }

#if !defined(AUTOMATA_ENGINE_DISABLE_IMGUI)
        // ImGUI is tightly coupled with the platform.
        if (g_isImGuiInitialized) {
#if defined(AUTOMATA_ENGINE_GL_BACKEND)
            ImGui_ImplOpenGL3_Shutdown();
#endif
#if defined(AUTOMATA_ENGINE_VK_BACKEND)
            ImGui_ImplVulkan_Shutdown();
#endif
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();
        }
#endif

        if (GameCleanup != nullptr) {
            GameCleanup(&g_gameMemory);
        }
#if defined(_DEBUG)
        else {
            AELoggerWarn("GameCleanup == nullptr");
        }
#endif

        if (g_gameMemory.data != nullptr) {
            Platform_free(g_gameMemory.data);
            g_gameMemory.data = nullptr;
        }

        if (windowHandle != NULL) { DestroyWindow(windowHandle); }
        if (classAtom != 0) { UnregisterClassA(windowClass.lpszClassName, instance); }

        // stall program to allow user to see err.
        
        // TODO(Noah): Sometimes this does not print?
        AELogger(
            "\x1b[2;37;41m"
            "\n\n====================================\nThanks for playing!\n====================================\nPress any key to exit\n"
            "\033[0m"
        );

        //HANDLE outputHdl = GetStdHandle(STD_OUTPUT_HANDLE);
        //FlushConsoleOutputBuffer(outputHdl);

        INPUT_RECORD record;
        DWORD inputsRead;
        HANDLE inputHandle = GetStdHandle(STD_INPUT_HANDLE);
        FlushConsoleInputBuffer(inputHandle);
        ReadConsoleInput(inputHandle, &record, 1, &inputsRead);
    }

    return globalProgramResult;
}
