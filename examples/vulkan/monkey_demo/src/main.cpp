#include <automata_engine.hpp>

#include <main.hpp>
#include "../../shared/monkey_demo.hpp"

static struct {
    VkBuffer       uploadBuffer;
    VkDeviceMemory uploadBufferBacking;
    VkBuffer       buffer = VK_NULL_HANDLE;
    VkImage        image  = VK_NULL_HANDLE;
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

    *gd = {};  // zero it out.

    MonkeyDemoInit(gameMemory);

    ae::VK::doDefaultInit(
        &gd->vkInstance, &gd->vkGpu, &gd->vkDevice, &gd->vkQueue, & gd->gfxQueueIndex, &gd->vkDebugCallback);

    // NOTE: init the VK resources on the engine side. this lets it know what queue to wait for work
    // and to present on. at this time, the engine also initializes the swapchain for the first time.
    ae::platform::VK::init(gd->vkInstance, gd->vkGpu, gd->vkDevice, gd->vkQueue, gd->gfxQueueIndex);

    const VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;

    // create the pipeline.
    {
        // TODO: review in detail https://registry.khronos.org/vulkan/specs/1.3-extensions/html/vkspec.html#textures.
        // how is the image precisely sampled?
        auto samplerInfo = ae::VK::samplerCreateInfo();
        ae::VK_CHECK(vkCreateSampler(gd->vkDevice, &samplerInfo, nullptr, &gd->sampler));

        VkDescriptorSetLayoutBinding bindings[] = {{.binding            = 0,
                                                       .descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                                       .descriptorCount = 1,
                                                       .stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT},
            {.binding               = 1,
                .descriptorType     = VK_DESCRIPTOR_TYPE_SAMPLER,
                .descriptorCount    = 1,
                .stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT,
                .pImmutableSamplers = &gd->sampler},
            {.binding            = 2,
                .descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 1,
                .stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT}};

        auto setLayoutInfo = ae::VK::createDescriptorSetLayout(_countof(bindings), bindings);
        ae::VK_CHECK(vkCreateDescriptorSetLayout(gd->vkDevice, &setLayoutInfo, nullptr, &gd->setLayout));

        /*VkPushConstantRange pushRange = {
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT, .offset = 0, .size = sizeof(PushDataVertex)};
        */
        auto pipelineLayoutInfo =
            ae::VK::createPipelineLayout(1, &gd->setLayout);  //.pushConstantRanges(1, &pushRange);
        ae::VK_CHECK(vkCreatePipelineLayout(gd->vkDevice, &pipelineLayoutInfo, nullptr, &gd->pipelineLayout));

        // create the render pass
        {
            VkAttachmentDescription attachments[2] = {{
                                                          .format        = ae::VK::getSwapchainFormat(),
                                                          .samples       = VK_SAMPLE_COUNT_1_BIT,
                                                          .loadOp        = VK_ATTACHMENT_LOAD_OP_CLEAR,
                                                          .storeOp       = VK_ATTACHMENT_STORE_OP_STORE,
                                                          .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                                          .finalLayout   = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                                      },
                {
                    .format  = depthFormat,
                    .samples = VK_SAMPLE_COUNT_1_BIT,
                    .loadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    // NOTE: we don't actually care about what the depth buffer is after the pass.
                    // we literally just want it to exist during the pass for the purpose of
                    // depth test.
                    .storeOp       = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                    .initialLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                    .finalLayout   = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                }};

            VkAttachmentReference color_ref = {.attachment = 0, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
            VkAttachmentReference depth_ref = {.attachment = 1, .layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL};

            VkSubpassDescription subpass = {.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                .colorAttachmentCount                          = 1,
                .pColorAttachments                             = &color_ref,
                .pDepthStencilAttachment                       = &depth_ref};

            auto rpInfo = ae::VK::createRenderPass(_countof(attachments), attachments, 1, &subpass);
            VK_CHECK(vkCreateRenderPass(gd->vkDevice, &rpInfo, nullptr, &gd->vkRenderPass));
        }

        auto vertModule = ae::VK::loadShaderModule(gd->vkDevice, "res\\vert.hlsl", L"main", L"vs_6_0");
        auto fragModule = ae::VK::loadShaderModule(gd->vkDevice, "res\\frag.hlsl", L"main", L"ps_6_0");
        // NOTE: pipline is self-contained after create.
        defer(vkDestroyShaderModule(gd->vkDevice, vertModule, nullptr));
        defer(vkDestroyShaderModule(gd->vkDevice, fragModule, nullptr));

        auto pipelineInfo = ae::VK::createGraphicsPipeline(
            vertModule, fragModule, "main", "main", gd->pipelineLayout, gd->vkRenderPass);
        ae::VK_CHECK(
            vkCreateGraphicsPipelines(gd->vkDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &gd->gameShader));
    }

    uint32_t uploadHeapIdx = ae::VK::getDesiredMemoryTypeIndex(gd->vkGpu,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
        VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT | VK_MEMORY_PROPERTY_PROTECTED_BIT);

    uint32_t vramHeapIdx = ae::VK::getDesiredMemoryTypeIndex(
        gd->vkGpu, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

    // create the depth buffer.
    {
        ae::VK::createImage2D_dumb(gd->vkDevice,
            winInfo.width,
            winInfo.height,
            vramHeapIdx,
            depthFormat,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            &gd->depthBuffer,
            &gd->depthBufferBacking);

        // create image view.
        auto viewInfo = ae::VK::createImageView(gd->depthBuffer, depthFormat).aspectMask(VK_IMAGE_ASPECT_DEPTH_BIT);
        vkCreateImageView(gd->vkDevice, &viewInfo, nullptr, &gd->depthBufferView);
    }

    // create the dynamic frame ubo.
    {
        // NOTE: we never unmap this. that is valid VK usage. there would be a cost to remap per frame.
        ae::VK::createUploadBufferDumb(gd->vkDevice,
            sizeof(PushData),
            uploadHeapIdx, // NOTE: don't need coherent heap because we will use the right barriers per frame to flush/invalidate.
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            &gd->dynamicFrameUbo,
            &gd->dynamicFrameUboBacking,
            &gd->dynamicFrameUboMapped);
    }

    // create THE command buffer (plus imgui command buffer).
    // NOTE: since this app is using the atomic update model, only one frame will ever be in flight at once.
    // so, we can literally get away with using just a single command buffer.
    {
        auto poolInfo = ae::VK::commandPoolCreateInfo(gd->gfxQueueIndex);
        ae::VK_CHECK(vkCreateCommandPool(gd->vkDevice, &poolInfo, nullptr, &gd->commandPool));
        auto cmdInfo = ae::VK::commandBufferAllocateInfo(1, gd->commandPool);
        ae::VK_CHECK(vkAllocateCommandBuffers(gd->vkDevice, &cmdInfo, &gd->commandBuffer));

        auto im_poolInfo = ae::VK::commandPoolCreateInfo(gd->gfxQueueIndex);
        ae::VK_CHECK(vkCreateCommandPool(gd->vkDevice, &im_poolInfo, nullptr, &gd->commandPool));
        auto im_cmdInfo = ae::VK::commandBufferAllocateInfo(1, gd->commandPool);
        ae::VK_CHECK(vkAllocateCommandBuffers(gd->vkDevice, &im_cmdInfo, &gd->imgui_commandBuffer));
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
            AELoggerError("unable to map upload buffer.");
            ae::setFatalExit();
            return;
        }

        // NOTE: the *UNORM forms read as "unsigned normalized". this is a float format where the
        // values go between 0.0 -> 1.0. SNORM is -1.0 -> 1.0.
        auto imageInfo =
            ae::VK::createImage(width, height, VK_FORMAT_R8G8B8A8_UINT, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT)
                .flags(VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT);

        ae::VK::createImage_dumb(gd->vkDevice, vramHeapIdx, imageInfo, image, backing);

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
            AELoggerError("unable to map upload buffer.");
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

    // create the checker texture view.
    {
        auto viewInfo = ae::VK::createImageView(gd->checkerTexture, VK_FORMAT_R8G8B8A8_UNORM);
        vkCreateImageView(gd->vkDevice, &viewInfo, nullptr, &gd->checkerTextureView);
    }

    // now that we have the checker image, we can create the descriptor for it.
    {
        // begin by create the pool.
        VkDescriptorPoolSize pools[] = {{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 2}, {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1}};

        VkDescriptorPoolCreateInfo ci = {};
        ci.sType                      = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        ci.maxSets                    = 1;
        ci.poolSizeCount              = _countof(pools);
        ci.pPoolSizes                 = pools;
        VK_CHECK(vkCreateDescriptorPool(gd->vkDevice, &ci, nullptr, &gd->descPool));

        VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool              = gd->descPool; // to allocate from.
        allocInfo.descriptorSetCount          = 1;
        allocInfo.pSetLayouts                 = &gd->setLayout;

        VK_CHECK(vkAllocateDescriptorSets(gd->vkDevice, &allocInfo, &gd->theDescSet));

        VkDescriptorImageInfo imageInfo = {
            VK_NULL_HANDLE,  // sampler.
            gd->checkerTextureView,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL  // layout at the time of access through this descriptor.
        };

        VkDescriptorBufferInfo bufferInfo = {.buffer = gd->dynamicFrameUbo, .offset = 0, .range = sizeof(PushData)};

        VkWriteDescriptorSet writes[] = {
            {.sType              = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet          = gd->theDescSet,
                .dstBinding      = 0,
                .descriptorCount = 1,
                .descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                .pImageInfo      = &imageInfo},
            {
                .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet          = gd->theDescSet,
                .dstBinding      = 2,
                .descriptorCount = 1,
                .descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .pBufferInfo     = &bufferInfo,
            },
        };

        ae::VK::updateDescriptorSets(gd->vkDevice, _countof(writes), writes);
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

    // issue the barriers to set some inital layouts.
    {
        auto barrierInfo = ae::VK::imageMemoryBarrier(VK_ACCESS_NONE,
            VK_ACCESS_MEMORY_WRITE_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
            gd->depthBuffer)
                               .aspectMask(VK_IMAGE_ASPECT_DEPTH_BIT);

        ae::VK::cmdImageMemoryBarrier(cmd,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,     // no stage before.
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,  // no stage after.
            1,
            &barrierInfo);

        auto barrierInfo2 = ae::VK::imageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            gd->checkerTexture);

        ae::VK::cmdImageMemoryBarrier(cmd,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,  // no stage after.
            1,
            &barrierInfo2);
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
}

void ae::InitAsync(game_memory_t *gameMemory)
{
    game_state_t *gd = getGameState(gameMemory);

    // wait for the init command list.
    auto &fence = gd->vkInitFence;
    WaitForAndResetFence(gd->vkDevice, &fence);

    ae::VK_CHECK(vkResetCommandBuffer(gd->commandBuffer, 0));
    ae::VK_CHECK(vkResetCommandPool(gd->vkDevice, gd->commandPool, 0));

    // TODO: destroy the upload resources.

    // game is ready to go now.
    gameMemory->setInitialized(true);
}

VkFramebuffer MaybeMakeFramebuffer(game_state_t *gd, VkImageView backbuffer, u32 width, u32 height, uint32_t idx)
{
    assert(idx < _countof(gd->vkFramebufferCache));
    if (!gd->vkFramebufferCache[idx]) {

        // NOTE: since we use the atomic update model, we can get away with using just one
        // depth buffer view for both framebuffer objects. these will never be in flight
        // at the same time.
        VkImageView views[2] = {backbuffer, gd->depthBufferView};

        VkFramebufferCreateInfo ci = {};
        ci.sType                   = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        ci.renderPass              = gd->vkRenderPass;
        ci.attachmentCount         = _countof(views);
        ci.pAttachments            = views;
        ci.width                   = width;
        ci.height                  = height;
        ci.layers                  = 1;
        vkCreateFramebuffer(gd->vkDevice, &ci, nullptr, &gd->vkFramebufferCache[idx]);
        
    }
    return gd->vkFramebufferCache[idx];
}

void MonkeyDemoRender(ae::game_memory_t *gameMemory)
{
    auto             winInfo   = ae::platform::getWindowInfo();
    game_state_t    *gd        = getGameState(gameMemory);

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
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,              // no stage before.
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,  // includes load op.
            1,
            &barrierInfo);

        // TODO: we could batch the barriers.
        // put a barrier for the dynamic ubo write -> read.
        auto bufferBarrier = ae::VK::bufferMemoryBarrier(
            VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_UNIFORM_READ_BIT, gd->dynamicFrameUbo, 0, sizeof(PushData));
        ae::VK::cmdBufferMemoryBarrier(cmd,
            VK_PIPELINE_STAGE_HOST_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
            1,
            &bufferBarrier);

        {
            VkRect2D renderArea = {VkOffset2D{0, 0}, VkExtent2D{winInfo.width, winInfo.height}};

            VkClearValue clearValues[2]       = {};
            clearValues[0].color.float32[0]   = 1.f;
            clearValues[0].color.float32[1]   = 1.f;
            clearValues[0].color.float32[2]   = 0.f;
            clearValues[0].color.float32[3]   = 1.f;
            clearValues[1].depthStencil.depth = 1.f;

            // Begin the render pass.
            VkRenderPassBeginInfo rp_begin = {};
            rp_begin.sType                 = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            rp_begin.renderPass            = gd->vkRenderPass;
            rp_begin.framebuffer           = framebuffer;
            rp_begin.renderArea            = renderArea;
            rp_begin.clearValueCount       = _countof(clearValues);
            rp_begin.pClearValues          = clearValues;
            
            vkCmdBeginRenderPass(cmd, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);
            defer(vkCmdEndRenderPass(cmd));

            // NOTE: need to flip Y like so for NDC space in VK.
            VkViewport viewport = {0.0f, winInfo.height, winInfo.width * 1.0f, winInfo.height * -1.0f, 0.0f, 1.0f};

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

            vkCmdBindDescriptorSets(cmd,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                gd->pipelineLayout,
                0,  // set number of first descriptor set to bind.
                1,  // number of sets to bind.
                &gd->theDescSet,
                0,       // dynamic offsets
                nullptr  // ^
            );

            // write the dynamic ubo.
            PushData pushData = {.modelMatrix = ae::math::buildMat4fFromTransform(gd->suzanneTransform),
                .modelRotate                  = ae::math::buildRotMat4(gd->suzanneTransform.eulerAngles),
                .projView                     = ae::math::buildProjMatForVk(gd->cam) * ae::math::buildViewMat(gd->cam),
                .lightColor                   = lightColor,
                .ambientStrength              = ambientStrength,
                .lightPos                     = lightPos,
                .specularStrength             = specularStrength,
                .viewPos                      = gd->cam.trans.pos};  // NOTE: LOL, this looks like JS.
            memcpy(gd->dynamicFrameUboMapped, &pushData, sizeof(PushData));

            vkCmdDrawIndexed(cmd, gd->suzanneIndexCount, 1, 0, 0, 0);

        }  // end render pass.

#if defined(AUTOMATA_ENGINE_DISABLE_IMGUI)
        // transit backbuffer from color attachment to present.
        auto barrierInfo2 = ae::VK::imageMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_ACCESS_MEMORY_READ_BIT,  // ensure cache flush.
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            backbuffer);

        ae::VK::cmdImageMemoryBarrier(
            cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 1, &barrierInfo2);
#endif

        vkEndCommandBuffer(cmd);
    }

    // NOTE: the engine expects that we architect our frame such that all work for the frame is known to
    // be complete one we signal this fence.
    auto pFence = ae::VK::getFrameEndFence();

    VkSubmitInfo si       = {};
    si.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers    = &cmd;

#if !defined(AUTOMATA_ENGINE_DISABLE_IMGUI)
    VkCommandBuffer ImguiCmd = gd->imgui_commandBuffer;
    ae::VK_CHECK(vkResetCommandBuffer(ImguiCmd, 0));

    ae::VK::renderAndRecordImGui(ImguiCmd);

    VkCommandBuffer cmds[] = {cmd, ImguiCmd};
    si.commandBufferCount  = 2;
    si.pCommandBuffers     = cmds;

#endif

    (vkQueueSubmit(gd->vkQueue, 1, &si, *pFence));

    // reset game state as the relative computation point for next frame.
    // the state we computed to use in rendering the frame is just the 60Hz sample.
    // but we need to retain the higher Hz sample.
    gd->cam.trans.pos         = s2_posVector;
    gd->cam.trans.eulerAngles = s2_eulerAngles;
}

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

void ae::Close(game_memory_t *gameMemory)
{
    // TODO: destroy VK resources.
}