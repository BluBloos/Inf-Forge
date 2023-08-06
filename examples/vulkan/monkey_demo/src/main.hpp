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

    VkCommandBuffer imgui_commandBuffer;
    //VkCommandPool   imgui_commandPool;

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

    VkBuffer       dynamicFrameUbo;
    VkDeviceMemory dynamicFrameUboBacking;
    void          *dynamicFrameUboMapped;

    VkSampler sampler;

    uint32_t              suzanneIndexCount;
    ae::math::transform_t suzanneTransform;
    VkBuffer              suzanneIbo;
    VkDeviceMemory        suzanneIboBacking;
    VkBuffer              suzanneVbo;
    VkDeviceMemory        suzanneVboBacking;

    std::atomic<float>            cameraSensitivity;
    std::atomic<bool>             lockCamYaw;
    std::atomic<bool>             lockCamPitch;

    bool             bSpin;
    float            ambientStrength;
    float            specularStrength;
    ae::math::vec4_t lightColor;
    ae::math::vec3_t lightPos;

    std::atomic<bool>               optInFirstPersonCam;
    std::atomic<ae::math::camera_t> cam;

    bool  bFocusedLastFrame;
    bool  lastFrameF5;

    float lastDeltaX[2];
    float lastDeltaY[2];
    
    bool debugRenderDepthFlag;

    
} game_state_t;

struct Vertex {
    float x, y, z;
    float u, v;
    float nx, ny, nz;
};

struct PushData {
    ae::math::mat4_t modelMatrix;
    ae::math::mat4_t modelRotate;
    ae::math::mat4_t projView;
    ae::math::vec4_t lightColor;
    float            ambientStrength;
    ae::math::vec3_t lightPos;
    float            specularStrength;
    ae::math::vec3_t viewPos;  // camera pos.
};
