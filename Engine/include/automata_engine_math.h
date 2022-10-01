#pragma once

// #include <automata_engine.h>

// I suppose we will use a combination of glm and our own routines.
// Isn't math fun?

// Oh, and, we'll probably want to make this code have some nice documentation.
// In fact, learning is always the most important part, isn't it?

#ifndef AUTOMATA_ENGINE_MATH
#define AUTOMATA_ENGINE_MATH

// TODO: Remove dependency on math.h and do transcendentals ourselves.
// We want SPEED.
#include <math.h>
#include <initializer_list>

#if !defined(PI)
#define PI 3.1415f
#endif
#define DEGREES_TO_RADIANS PI / 180.0f

namespace automata_engine {
  namespace math {
    typedef struct vec2 {
      float x,y;
    } vec2_t;
    typedef struct vec3 {
      float x,y,z;
      vec3();
      vec3(float, float, float);
    } vec3_t;
    typedef struct vec4 {
      float x,y,z,w;
    } vec4_t;
    typedef struct mat3 {
      union {
        float mat[3][3];
        float matp[9];
        vec3_t matv[3];
      };
      mat3(std::initializer_list<float>);
    } mat3_t;
    typedef struct mat4 {
      union {
        float   mat[4][4];
        float matp[16];
        vec4_t matv[4];
      };
      mat4(std::initializer_list<float>);
    } mat4_t;
    /**
      * @param pos   camera is located by pos
      * @param scale camera is scaled by scale
      * @param eulerAngles camera is rotated about its own axis by eulerAngles.
      * Euler angles apply in the following rotation order: Z, Y, X.
      */
    typedef struct transform {
      vec3_t pos;
      vec3_t eulerAngles; // euler angles with rotation order of Z, X, and Y (like Unity).
      vec3_t scale;
    } transform_t;
  }
}

#endif