typedef struct game_state {

  ae::GL::vbo_t cubeVbo;
  GLuint cubeVao;

  GLuint gBuffer;
  GLuint gNormal;
  GLuint gColor;
  GLuint gDepth;
  GLuint gDepthRay;
  GLuint gPos;

  GLuint gBuffer2;
  GLuint gUVs;

  GLuint gameShader;
  GLuint SSR_shader;
  GLuint lightingPass;
  GLuint debugRenderDepth;

  bool debugRenderDepthFlag;

  ae::raw_model_t suzanne;
  uint32_t suzanneIndexCount;
  ae::GL::ibo_t suzanneIbo;
  ae::GL::vbo_t suzanneVbo;
  ae::math::transform_t suzanneTransform;
  GLuint suzanneVao;

  ae::math::camera_t cam;

  GLuint checkerTexture;

} game_state_t;