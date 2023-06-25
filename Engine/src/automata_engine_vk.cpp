
#if defined(AUTOMATA_ENGINE_VK_BACKEND)

#define VOLK_IMPLEMENTATION
#include <automata_engine.hpp>

// TODO: maybe we can have some sort of AE com_release idea?
#define COM_RELEASE(comPtr) (comPtr != nullptr) ? comPtr->Release() : 0;

// TODO: maybe we can have like an AE_vk_check kind of idea?
#define VK_CHECK(x)                                                                                                    \
    do {                                                                                                               \
        VkResult err = x;                                                                                              \
        if (err) {                                                                                                     \
            AELoggerError("Detected Vulkan error: %s", ae::VK::VkResultToString(err));                                 \
            abort();                                                                                                   \
        }                                                                                                              \
    } while (0)

namespace automata_engine {
    namespace VK {

        // TODO: so since we can have VK on different platforms, it would be nice to use things that are not WCHAR here.
        VkShaderModule loadShaderModule(
            VkDevice vkDevice, const char *filePathIn, const WCHAR *entryPoint, const WCHAR *profile)
        {
            IDxcBlob *spirvBlob;
            defer(COM_RELEASE(spirvBlob));

            bool emitSpirv = true;
            ae::HLSL::compileBlobFromFile(filePathIn, entryPoint, profile, &spirvBlob, emitSpirv);

            if (spirvBlob->GetBufferPointer()) {
                VkShaderModuleCreateInfo module_info = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
                module_info.codeSize                 = spirvBlob->GetBufferSize();
                module_info.pCode                    = (uint32_t *)spirvBlob->GetBufferPointer();

                VkShaderModule shader_module;
                VK_CHECK(vkCreateShaderModule(vkDevice,
                    &module_info,
                    nullptr,  // this is an allocator. if we provided one it means the VK driver would not alloc and would use our impl to alloc. this is an alloc on the host (system RAM, not VRAM).
                    &shader_module));
                return shader_module;
            }

            return VK_NULL_HANDLE;
        }

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

        VKAPI_ATTR VkBool32 VKAPI_CALL ValidationDebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
            VkDebugUtilsMessageTypeFlagsEXT                                                           messageType,
            const VkDebugUtilsMessengerCallbackDataEXT                                               *pCallbackData,
            void                                                                                     *pUserData)
        {
            if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
                AELoggerError("Validation Layer: Error: %s", pCallbackData->pMessage);
            } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
                AELoggerWarn("Validation Layer: Warning: %s", pCallbackData->pMessage);
            } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
                AELoggerLog("Validation Layer: Info: %s", pCallbackData->pMessage);
            }
#if 0
	else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
		AELoggerLog("Validation Layer: Verbose: %s", pCallbackData->pMessage);
	}
#endif
            return VK_FALSE;
        }

        size_t createBufferDumb(VkDevice device,
            size_t                       size,
            uint32_t                     heapIdx,
            VkBufferUsageFlags           usage,
            VkBuffer                    *bufferOut,
            VkDeviceMemory              *memOut)
        {
            VkBufferCreateInfo bufCI = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
            bufCI.size               = size;
            bufCI.usage              = usage;

            vkCreateBuffer(device, &bufCI, nullptr, bufferOut);

            VkMemoryRequirements bufferReq;
            (vkGetBufferMemoryRequirements(device, *bufferOut, &bufferReq));

            VkMemoryAllocateFlagsInfo flagsInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO};

            VkMemoryAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
            allocInfo.allocationSize       = ae::math::align_up(bufferReq.size, bufferReq.alignment);  //TODO.
            allocInfo.memoryTypeIndex      = heapIdx;

            // NOTE: what is the point of this API ??
            //
            // well, this is the only way to get the GPU virtual address of a buffer.
            //
            // otherwise, when we map to some vkdevicememory, we don't actually know about any GPU virt
            // addresses. we only know about this opaque handle to the device mem. and through API calls
            // we can point to offsets within that.
            //
            if (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
                allocInfo.pNext = &flagsInfo;
                flagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
            }

            size_t out = allocInfo.allocationSize;

            vkAllocateMemory(device, &allocInfo, nullptr, memOut);

            VK_CHECK(vkBindBufferMemory(device,
                *bufferOut,
                *memOut,
                0));  // so this offset is how we can do placed resources effectively - we can bind to an offset in the memory.

            return out;
        }

        size_t createImage2D_dumb(VkDevice device,
            uint32_t                       width,
            uint32_t                       height,
            uint32_t                       heapIdx,
            VkFormat                       format,
            VkImageUsageFlags              usage,
            VkImage                       *imageOut,
            VkDeviceMemory                *memOut)
        {
            VkImageCreateInfo ci = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
            ci.imageType         = VK_IMAGE_TYPE_2D;
            ci.format            = format;
            ci.extent            = {width, height, 1};
            ci.mipLevels         = 1;
            ci.arrayLayers       = 1;
            ci.samples           = VK_SAMPLE_COUNT_1_BIT;
            ci.usage             = usage;
            ci.initialLayout     = VK_IMAGE_LAYOUT_UNDEFINED;

            VK_CHECK(vkCreateImage(device, &ci, nullptr, imageOut));
            VkMemoryRequirements imageReq;
            (vkGetImageMemoryRequirements(device, *imageOut, &imageReq));

            VkMemoryAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
            allocInfo.allocationSize       = ae::math::align_up(imageReq.size, imageReq.alignment);  //TODO.
            allocInfo.memoryTypeIndex      = heapIdx;

            size_t res = allocInfo.allocationSize;

            // bind this to some memory.
            vkAllocateMemory(device, &allocInfo, nullptr, memOut);

            VK_CHECK(vkBindImageMemory(device,
                *imageOut,
                *memOut,
                0));  // so this offset is how we can do placed resources effectively ...

            return res;
        }

        size_t createUploadBufferDumb(VkDevice device,
            size_t                             size,
            uint32_t                           heapIdx,
            VkBufferUsageFlags                 usage,
            VkBuffer                          *bufferOut,
            VkDeviceMemory                    *memOut,
            void                             **pData)
        {
            auto res = createBufferDumb(device, size, heapIdx, usage, bufferOut, memOut);

            // do the map.
            auto   thingToMap = *memOut;
            size_t mapOffset  = 0;
            size_t mapSize    = size;
            size_t flags      = 0;
            VK_CHECK(vkMapMemory(device, thingToMap, mapOffset, mapSize, flags, pData));

            return res;
        }

        VkDeviceAddress getBufferVirtAddr(VkDevice device, VkBuffer buffer)
        {
            VkBufferDeviceAddressInfo info = {VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
            info.buffer                    = buffer;
            return vkGetBufferDeviceAddress(device, &info);
        }

        void flushAndUnmapUploadBuffer(VkDevice device, size_t size, VkDeviceMemory mappedThing)
        {
            VkMappedMemoryRange range = {VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};
            range.memory              = mappedThing;
            range.offset              = 0;
            range.size                = size;
            // flush the write caches so that GPU can see the data.
            VK_CHECK(vkFlushMappedMemoryRanges(device, 1, &range));

            // unmap the buffer.
            vkUnmapMemory(device, mappedThing);
        }

    }  // namespace VK
};     // namespace automata_engine

#endif
