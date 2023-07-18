void GameUpdateAndRender(ae::game_memory_t *gameMemory);

typedef struct game_state {
  GLuint computeShader;
  GLuint outputImage;
  GLenum outputFormat;
  GLuint outputImageFramebuffer;
} game_state_t;

game_state_t *getGameState(ae::game_memory_t *gameMemory);