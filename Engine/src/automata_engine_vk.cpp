
#if defined(AUTOMATA_ENGINE_VK_BACKEND)

#define VOLK_IMPLEMENTATION
#include <automata_engine.hpp>

// TODO: maybe we can have some sort of AE com_release idea?
#define COM_RELEASE(comPtr) (comPtr != nullptr) ? comPtr->Release() : 0;

namespace automata_engine {

    void VK_CHECK(VkResult err)
    {
        if (err) {
            AELoggerError("Detected Vulkan error: %s", ae::VK::VkResultToString(err));
            // TODO: there is probably a much better way to handle this...
            abort();
        }
    }

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

        Image createImage(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage)
        {
            Image              image = {};
            VkImageCreateInfo &ci    = image;
            ci.sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            ci.imageType             = VK_IMAGE_TYPE_2D;
            ci.format                = format;
            ci.extent                = {width, height, 1};
            ci.mipLevels             = 1;
            ci.arrayLayers           = 1;
            ci.samples               = VK_SAMPLE_COUNT_1_BIT;
            ci.usage                 = usage;
            ci.initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED;
            return image;
        }

        size_t createImage_dumb(
            VkDevice device, uint32_t heapIdx, const Image &imageInfo, VkImage *imageOut, VkDeviceMemory *memOut)
        {
            VK_CHECK(vkCreateImage(device, &imageInfo, nullptr, imageOut));
            VkMemoryRequirements imageReq;
            (vkGetImageMemoryRequirements(device, *imageOut, &imageReq));

            VkMemoryAllocateInfo allocInfo = {};
            allocInfo.sType                = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
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

        size_t createImage2D_dumb(VkDevice device,
            uint32_t                       width,
            uint32_t                       height,
            uint32_t                       heapIdx,
            VkFormat                       format,
            VkImageUsageFlags              usage,
            VkImage                       *imageOut,
            VkDeviceMemory                *memOut)
        {
            auto imageInfo = createImage(width, height, format, usage);
            return createImage_dumb(device, heapIdx, imageInfo, imageOut, memOut);
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

        // TODO:: is there a nice way to guarantee that the caller of this is always doing
        // after pre init, where e.g. ae::defaultWindowName is always stable?
        void doDefaultInit(VkInstance *pInstance,
            VkPhysicalDevice          *pGpu,
            VkDevice                  *pDevice,
            VkQueue                   *pQueue,
            uint32_t                  *pQueueIndex,
            VkDebugUtilsMessengerEXT  *pDebugCallback)
        {
            // TODO: remove need for volk.
            if (volkInitialize()) {
                AELoggerError("Failed to initialize volk.");
                return;
            }

            auto validateExtensions = [](const char **required, const VkExtensionProperties *available) -> bool {
                for (uint32_t i = 0; i < StretchyBufferCount(required); i++) {
                    auto extension = required[i];
                    bool found     = false;
                    for (uint32_t j = 0; j < StretchyBufferCount(available); j++) {
                        auto available_extension = available[j];
                        if (strcmp(available_extension.extensionName, extension) == 0) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) { return false; }
                }
                return true;
            };

            // query the instance layers we want.
            const char **requested_validation_layers = nullptr;
            {
                auto validateLayers = [](const char              **required,
                                          uint32_t                 requiredCount,
                                          const VkLayerProperties *available,
                                          uint32_t                 availableCount) -> bool {
                    for (uint32_t i = 0; i < requiredCount; i++) {
                        auto *extension = required[i];
                        bool  found     = false;
                        for (uint32_t j = 0; j < availableCount; j++) {
                            auto available_extension = available[j];
                            if (strcmp(available_extension.layerName, extension) == 0) {
                                found = true;
                                break;
                            }
                        }
                        if (!found) { return false; }
                    }
                    return true;
                };

                uint32_t           instance_layer_count;
                VkLayerProperties *supported_validation_layers = nullptr;
                defer(StretchyBufferFree(supported_validation_layers));
                VK_CHECK(vkEnumerateInstanceLayerProperties(&instance_layer_count, nullptr));
                StretchyBufferInitWithCount(supported_validation_layers, instance_layer_count);
                VK_CHECK(vkEnumerateInstanceLayerProperties(&instance_layer_count, supported_validation_layers));

                #if defined(_DEBUG)
                {
                    std::vector<std::vector<const char *>> validation_layer_priority_list = {
                        // The preferred validation layer is "VK_LAYER_KHRONOS_validation"
                        {"VK_LAYER_KHRONOS_validation"},
                        // Otherwise we fallback to using the LunarG meta layer
                        {"VK_LAYER_LUNARG_standard_validation"},
                        // Otherwise we attempt to enable the individual layers that compose the LunarG meta layer since it doesn't exist
                        {
                            "VK_LAYER_GOOGLE_threading",
                            "VK_LAYER_LUNARG_parameter_validation",
                            "VK_LAYER_LUNARG_object_tracker",
                            "VK_LAYER_LUNARG_core_validation",
                            "VK_LAYER_GOOGLE_unique_objects",
                        },
                        // Otherwise as a last resort we fallback to attempting to enable the LunarG core layer
                        {"VK_LAYER_LUNARG_core_validation"}};

                    for (auto validation_layers : validation_layer_priority_list) {
                        if (validateLayers(validation_layers.data(),
                                validation_layers.size(),
                                supported_validation_layers,
                                StretchyBufferCount(supported_validation_layers))) {
                            for (auto layer : validation_layers) {
                                StretchyBufferPush(requested_validation_layers, layer);
                            }
                            break;
                        }

                        AELoggerWarn("Couldn't find {%s, ...} - falling back", validation_layers[0]);
                    }
                }


                if (requested_validation_layers == nullptr) { AELoggerWarn("No validation layers enabled"); }

                #endif
            }  // TODO: can we get rid of all the complexity above? I bet this kind of thing isn't really needed.

            // setup instance extensions.
            const char **active_instance_extensions = nullptr;
            defer(StretchyBufferFree(active_instance_extensions));
            {
                uint32_t               instance_extension_count;
                VkExtensionProperties *instance_extensions = nullptr;
                defer(StretchyBufferFree(instance_extensions));

                VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &instance_extension_count, nullptr));
                StretchyBufferInitWithCount(instance_extensions, instance_extension_count);
                VK_CHECK(
                    vkEnumerateInstanceExtensionProperties(nullptr, &instance_extension_count, instance_extensions));
                assert(instance_extension_count == StretchyBufferCount(instance_extensions));

                StretchyBufferPush(active_instance_extensions, VK_KHR_SURFACE_EXTENSION_NAME);
                // TODO: this is win32 specific. default init is win32??
                StretchyBufferPush(active_instance_extensions, VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
                StretchyBufferPush(active_instance_extensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
                //StretchyBufferPush(active_instance_extensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

                if (!validateExtensions(active_instance_extensions, instance_extensions)) {
                    AELoggerError("Required instance extensions are missing.");
                    AELoggerLog("instance exts found count = %d", StretchyBufferCount(instance_extensions));
                    return;
                }
            }

            VkApplicationInfo app = {};
            app.sType             = VK_STRUCTURE_TYPE_APPLICATION_INFO;
            app.pApplicationName  = ae::defaultWindowName;
            // TODO: is there a place from AE that we can get this?
            app.pEngineName = "Automata Engine";
            app.apiVersion  = VK_MAKE_VERSION(1, 3, 0);  // highest version the app will use.

            VkInstanceCreateInfo instance_info = {};
            instance_info.sType                = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
            instance_info.pApplicationInfo     = &app;

            instance_info.enabledExtensionCount   = StretchyBufferCount(active_instance_extensions);
            instance_info.ppEnabledExtensionNames = active_instance_extensions;

            // layers are inserted into VK API call chains when they are enabled.
            instance_info.enabledLayerCount   = StretchyBufferCount(requested_validation_layers);
            instance_info.ppEnabledLayerNames = requested_validation_layers;

            ae::VK_CHECK(vkCreateInstance(&instance_info, nullptr, pInstance));

            // Load VK instance functions.
            volkLoadInstance(*pInstance);

            // setup the debug print callback idea.
            {
                VkDebugUtilsMessengerCreateInfoEXT debugMsgerCI = {};
                debugMsgerCI.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
                debugMsgerCI.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

                {
                    debugMsgerCI.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                                    VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
                }

                debugMsgerCI.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
                debugMsgerCI.pfnUserCallback = ae::VK::ValidationDebugCallback;

                vkCreateDebugUtilsMessengerEXT(*pInstance, &debugMsgerCI, nullptr, pDebugCallback);
            }

            // now we init device and queue.
            {
                // find the GPU.
                uint32_t          gpu_count = 0;
                VkPhysicalDevice *gpus      = nullptr;
                defer(StretchyBufferFree(gpus));
                VK_CHECK(vkEnumeratePhysicalDevices(*pInstance, &gpu_count, nullptr));
                if (gpu_count != 1) {
                    if (gpu_count < 1)
                        AELoggerError("No physical device found.");
                    else
                        AELoggerError("Too many GPUs! Automata Engine only supports single adapter systems.");
                    return;
                }
                StretchyBufferInitWithCount(gpus, gpu_count);
                VK_CHECK(vkEnumeratePhysicalDevices(*pInstance, &gpu_count, gpus));

                // set gpu.
                *pGpu = gpus[0];

                // find the graphics queue.
                uint32_t                 queue_family_count      = 0;
                VkQueueFamilyProperties *queue_family_properties = nullptr;
                defer(StretchyBufferFree(queue_family_properties));
                (vkGetPhysicalDeviceQueueFamilyProperties(*pGpu, &queue_family_count, nullptr));
                if (queue_family_count < 1) {
                    AELoggerError("No queue family found.");
                    return;
                }
                StretchyBufferInitWithCount(queue_family_properties, queue_family_count);
                vkGetPhysicalDeviceQueueFamilyProperties(*pGpu, &queue_family_count, queue_family_properties);
                for (uint32_t i = 0; i < queue_family_count; i++) {
                    // Find a queue family which supports graphics and presentation.
                    if ((queue_family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
                        *pQueueIndex = i;
                        break;
                    }
                }
                if (*pQueueIndex < 0) {
                    AELoggerError("Did not find suitable queue which supports graphics, compute and presentation.");
                }

                // Initialize required extensions.
                uint32_t               device_extension_count;
                VkExtensionProperties *device_extensions = nullptr;
                defer(StretchyBufferFree(device_extensions));
                VK_CHECK(vkEnumerateDeviceExtensionProperties(*pGpu, nullptr, &device_extension_count, nullptr));
                StretchyBufferInitWithCount(device_extensions, device_extension_count);
                VK_CHECK(
                    vkEnumerateDeviceExtensionProperties(*pGpu, nullptr, &device_extension_count, device_extensions));

                // Initialize required extensions.
                const char **required_device_extensions = nullptr;
                defer(StretchyBufferFree(required_device_extensions));
                StretchyBufferPush(required_device_extensions, VK_KHR_SWAPCHAIN_EXTENSION_NAME);
                if (!validateExtensions(required_device_extensions, device_extensions)) {
                    AELoggerError("missing required device extensions");
                    throw std::exception();
                }

                VkDeviceQueueCreateInfo queue_info = {};
                queue_info.sType                   = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                const float queue_priority         = 1.0f;
                queue_info.queueFamilyIndex        = *pQueueIndex;
                queue_info.queueCount              = 1;
                queue_info.pQueuePriorities        = &queue_priority;

                VkDeviceCreateInfo device_info      = {};
                device_info.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
                device_info.queueCreateInfoCount    = 1;
                device_info.pQueueCreateInfos       = &queue_info;
                device_info.enabledExtensionCount   = StretchyBufferCount(required_device_extensions);
                device_info.ppEnabledExtensionNames = required_device_extensions;

#if VK_VERSION_1_3

                VkPhysicalDeviceVulkan13Features physDevVk13Features = {};
                physDevVk13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;

                VkPhysicalDeviceFeatures2 physDevFeatures2 = {};
                physDevFeatures2.sType                     = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

                physDevFeatures2.pNext = &physDevVk13Features;

                vkGetPhysicalDeviceFeatures2(*pGpu, &physDevFeatures2);

                device_info.pNext = &physDevFeatures2;
#endif

                VK_CHECK(vkCreateDevice(*pGpu, &device_info, nullptr, pDevice));

                // loads device functions.
                volkLoadDevice(*pDevice);

                vkGetDeviceQueue(*pDevice, *pQueueIndex, 0, pQueue);
            }
        }

        // determine heap indices.
        uint32_t getDesiredMemoryTypeIndex(
            VkPhysicalDevice gpu, VkMemoryPropertyFlags positiveProp, VkMemoryPropertyFlags negativeProp)
        {
            VkPhysicalDeviceMemoryProperties deviceMemoryProperties;
            vkGetPhysicalDeviceMemoryProperties(gpu, &deviceMemoryProperties);

            for (uint32_t i = 0; i < deviceMemoryProperties.memoryTypeCount; i++) {
                auto flags = deviceMemoryProperties.memoryTypes[i].propertyFlags;
                if (((flags & positiveProp) == positiveProp) && ((flags & negativeProp) == 0)) { return i; }
            }
            AELoggerWarn("Could not find a heap with a suitable memory type!");
            return UINT32_MAX;
        };

        Sampler samplerCreateInfo()
        {
            Sampler              s           = {};
            VkSamplerCreateInfo &samplerInfo = s;
            samplerInfo.sType                = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            samplerInfo.magFilter            = VK_FILTER_LINEAR;
            samplerInfo.minFilter            = VK_FILTER_LINEAR;
            samplerInfo.mipmapMode           = VK_SAMPLER_MIPMAP_MODE_NEAREST;  // resample across mips.
            samplerInfo.addressModeU         = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerInfo.addressModeV         = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerInfo.addressModeW         = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerInfo.mipLodBias           = 0.0f;
            samplerInfo.compareEnable        = VK_TRUE;
            samplerInfo.compareOp            = VK_COMPARE_OP_LESS;
            samplerInfo.minLod               = 0.0f;
            samplerInfo.maxLod               = VK_LOD_CLAMP_NONE;
            samplerInfo.maxAnisotropy        = 1.0;
            samplerInfo.anisotropyEnable     = VK_FALSE;
            samplerInfo.borderColor          = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
            return s;
        }

        CommandBuffer commandBufferAllocateInfo(uint32_t count, VkCommandPool pool)
        {
            CommandBuffer                buf          = {};
            VkCommandBufferAllocateInfo &cmd_buf_info = buf;
            cmd_buf_info.sType                        = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            cmd_buf_info.commandPool                  = pool;
            cmd_buf_info.level                        = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            cmd_buf_info.commandBufferCount           = count;
            return buf;
        }

        CommandPool commandPoolCreateInfo(uint32_t queueIndex)
        {
            CommandPool              pool          = {};
            VkCommandPoolCreateInfo &cmd_pool_info = pool;
            cmd_pool_info.sType                    = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            cmd_pool_info.queueFamilyIndex         = queueIndex;
            cmd_pool_info.flags                    = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            return pool;
        }

        VkResult beginCommandBuffer(VkCommandBuffer cmd)
        {
            VkCommandBufferBeginInfo info = {};
            info.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            return vkBeginCommandBuffer(cmd, &info);
        }

        ImageMemoryBarrier imageMemoryBarrier(
            VkAccessFlags src, VkAccessFlags dst, VkImageLayout srcLayout, VkImageLayout dstLayout, VkImage img)
        {
            ImageMemoryBarrier    imb     = {};
            VkImageMemoryBarrier &barrier = imb;
            barrier.sType                 = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.srcAccessMask         = src;
            barrier.dstAccessMask         = dst;
            barrier.oldLayout             = srcLayout;
            barrier.newLayout             = dstLayout;
            barrier.image                 = img;
            barrier.subresourceRange      = {
                VK_IMAGE_ASPECT_COLOR_BIT,
                0,  // base mip level.
                1,  //mip count.
                0,  //array base.
                1   //array layers
            };
            return imb;
        }

        BufferMemoryBarrier bufferMemoryBarrier(
            VkAccessFlags src, VkAccessFlags dst, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size)
        {
            BufferMemoryBarrier    bmb     = {};
            VkBufferMemoryBarrier &barrier = bmb;
            barrier.sType                  = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            barrier.srcAccessMask          = src;
            barrier.dstAccessMask          = dst;
            barrier.buffer                 = buffer;
            barrier.offset                 = offset;
            barrier.size                   = size;
            return bmb;
        }

        ImageView createImageView(VkImage image, VkFormat format)
        {
            ImageView              view           = {};
            VkImageViewCreateInfo &view_info      = view;
            view_info.sType                       = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            view_info.viewType                    = VK_IMAGE_VIEW_TYPE_2D;
            view_info.format                      = format;
            view_info.image                       = image;
            view_info.subresourceRange.levelCount = 1;
            view_info.subresourceRange.layerCount = 1;
            view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            return view;
        }

        void cmdImageMemoryBarrier(VkCommandBuffer cmd,
            VkPipelineStageFlags                   before,
            VkPipelineStageFlags                   after,
            uint32_t                               count,
            VkImageMemoryBarrier                  *pBarriers)
        {
            vkCmdPipelineBarrier(cmd, before, after, 0, 0, nullptr, 0, nullptr, count, pBarriers);
        }

        void cmdBufferMemoryBarrier(VkCommandBuffer cmd,
            VkPipelineStageFlags                    before,
            VkPipelineStageFlags                    after,
            uint32_t                                count,
            VkBufferMemoryBarrier                  *pBarriers)
        {
            vkCmdPipelineBarrier(cmd, before, after, 0, 0, nullptr, count, pBarriers, 0, nullptr);
        }

        GraphicsPipeline createGraphicsPipeline(VkShaderModule vertShader,
            VkShaderModule                                     fragShader,
            const char                                        *vertEntryName,
            const char                                        *fragEntryName,
            VkPipelineLayout                                   pipelineLayout,
            VkRenderPass                                       renderPass)
        {
            GraphicsPipeline gPipe = {};

            // NOTE: the default setup for the vertex binding stuff here is based on a single
            // bound vertex buffer where the vertex within is same as our .OBJ.
            static constexpr VkVertexInputAttributeDescription vertex_input_attr_desc[3] = {
                {
                    .location = 0,
                    .binding  = 0,  // slot 0.
                    .format   = VK_FORMAT_R32G32B32_SFLOAT,
                    .offset   = 0,  // offset in vertex buf with which to look for elems.
                },
                {.location   = 1,  // in shader.
                    .binding = 0,
                    .format  = VK_FORMAT_R32G32_SFLOAT,
                    .offset  = sizeof(float) * 3},
                {.location   = 2,  // in shader.
                    .binding = 0,
                    .format  = VK_FORMAT_R32G32B32_SFLOAT,
                    .offset  = sizeof(float) * 5}};

            static constexpr VkVertexInputBindingDescription vertex_input_binding_desc[1] = {
                {.binding      = 0,                  // this structure describes slot 0.
                    .stride    = sizeof(float) * 8,  // to get to next vertex.
                    .inputRate = VK_VERTEX_INPUT_RATE_VERTEX}};

            VkPipelineVertexInputStateCreateInfo &vertex_input = gPipe.m_vertexInputState;
            vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

            vertex_input.vertexBindingDescriptionCount   = _countof(vertex_input_binding_desc);
            vertex_input.pVertexBindingDescriptions      = vertex_input_binding_desc;
            vertex_input.vertexAttributeDescriptionCount = _countof(vertex_input_attr_desc);
            vertex_input.pVertexAttributeDescriptions    = vertex_input_attr_desc;

            // Specify we will use triangle lists to draw geometry.
            VkPipelineInputAssemblyStateCreateInfo &input_assembly = gPipe.m_inputAssemblyState;
            input_assembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

            // Specify rasterization state.
            VkPipelineRasterizationStateCreateInfo &raster = gPipe.m_rasterizationState;
            raster.sType                                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
            raster.cullMode                                = VK_CULL_MODE_BACK_BIT;
            raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;  // NOTE: how our .OBJ is. also default in GL.
            raster.lineWidth = 1.0f;
            raster.rasterizerDiscardEnable = VK_FALSE;

            // blend infos.
            static constexpr VkPipelineColorBlendAttachmentState blend_attachment = {
                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                                  VK_COLOR_COMPONENT_A_BIT};

            VkPipelineColorBlendStateCreateInfo &blend = gPipe.m_colorBlendState;
            blend.sType                                = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            blend.attachmentCount                      = 1;
            blend.pAttachments                         = &blend_attachment;

            // We will have one viewport and scissor box.
            VkPipelineViewportStateCreateInfo &viewport = gPipe.m_viewportState;
            viewport.sType                              = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
            viewport.viewportCount                      = 1;
            viewport.scissorCount                       = 1;

            // set depth testing params.
            VkPipelineDepthStencilStateCreateInfo &depth_stencil = gPipe.m_depthStencilState;
            depth_stencil.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
            depth_stencil.depthTestEnable       = VK_TRUE;
            depth_stencil.stencilTestEnable     = VK_FALSE;
            depth_stencil.depthBoundsTestEnable = VK_FALSE;
            depth_stencil.depthCompareOp        = VK_COMPARE_OP_LESS;
            depth_stencil.depthWriteEnable      = VK_TRUE;

            // No multisampling.
            VkPipelineMultisampleStateCreateInfo &multisample = gPipe.m_multisampleState;
            multisample.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
            multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

            // which states of the pipeline are dynamic?
            static constexpr VkDynamicState   dynamics[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
            VkPipelineDynamicStateCreateInfo &dynamic     = gPipe.m_dynamicState;
            dynamic.sType                                 = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
            dynamic.pDynamicStates                        = dynamics;
            // TODO: _countof is MSVC specific.
            dynamic.dynamicStateCount = _countof(dynamics);

            VkPipelineShaderStageCreateInfo (&shader_stages)[2] = gPipe.m_stages;
            // Vertex stage of the pipeline
            shader_stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            shader_stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
            shader_stages[0].module = vertShader;
            shader_stages[0].pName  = vertEntryName;
            // Fragment stage of the pipeline
            shader_stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            shader_stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
            shader_stages[1].module = fragShader;
            shader_stages[1].pName  = fragEntryName;

            VkGraphicsPipelineCreateInfo &pipe  = gPipe;
            pipe.sType                          = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            pipe.stageCount                     = _countof(shader_stages);
            pipe.pStages                        = shader_stages;
            pipe.pVertexInputState              = &vertex_input;
            pipe.pInputAssemblyState            = &input_assembly;
            pipe.pRasterizationState            = &raster;
            pipe.pColorBlendState               = &blend;
            pipe.pMultisampleState              = &multisample;
            pipe.pViewportState                 = &viewport;
            pipe.pDepthStencilState             = &depth_stencil;
            pipe.pDynamicState                  = &dynamic;
            pipe.renderPass                     = renderPass;
            pipe.layout                         = pipelineLayout;

            return gPipe;
        }

        void updateDescriptorSets(VkDevice device, uint32_t count, VkWriteDescriptorSet *writes)
        {
            vkUpdateDescriptorSets(device,
                count,  // write count.
                writes,
                0,       //copy count.
                nullptr  // copies.
            );
        }

        PipelineLayout createPipelineLayout(uint32_t setCount, VkDescriptorSetLayout *layouts)
        {
            PipelineLayout layout = {};
            VkPipelineLayoutCreateInfo &layout_info = layout;
            layout_info.sType                      = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            layout_info.setLayoutCount             = setCount;
            layout_info.pSetLayouts                = layouts;
            return layout;
        }

        DescriptorSetLayout createDescriptorSetLayout(uint32_t bindCount, VkDescriptorSetLayoutBinding *bindings)
        {
            DescriptorSetLayout              setLayout = {};
            VkDescriptorSetLayoutCreateInfo &setInfo   = setLayout;
            setInfo.sType                              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            setInfo.bindingCount                       = bindCount;
            setInfo.pBindings                          = bindings;
            return setLayout;
        }

        RenderPass createRenderPass(uint32_t attachmentCount,
            VkAttachmentDescription         *attachments,
            uint32_t                         subpassCount,
            VkSubpassDescription            *subpasses)
        {
            RenderPass              rp      = {};
            VkRenderPassCreateInfo &rp_info = rp;
            rp_info.sType                   = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
            rp_info.attachmentCount         = attachmentCount;
            rp_info.pAttachments            = attachments;
            rp_info.subpassCount            = 1;
            rp_info.pSubpasses              = subpasses;
            return rp;
        }

    }  // namespace VK
};     // namespace automata_engine

#endif
