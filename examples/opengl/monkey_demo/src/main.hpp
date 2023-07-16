typedef struct game_state {

    uint32_t suzanneIndexCount;
    bool     debugRenderDepthFlag;

    bool             bSpin;
    bool             lockCamYaw;
    bool             lockCamPitch;
    float            ambientStrength;
    float            specularStrength;
    ae::math::vec4_t lightColor;
    ae::math::vec3_t lightPos;
    float            cameraSensitivity;
    bool             optInFirstPersonCam;

    bool bFocusedLastFrame;
    float lastDeltaX[2];
    float lastDeltaY[2];


  ae::raw_model_t suzanne;
  GLuint gameShader;
  ae::GL::ibo_t suzanneIbo;
  ae::GL::vbo_t suzanneVbo;
  ae::math::transform_t suzanneTransform;

  // TODO: we could have a game_state_private_t struct for those elements of this thing
  // that are only for whatever backend X. that way there can be maximum reuse for the other
  // common elements of this struct.
  GLuint suzanneVao;
  GLuint checkerTexture;
  ae::math::camera_t cam;
} game_state_t;