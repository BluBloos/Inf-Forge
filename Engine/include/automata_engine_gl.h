#pragma once

// #include <automata_engine.h>
#include <glew.h>
#include <gl/gl.h>
#include <gl/Glu.h>
#include <initializer_list>

#define GL_CALL(code) ae::GL::GLClearError(); code; assert(ae::GL::GLCheckError(#code, __FILE_RELATIVE__, __LINE__));

namespace automata_engine {
    namespace GL {
        typedef struct vertex_attrib {
            GLenum type;
            uint32_t count;
            vertex_attrib(GLenum type, uint32_t count);
        } vertex_attrib_t;
        typedef struct vbo {
            uint32_t glHandle;
            vertex_attrib_t *attribs; // stretchy buffer
        } vbo_t;
        typedef struct ibo {
            uint32_t count;
            GLuint glHandle;
        } ibo_t;
        typedef struct vertex_attrib_desc {
            uint32_t attribIndex;
            uint32_t *indices; // stretchy buffer
            vbo_t vbo;
            bool iterInstance;
            vertex_attrib_desc(
                uint32_t attribIndex,
                std::initializer_list<uint32_t> indices,
                vbo_t vbo,
                bool iterInstance);
        } vertex_attrib_desc_t;
    }
}