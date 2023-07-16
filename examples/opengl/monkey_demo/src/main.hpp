typedef struct game_state {
uint32_t suzanneIndexCount;
    bool     debugRenderDepthFlag;

  ae::raw_model_t suzanne;
  GLuint gameShader;
  ae::GL::ibo_t suzanneIbo;
  ae::GL::vbo_t suzanneVbo;
  ae::math::transform_t suzanneTransform;
  GLuint suzanneVao;
  GLuint checkerTexture;
  ae::math::camera_t cam;
} game_state_t;