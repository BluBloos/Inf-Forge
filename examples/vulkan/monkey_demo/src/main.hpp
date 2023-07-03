typedef uint32_t u32;

void GameUpdateAndRender(ae::game_memory_t *gameMemory);

void WaitForAndResetFence(VkDevice device, VkFence *pFence, uint64_t waitTime = 1000 * 1000 * 1000);

typedef struct game_state {
    ae::raw_model_t suzanne;

    VkPhysicalDevice vkGpu;
    VkDevice         vkDevice;
    VkInstance       vkInstance;

    VkDebugUtilsMessengerEXT vkDebugCallback;

    uint32_t        gfxQueueIndex;
    VkQueue         vkQueue;
    VkCommandBuffer commandBuffer;
    VkCommandPool   commandPool;

    VkFence vkInitFence;

    VkDescriptorSetLayout setLayout;
    VkPipelineLayout      pipelineLayout;
    VkPipeline            gameShader;

    VkDescriptorPool descPool;
    VkDescriptorSet  theDescSet;

    // TODO: dyn rendering prefer.
    VkRenderPass vkRenderPass;  

    // NOTE: 10 is certainly larger than swapchain count.
    VkFramebuffer vkFramebufferCache[10] = {};

    VkImageView    depthBufferView;
    VkImage        depthBuffer;
    VkDeviceMemory depthBufferBacking;

    VkImageView    checkerTextureView;
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
