#pragma once

// #include <automata_engine.h>

// I suppose we will use a combination of glm and our own routines.
// Isn't math fun?

// Oh, and, we'll probably want to make this code have some nice documentation.
// In fact, learning is always the most important part, isn't it?

#ifndef AUTOMATA_ENGINE_MATH
#define AUTOMATA_ENGINE_MATH

// TODO: Remove dependency on math.h and do transcendentals ourselves.
// From a learning perspective, this is a good idea.
#include <math.h>
#include <initializer_list>

#if !defined(PI)
#define PI 3.1415f
#endif
#define DEGREES_TO_RADIANS PI / 180.0f

namespace automata_engine {
  namespace math {
    #pragma pack(push, 4) // align on 4 bytes 
    typedef struct vec2 {
      float x,y;
    } vec2_t;
    typedef struct vec4 vec4_t;
    typedef struct vec3 {
      float x,y,z;
      constexpr vec3();
      vec3(float, float, float);
      vec3(vec4_t);
      vec3 operator-();
      float &operator[](int index);
    } vec3_t;
    typedef struct vec4 {
      float x,y,z,w;
      constexpr vec4();
      vec4 operator-();
      vec4(float, float, float, float);
      vec4(vec3_t, float);
      float &operator[](int index);
    } vec4_t;
    typedef struct mat4 mat4_t;
    typedef struct mat3 {
      union {
        float mat[3][3];
        float matp[9];
        vec3_t matv[3];
      };
      mat3(std::initializer_list<float>);
      mat3(mat4_t);
    } mat3_t;
    typedef struct mat4 {
      union {
        float   mat[4][4];
        float matp[16];
        vec4_t matv[4];
      };
      mat4(); // TODO(Noah): see if we can get this to be constexpr.
      mat4(std::initializer_list<float>);
    } mat4_t;
    #pragma pack(pop)
    /**
      * @param pos   camera is located by pos
      * @param scale camera is scaled by scale
      * @param eulerAngles camera is rotated about its own axis by eulerAngles.
      * Euler angles apply in the following rotation order: Z, Y, X.
      */
    typedef struct transform {
      vec3_t pos;
      vec3_t eulerAngles; // euler angles with rotation order of Z, Y, and X.
      vec3_t scale;
    } transform_t;
    /**
      * @param fov is field of vision. Angle is applied in the vertical.
      */
    typedef struct camera {
      transform_t trans;
      float fov;
      float nearPlane;
      float farPlane;
      int height;
      int width;
    } camera_t;
    typedef struct box {
      vec3_t pos;
      vec3_t scale;
    } box_t;
  }
}

#endif