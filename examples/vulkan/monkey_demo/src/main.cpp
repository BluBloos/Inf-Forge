#include <automata_engine.hpp>

typedef uint32_t u32;

void GameUpdateAndRender(ae::game_memory_t *gameMemory);
void CreateGraphicsPipeline(VkDevice vkDevice,
    VkShaderModule                   vertShader,
    VkShaderModule                   fragShader,
    VkPipelineLayout                 pipelineLayout,
    VkRenderPass                     renderPass,  //TODO,
    VkPipeline                      *pPipeline);
void WaitForAndResetFence(VkDevice device, VkFence *pFence, uint64_t waitTime = 1000 * 1000 * 1000);

typedef struct game_state {
    ae::raw_model_t suzanne;

    VkPhysicalDevice vkGpu;
    VkDevice         vkDevice;
    VkInstance       vkInstance;

    VkDebugUtilsMessengerEXT vkDebugCallback;

    uint32_t        gfxQueueIndex;  // TODO: maybe don't care about this?
    VkQueue         vkQueue;
    VkCommandBuffer commandBuffer;
    VkCommandPool   commandPool;

    VkFence vkInitFence;

    VkDescriptorSetLayout setLayout;
    VkPipelineLayout      pipelineLayout;
    VkPipeline            gameShader;

    VkRenderPass vkRenderPass;  // TODO: dyn rendering prefer.

    // NOTE: 10 is certainly larger than swapchain count.
    VkFramebuffer vkFramebufferCache[10] = {};

    VkImage        checkerTexture;
    VkDeviceMemory checkerTextureBacking;

    VkSampler sampler;

    uint32_t              suzanneIndexCount;
    ae::math::transform_t suzanneTransform;
    VkBuffer              suzanneIbo;
    VkDeviceMemory        suzanneIboBacking;
    VkBuffer              suzanneVbo;
    VkDeviceMemory        suzanneVboBacking;

    ae::math::camera_t cam;
} game_state_t;

struct Vertex {
    float x, y, z;
    float u, v;
    float nx, ny, nz;
};

game_state_t *getGameState(ae::game_memory_t *gameMemory) { return (game_state_t *)gameMemory->data; }

void ae::HandleWindowResize(game_memory_t *gameMemory, int newWidth, int newHeight) {}
void ae::PreInit(game_memory_t *gameMemory)
{
    ae::defaultWinProfile = AUTOMATA_ENGINE_WINPROFILE_NORESIZE;
    ae::defaultWindowName = "MonkeyDemo";
}

static struct {
    VkBuffer       uploadBuffer;
    VkDeviceMemory uploadBufferBacking;
    VkBuffer       buffer = VK_NULL_HANDLE;
    VkImage        image  = VK_NULL_HANDLE;
    VkImageLayout  dstLayout;
    VkDeviceMemory backing;

    union {
        VkDeviceSize size;
        struct {
            u32 width;
            u32 height;
        } dims;
    };
} g_uploadResources[3] = {};

void ae::Init(game_memory_t *gameMemory)
{
    auto          winInfo = ae::platform::getWindowInfo();
    game_state_t *gd      = getGameState(gameMemory);

    *gd = {}; // zero it out.

    ae::VK::doDefaultInit(
        &gd->vkInstance, &gd->vkGpu, &gd->vkDevice, &gd->gfxQueueIndex, &gd->vkQueue, &gd->vkDebugCallback);

    // NOTE: init the VK resources on the engine side. this lets it know what queue to wait for work
    // and to present on. at this time, the engine also initializes the swapchain for the first time.
    ae::platform::VK::init(gd->vkInstance, gd->vkGpu, gd->vkDevice, gd->vkQueue);

    auto device = gd->vkDevice;

    // create the pipeline.
    {
        auto vertModule = ae::VK::loadShaderModule(device, "res\\vert.hlsl", L"main", L"vs_6_0");
        auto fragModule = ae::VK::loadShaderModule(device, "res\\frag.hlsl", L"main", L"ps_6_0");

        // TODO: review in detail https://registry.khronos.org/vulkan/specs/1.3-extensions/html/vkspec.html#textures. how is the image precisely sampled?
        //
        auto samplerInfo = ae::VK::samplerCreateInfo().magFilter(VK_FILTER_LINEAR);
        ae::VK_CHECK(vkCreateSampler(gd->vkDevice, &samplerInfo, nullptr, &gd->sampler));

        // TODO: is it possible to clean up the below?

        VkDescriptorSetLayoutBinding bindings[2] = {};
        bindings[0].binding                      = 0;
        bindings[0].descriptorType               = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        bindings[0].descriptorCount              = 1;
        bindings[0].stageFlags                   = VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings[1].binding                      = 1;
        bindings[1].descriptorType               = VK_DESCRIPTOR_TYPE_SAMPLER;
        bindings[1].descriptorCount              = 1;
        bindings[1].stageFlags                   = VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings[1].pImmutableSamplers           = &gd->sampler;

        VkDescriptorSetLayoutCreateInfo setInfo = {};
        setInfo.sType                           = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        setInfo.bindingCount                    = _countof(bindings);
        setInfo.pBindings                       = bindings;

        ae::VK_CHECK(vkCreateDescriptorSetLayout(gd->vkDevice, &setInfo, nullptr, &gd->setLayout));

        VkPushConstantRange pushRange = {};
        pushRange.stageFlags          = VK_SHADER_STAGE_VERTEX_BIT;
        pushRange.offset              = 0;  // TODO.
        pushRange.size                = sizeof(ae::math::mat4_t) * 2;

        VkPipelineLayoutCreateInfo layout_info = {};
        layout_info.sType                      = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        // TODO: for now we don't care about the descriptors.
        //layout_info.setLayoutCount             = 1;
        //layout_info.pSetLayouts                = &gd->setLayout;
        layout_info.pushConstantRangeCount = 1;
        layout_info.pPushConstantRanges    = &pushRange;

        ae::VK_CHECK(vkCreatePipelineLayout(gd->vkDevice, &layout_info, nullptr, &gd->pipelineLayout));

        // init the render pass
        {
            VkAttachmentDescription attachment = {0};
            attachment.format                  = VK_FORMAT_B8G8R8A8_SRGB;  // TODO.
            attachment.samples                 = VK_SAMPLE_COUNT_1_BIT;
            attachment.loadOp                  = VK_ATTACHMENT_LOAD_OP_CLEAR;
            attachment.storeOp                 = VK_ATTACHMENT_STORE_OP_STORE;
            attachment.initialLayout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            attachment.finalLayout             = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            VkAttachmentReference color_ref = {0,  // ref attachment 0
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

            VkSubpassDescription subpass = {0};
            subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpass.colorAttachmentCount = 1;
            subpass.pColorAttachments    = &color_ref;

            // Finally, create the renderpass.
            VkRenderPassCreateInfo rp_info = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
            rp_info.attachmentCount        = 1;
            rp_info.pAttachments           = &attachment;
            rp_info.subpassCount           = 1;
            rp_info.pSubpasses             = &subpass;

            VK_CHECK(vkCreateRenderPass(gd->vkDevice, &rp_info, nullptr, &gd->vkRenderPass));
        }

        CreateGraphicsPipeline(
            gd->vkDevice, vertModule, fragModule, gd->pipelineLayout, gd->vkRenderPass, &gd->gameShader);
    }

    gd->cam.trans.scale              = ae::math::vec3_t(1.0f, 1.0f, 1.0f);
    gd->cam.fov                      = 90.0f;
    gd->cam.nearPlane                = 0.01f;
    gd->cam.farPlane                 = 1000.0f;
    gd->cam.width                    = winInfo.width;
    gd->cam.height                   = winInfo.height;
    gd->suzanneTransform.scale       = ae::math::vec3_t(1.0f, 1.0f, 1.0f);
    gd->suzanneTransform.pos         = ae::math::vec3_t(0.0f, 0.0f, -3.0f);
    gd->suzanneTransform.eulerAngles = {};

    uint32_t uploadHeapIdx = ae::VK::getDesiredMemoryTypeIndex(gd->vkGpu,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
        VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT | VK_MEMORY_PROPERTY_PROTECTED_BIT);

    uint32_t vramHeapIdx = ae::VK::getDesiredMemoryTypeIndex(
        gd->vkGpu, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

    // create THE command buffer.
    // NOTE: since this app is using the atomic update model, only one frame will ever be in flight at once.
    // so, we can literally get away with using just a single command buffer.
    {
        auto poolInfo = ae::VK::commandPoolCreateInfo(gd->gfxQueueIndex);
        ae::VK_CHECK(vkCreateCommandPool(gd->vkDevice, &poolInfo, nullptr, &gd->commandPool));
        auto cmdInfo = ae::VK::commandBufferAllocateInfo(1, gd->commandPool);
        ae::VK_CHECK(vkAllocateCommandBuffers(gd->vkDevice, &cmdInfo, &gd->commandBuffer));
    }

    VkCommandBuffer cmd = gd->commandBuffer;

    auto writeUploadImage = [&](uint32_t          whichRes,
                                u32               width,
                                u32               height,
                                void             *resData,
                                VkImageUsageFlags usage,
                                VkImage          *image,
                                VkDeviceMemory   *backing) {
        assert(whichRes < _countof(g_uploadResources));
        auto &res = g_uploadResources[whichRes];

        void *data;

        size_t resSize = width * height * sizeof(u32);

        ae::VK::createUploadBufferDumb(gd->vkDevice,
            resSize,
            uploadHeapIdx,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            &res.uploadBuffer,
            &res.uploadBufferBacking,
            &data);

        if (data) {
            memcpy(data, resData, resSize);
            ae::VK::flushAndUnmapUploadBuffer(gd->vkDevice, resSize, res.uploadBufferBacking);
        } else {
            // TODO: log?
            ae::setFatalExit();
            return;
        }

        // NOTE: the *UNORM forms read as "unsigned normalized". so if it is a UNORM float format,
        // then the format stores colors as 0.0 -> 1.0.
        ae::VK::createImage2D_dumb(gd->vkDevice,
            width,
            height,
            vramHeapIdx,
            VK_FORMAT_R8G8B8A8_UINT,
            usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            image,
            backing);

        res.image       = *image;
        res.backing     = *backing;
        res.dims.width  = width;
        res.dims.height = height;
    };

    auto writeUploadBuffer = [&](uint32_t           whichRes,
                                 size_t             resSize,
                                 void              *resData,
                                 VkBufferUsageFlags usage,
                                 VkBuffer          *buffer,
                                 VkDeviceMemory    *backing) {
        assert(whichRes < _countof(g_uploadResources));
        auto &res = g_uploadResources[whichRes];

        void *data;
        ae::VK::createUploadBufferDumb(gd->vkDevice,
            resSize,
            uploadHeapIdx,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            &res.uploadBuffer,
            &res.uploadBufferBacking,
            &data);
        if (data) {
            memcpy(data, resData, resSize);
            ae::VK::flushAndUnmapUploadBuffer(gd->vkDevice, resSize, res.uploadBufferBacking);
        } else {
            // TODO: log?
            ae::setFatalExit();
            return;
        }

        // create the actual one.
        ae::VK::createBufferDumb(
            gd->vkDevice, resSize, vramHeapIdx, usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT, buffer, backing);
        res.size    = resSize;
        res.buffer  = *buffer;
        res.backing = *backing;
    };

    // load the checker data and upload to GPU.
    {
        loaded_image_t bitmap = ae::io::loadBMP("res\\highres_checker.bmp");

        if (bitmap.pixelPointer == nullptr) {
            ae::setFatalExit();
            return;
        }

        writeUploadImage(0,
            bitmap.width,
            bitmap.height,
            bitmap.pixelPointer,
            VK_IMAGE_USAGE_SAMPLED_BIT,
            &gd->checkerTexture,
            &gd->checkerTextureBacking);

        ae::io::freeLoadedImage(bitmap);
    }

    // load the vertex and index buffers to the GPU.
    {
        gd->suzanne = ae::io::loadObj("res\\monke.obj");
        if (gd->suzanne.vertexData == nullptr) {
            ae::setFatalExit();
            return;
        }

        size_t resSize = StretchyBufferCount(gd->suzanne.vertexData) * sizeof(float);

        gd->suzanneIndexCount = StretchyBufferCount(gd->suzanne.indexData);

        writeUploadBuffer(1,
            resSize,
            gd->suzanne.vertexData,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            &gd->suzanneVbo,
            &gd->suzanneVboBacking);

        // TODO: see if we can make the indices u16.
        size_t resSize2 = StretchyBufferCount(gd->suzanne.indexData) * sizeof(u32);
        writeUploadBuffer(2,
            resSize2,
            gd->suzanne.indexData,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            &gd->suzanneIbo,
            &gd->suzanneIboBacking);

        ae::io::freeObj(gd->suzanne);
    }

    // record the uploads.
    ae::VK::beginCommandBuffer(cmd);
    for (u32 it_index = 0; it_index < _countof(g_uploadResources); it_index++) {
        auto &it = g_uploadResources[it_index];

        if (it.buffer != VK_NULL_HANDLE) {
            VkBufferCopy region = {0, 0, it.size};
            vkCmdCopyBuffer(cmd, it.uploadBuffer, it.buffer, 1, &region);
        } else {
            auto barrierInfo = ae::VK::imageMemoryBarrier(VK_ACCESS_NONE,
                VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                gd->checkerTexture);

            ae::VK::cmdImageMemoryBarrier(cmd,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,  // no stage before.
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                1,
                &barrierInfo);

            VkBufferImageCopy region = {0,
                0,
                0,
                VkImageSubresourceLayers{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                VkOffset3D{},
                VkExtent3D{it.dims.width, it.dims.height, 1}};
            vkCmdCopyBufferToImage(cmd, it.uploadBuffer, it.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        }
    }
    vkEndCommandBuffer(cmd);

    // create the init fence.
    VkFenceCreateInfo ci = {};
    ci.sType             = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    ae::VK_CHECK(vkCreateFence(gd->vkDevice, &ci, nullptr, &gd->vkInitFence));

    // submit the init command list.
    auto &fence = gd->vkInitFence;

    VkSubmitInfo si       = {};
    si.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers    = &cmd;
    (vkQueueSubmit(gd->vkQueue, 1, &si, fence));

    // TODO: for now we are waiting for this just because the async init stuff
    // could get messy.
    WaitForAndResetFence(gd->vkDevice, &fence);

    ae::bifrost::registerApp("spinning_monkey", GameUpdateAndRender);
    ae::setUpdateModel(AUTOMATA_ENGINE_UPDATE_MODEL_ATOMIC);
    ae::platform::setVsync(true);
}

void ae::InitAsync(game_memory_t *gameMemory)
{
    game_state_t *gd = getGameState(gameMemory);

    // wait for the init command list.

    ae::VK_CHECK(vkResetCommandBuffer(gd->commandBuffer, 0));
    ae::VK_CHECK(vkResetCommandPool(gd->vkDevice, gd->commandPool, 0));

    // now we can destroy the upload resources. TODO.
    // TODO: can we get rid of the globals? why do people say that globals are bad tho?

    // and only after then mark the game ready to go.
    gameMemory->setInitialized(true);
}

// TODO: would be nice if it wasn't a requirement to do this thing. and instead we could
// just be like, "we aren't using sound stuff."
void ae::OnVoiceBufferEnd(game_memory_t *gameMemory, intptr_t voiceHandle) {}
void ae::OnVoiceBufferProcess(game_memory_t *gameMemory,
    intptr_t                                 voiceHandle,
    float                                   *dst,
    float                                   *src,
    uint32_t                                 samplesToWrite,
    int                                      channels,
    int                                      bytesPerSample)
{}

VkFramebuffer MaybeMakeFramebuffer(game_state_t *gd, VkImageView view, u32 width, u32 height, uint32_t idx)
{
    assert(idx < _countof(gd->vkFramebufferCache));
    if (!gd->vkFramebufferCache[idx]) {
        VkFramebufferCreateInfo ci = {};
        ci.sType                   = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        ci.renderPass              = gd->vkRenderPass;
        ci.attachmentCount         = 1;
        ci.pAttachments            = &view;
        ci.width                   = width;
        ci.height                  = height;
        ci.layers                  = 1;
        vkCreateFramebuffer(gd->vkDevice, &ci, nullptr, &gd->vkFramebufferCache[idx]);
    }
    return gd->vkFramebufferCache[idx];
}

void GameUpdateAndRender(ae::game_memory_t *gameMemory)
{
    auto             winInfo   = ae::platform::getWindowInfo();
    game_state_t    *gd        = getGameState(gameMemory);
    ae::user_input_t userInput = {};
    ae::platform::getUserInput(&userInput);
    float speed = 5 / 60.0f;

    ae::math::mat3_t camBasis =
        ae::math::mat3_t(buildRotMat4(ae::math::vec3_t(0.0f, gd->cam.trans.eulerAngles.y, 0.0f)));
    if (userInput.keyDown[ae::GAME_KEY_W]) {
        // NOTE(Noah): rotation matrix for a camera happens to also be the basis vectors
        // defining the space for any children of the camera.
        // ...
        // we also note that as per a game-feel thing, WASD movement is only along cam
        // local axis by Y rot. Ignore X and Z rot.
        gd->cam.trans.pos += camBasis * ae::math::vec3_t(0.0f, 0.0f, -speed);
    }
    if (userInput.keyDown[ae::GAME_KEY_A]) { gd->cam.trans.pos += camBasis * ae::math::vec3_t(-speed, 0.0f, 0.0f); }
    if (userInput.keyDown[ae::GAME_KEY_S]) { gd->cam.trans.pos += camBasis * ae::math::vec3_t(0.0f, 0.0f, speed); }
    if (userInput.keyDown[ae::GAME_KEY_D]) { gd->cam.trans.pos += camBasis * ae::math::vec3_t(speed, 0.0f, 0.0f); }
    // NOTE(Noah): up and down movement go in world space. This is a game-feel thing.
    if (userInput.keyDown[ae::GAME_KEY_SHIFT]) { gd->cam.trans.pos += ae::math::vec3_t(0.0f, -speed, 0.0f); }
    if (userInput.keyDown[ae::GAME_KEY_SPACE]) { gd->cam.trans.pos += ae::math::vec3_t(0.0f, speed, 0.0f); }
    if (userInput.mouseLBttnDown) {
        // TODO: why is this ae::platform for the lastFrameTimeTotal?
        gd->cam.trans.eulerAngles +=
            ae::math::vec3_t(0.0f, (float)userInput.deltaMouseX / 5 * ae::platform::lastFrameTimeTotal, 0.0f);
        gd->cam.trans.eulerAngles +=
            ae::math::vec3_t(-(float)userInput.deltaMouseY / 5 * ae::platform::lastFrameTimeTotal, 0.0f, 0.0f);
    }
    gd->suzanneTransform.eulerAngles += ae::math::vec3_t(0.0f, 2.0f * ae::platform::lastFrameTimeTotal, 0.0f);

    // TODO: look into the depth testing stuff more deeply on the hardware side of things.
    // what is something that we can only do because we really get it?

    VkCommandBuffer cmd = gd->commandBuffer;

    // reset the command buffer and allocator from last frame, which we know is okay due to atomic
    // update model.
    ae::VK_CHECK(vkResetCommandBuffer(gd->commandBuffer, 0));
    ae::VK_CHECK(vkResetCommandPool(gd->vkDevice, gd->commandPool, 0));

    // main render loop.
    {
        ae::VK::beginCommandBuffer(cmd);

        VkImageView backbufferView;
        VkImage     backbuffer;
        uint32_t    backbufferIdx = ae::VK::getCurrentBackbuffer(&backbuffer, &backbufferView);

        // NOTE: since the window cannot be resized, the swapchain will never be resized.
        // for this reason, it is safe to rely that VkImageView is the same view across the entire
        // app lifetime.
        VkFramebuffer framebuffer =
            MaybeMakeFramebuffer(gd, backbufferView, winInfo.width, winInfo.height, backbufferIdx);

        // NOTE: the barriers are special casing the fact that we have the atomic update model.
        // so, we don't need to sync against any work before.
        // and we don't need to sync against any work after.

        // transit from present to color attachment.
        auto barrierInfo = ae::VK::imageMemoryBarrier(VK_ACCESS_NONE,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            backbuffer);

        ae::VK::cmdImageMemoryBarrier(cmd,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,   // no stage before.
            VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,  // includes load op.
            1,
            &barrierInfo);

        {
            VkRect2D renderArea = {VkOffset2D{0, 0}, VkExtent2D{winInfo.width, winInfo.height}};

            VkClearValue clearValue;
            clearValue.color.float32[0] = 1.f;
            clearValue.color.float32[1] = 1.f;
            clearValue.color.float32[2] = 0.f;
            clearValue.color.float32[3] = 1.f;

            // Begin the render pass.
            VkRenderPassBeginInfo rp_begin = {};
            rp_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            rp_begin.renderPass  = gd->vkRenderPass;
            rp_begin.framebuffer = framebuffer;
            rp_begin.renderArea            = renderArea;
            rp_begin.clearValueCount       = 1;
            rp_begin.pClearValues          = &clearValue;
            
            vkCmdBeginRenderPass(cmd, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);
            defer(vkCmdEndRenderPass(cmd));

            VkViewport viewport = {0.0f, 0.0f, winInfo.width * 1.0f, winInfo.height * 1.0f, 0.0f, 1.0f};
            VkRect2D   scissor  = renderArea;
            vkCmdSetViewport(cmd, 0, 1, &viewport);
            vkCmdSetScissor(cmd, 0, 1, &scissor);

            VkDeviceSize verticesOffsetInBuffer = 0;
            vkCmdBindVertexBuffers(cmd,
                0,  // firstBinding,
                1,  // bindingCount,
                &gd->suzanneVbo,
                &verticesOffsetInBuffer);

            VkDeviceSize indicesOffsetInBuffer = 0;
            vkCmdBindIndexBuffer(cmd, gd->suzanneIbo, indicesOffsetInBuffer, VK_INDEX_TYPE_UINT32);

            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, gd->gameShader);

            struct {
                ae::math::mat4_t model;
                ae::math::mat4_t projView;
            } pushData = {ae::math::buildMat4fFromTransform(gd->suzanneTransform),
                buildProjMatForVk(gd->cam) * buildViewMat(gd->cam)};

            vkCmdPushConstants(cmd, gd->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pushData), &pushData);

            vkCmdDrawIndexed(cmd, gd->suzanneIndexCount, 1, 0, 0, 0);

        }  // end render pass.

        // transit backbuffer from color attachment to present.
        auto barrierInfo2 = ae::VK::imageMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_ACCESS_MEMORY_READ_BIT,  // ensure cache flush.
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            backbuffer);

        ae::VK::cmdImageMemoryBarrier(
            cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 1, &barrierInfo2);

        vkEndCommandBuffer(cmd);
    }

    // NOTE: the engine expects that we architect our frame such that all work for the frame is known to
    // be complete one we signal this fence.
    auto pFence = ae::VK::getFrameEndFence();

    VkSubmitInfo si       = {};
    si.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers    = &cmd;
    (vkQueueSubmit(gd->vkQueue, 1, &si, *pFence));

#if !defined(AUTOMATA_ENGINE_DISABLE_IMGUI)
    ImGui::Begin("MonkeyDemo");

    //  ImGui::Text("tris / faces: %d", gd->suzanneIbo.count / 3);
    ImGui::Text("verts: %f", StretchyBufferCount(gd->suzanne.vertexData) / 8.f);

    ImGui::Text("");

    ae::ImGuiRenderMat4("camProjMat", buildProjMat(gd->cam));
    ae::ImGuiRenderMat4("camViewMat", buildViewMat(gd->cam));
    ae::ImGuiRenderMat4((char *)(std::string(gd->suzanne.modelName) + "Mat").c_str(),
        ae::math::buildMat4fFromTransform(gd->suzanneTransform));
    ae::ImGuiRenderVec3("camPos", gd->cam.trans.pos);
    ae::ImGuiRenderVec3((char *)(std::string(gd->suzanne.modelName) + "Pos").c_str(), gd->suzanneTransform.pos);
    ImGui::Text(
        "Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGui::End();
#endif
}

void ae::Close(game_memory_t *gameMemory) { game_state_t *gd = getGameState(gameMemory); }

// TODO: see if we can get this into the "engine". the idea is that we can see the need to expose
// for stuff like set depth. set blend. the vertex input. many of these params should be exposed.
//
// it looks like we might be able to use the default struct idea.
//
// init the pipeline
void CreateGraphicsPipeline(VkDevice vkDevice,
    VkShaderModule                   vertShader,
    VkShaderModule                   fragShader,
    VkPipelineLayout                 pipelineLayout,
    VkRenderPass                     renderPass,
    VkPipeline                      *pPipeline)
{
    VkVertexInputAttributeDescription vertex_input_attr_desc[3];
    vertex_input_attr_desc[0].location = 0;
    vertex_input_attr_desc[0].binding  = 0;  // slot 0.
    vertex_input_attr_desc[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
    vertex_input_attr_desc[0].offset   = 0;  // offset in vertex buf with which to look for elems.

    vertex_input_attr_desc[1].location = 1;  // in shader.
    vertex_input_attr_desc[1].binding  = 0;
    vertex_input_attr_desc[1].format   = VK_FORMAT_R32G32_SFLOAT;
    vertex_input_attr_desc[1].offset   = sizeof(float) * 3;

    vertex_input_attr_desc[2].location = 2;  // in shader.
    vertex_input_attr_desc[2].binding  = 0;
    vertex_input_attr_desc[2].format   = VK_FORMAT_R32G32B32_SFLOAT;
    vertex_input_attr_desc[2].offset   = sizeof(float) * 5;

    VkPipelineVertexInputStateCreateInfo vertex_input = {};
    vertex_input.sType                                = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkVertexInputBindingDescription vertex_input_binding_desc[1];
    vertex_input_binding_desc[0].binding   = 0;               // this structure describes slot 0.
    vertex_input_binding_desc[0].stride    = sizeof(Vertex);  // to get to next vertex.
    vertex_input_binding_desc[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    vertex_input.vertexBindingDescriptionCount   = 1;
    vertex_input.pVertexBindingDescriptions      = vertex_input_binding_desc;
    vertex_input.vertexAttributeDescriptionCount = _countof(vertex_input_attr_desc);
    vertex_input.pVertexAttributeDescriptions    = vertex_input_attr_desc;

    // Specify we will use triangle lists to draw geometry.
    VkPipelineInputAssemblyStateCreateInfo input_assembly = {};
    input_assembly.sType                                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology                               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    // Specify rasterization state.
    VkPipelineRasterizationStateCreateInfo raster = {};
    raster.sType                                  = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.cullMode                               = VK_CULL_MODE_NONE;  // TODO: temp.
    raster.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;   // NOTE: how our .OBJ is. also default in GL.
    raster.lineWidth               = 1.0f;
    raster.rasterizerDiscardEnable = VK_FALSE;

    // Our attachment will write to all color channels, but no blending is enabled.
    VkPipelineColorBlendAttachmentState blend_attachment = {};
    blend_attachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    // blend infos.
    VkPipelineColorBlendStateCreateInfo blend = {};
    blend.sType                               = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend.attachmentCount                     = 1;
    blend.pAttachments                        = &blend_attachment;

    // We will have one viewport and scissor box.
    VkPipelineViewportStateCreateInfo viewport = {};
    viewport.sType                             = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount                     = 1;
    viewport.scissorCount                      = 1;

    // Disable all depth testing.
    VkPipelineDepthStencilStateCreateInfo depth_stencil = {};
    depth_stencil.sType                                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil.depthTestEnable                       = VK_FALSE;
    depth_stencil.stencilTestEnable                     = VK_FALSE;
    depth_stencil.depthBoundsTestEnable                 = VK_FALSE;

    // No multisampling.
    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType                                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples                 = VK_SAMPLE_COUNT_1_BIT;

    // Specify that these states will be dynamic, i.e. not part of pipeline state object.
    VkDynamicState                   dynamics[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic     = {};
    dynamic.sType                                = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.pDynamicStates                       = dynamics;
    dynamic.dynamicStateCount                    = _countof(dynamics);

    VkPipelineShaderStageCreateInfo shader_stages[2] = {};
    // Vertex stage of the pipeline
    shader_stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    shader_stages[0].module = vertShader;
    shader_stages[0].pName  = "main";  // TODO.
    // Fragment stage of the pipeline
    shader_stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    shader_stages[1].module = fragShader;
    shader_stages[1].pName  = "main";  // TODO.

    VkGraphicsPipelineCreateInfo pipe = {};
    pipe.sType                        = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipe.stageCount                   = _countof(shader_stages);
    pipe.pStages                      = shader_stages;
    pipe.pVertexInputState            = &vertex_input;
    pipe.pInputAssemblyState          = &input_assembly;
    pipe.pRasterizationState          = &raster;
    pipe.pColorBlendState             = &blend;
    pipe.pMultisampleState            = &multisample;
    pipe.pViewportState               = &viewport;
    pipe.pDepthStencilState           = &depth_stencil;
    pipe.pDynamicState                = &dynamic;
    // NOTE: no need since dynamic rendering.
    pipe.renderPass = renderPass;
    pipe.layout     = pipelineLayout;

    ae::VK_CHECK(vkCreateGraphicsPipelines(vkDevice, VK_NULL_HANDLE, 1, &pipe, nullptr, pPipeline));

    // no longer needed since PSO is self-contained.
    vkDestroyShaderModule(vkDevice, shader_stages[0].module, nullptr);
    vkDestroyShaderModule(vkDevice, shader_stages[1].module, nullptr);
};

void WaitForAndResetFence(VkDevice device, VkFence *pFence, uint64_t waitTime)
{
    VkResult result = (vkWaitForFences(device,
        1,
        pFence,
        // wait until all the fences are signaled. this blocks the CPU thread.
        VK_TRUE,
        waitTime));
    // TODO: there was an interesting bug where if I went 1ms on the timeout, things were failing,
    // where the fence was reset too soon. figure out what what going on in that case.

    if (result == VK_SUCCESS) {
        // reset fences back to unsignaled so that can use em' again;
        vkResetFences(device, 1, pFence);
    } else {
        AELoggerError("some error occurred during the fence wait thing., %s", ae::VK::VkResultToString(result));
    }
}
