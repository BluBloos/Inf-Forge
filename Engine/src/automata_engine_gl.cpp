#include <automata_engine.hpp>
#include <cstdarg>

// TODO(Noah): impl dealloc unfunction for vbo_t

#if defined(AUTOMATA_ENGINE_GL_BACKEND)
namespace automata_engine {
    namespace GL {
        void GLClearError() { while(glGetError() != GL_NO_ERROR); }
        bool GLCheckError(const char *expr, const char *file, int line) {
            bool result = true;
            while(GLenum error = glGetError()) {
                static_assert(sizeof(GLubyte)==sizeof(char), "platform is odd");
                const char *errorString = (const char*)gluErrorString(error);
                AELoggerLog("GL_ERROR: (%d, 0x%x) %s \nEXPR: %s\nFILE: %s\nLINE: %d", 
                    error, error, errorString, expr, file, line);
                result = false;
            }
            return result;
        }
        vertex_attrib_desc_t::vertex_attrib_desc_t(
            uint32_t attribIndex,
            std::initializer_list<uint32_t> indices_list,
            vbo_t vbo,
            bool iterInstance
        ) : attribIndex(attribIndex), indices(nullptr), vbo(vbo), iterInstance(iterInstance) {
            for (uint32_t i = 0; i < indices_list.size(); i++) {
                StretchyBufferPush(indices, std::data(indices_list)[i]);
            }
        };
        vertex_attrib_t::vertex_attrib_t(GLenum type, uint32_t count)
            :
            type(type), count(count) {};
        bool glewIsInit = false;
        static GLuint compileShader(uint32_t type, char *shader) {
            uint32_t id = glCreateShader(type);
            glShaderSource(id, 1, &shader, NULL);
            glCompileShader(id);
            int result;
            glGetShaderiv(id, GL_COMPILE_STATUS, &result);
            if(result == GL_FALSE) {
                int length;
                glGetShaderiv(id, GL_INFO_LOG_LENGTH, &length);
                char *message = (char *)automata_engine::platform::alloc(length * sizeof(char));
                if (message != NULL) {
                    glGetShaderInfoLog(id, length, &length, message);
                    AELoggerError("Failed to compile shader!\n");
                    AELoggerError("%s\n", message);
                    automata_engine::platform::free(message);
                }
                glDeleteShader(id);
                return -1;
            }
            return id;
        }
        // TODO: there is quite a bit of copy-pasta between this and the normal
        // createShader. we should prob clean that up! failure mode for this
        // function is -1
        GLuint createComputeShader(const char *filePath) {
            uint32_t program;
            // Step 1: Setup our vertex and fragment GLSL shaders!
            loaded_file_t f1 =
                automata_engine::platform::readEntireFile(filePath);
            GL_CALL(program = glCreateProgram());
            uint32_t cs = compileShader(GL_COMPUTE_SHADER, (char *)f1.contents);
            auto findAndlogError = [=]() {
              GLsizei length;
              GLint logLength;
              glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
              const char *logMsg = new char[logLength + 1];
              defer(delete[] logMsg);
              static_assert(sizeof(GLchar) == sizeof(char), "odd platform");
              glGetProgramInfoLog(program, logLength, &length,
                                  (GLchar *)logMsg);
              AELoggerError("Program link or validation failure:\n%s", logMsg);
            };
            if ((int)cs == -1) {
                goto createComputeShader_Fail;
            }
            glAttachShader(program, cs);
            glLinkProgram(program);
            GLint result;
            glGetProgramiv(program, GL_LINK_STATUS, &result);
            if (result == GL_FALSE) {
                findAndlogError();
                goto createComputeShader_Fail;
            }
            glValidateProgram(program);
            glGetProgramiv(program, GL_LINK_STATUS, &result);
            if (result == GL_FALSE) {
                findAndlogError();
                goto createComputeShader_Fail;
            }
            goto createComputeShader_end;
        createComputeShader_Fail:
            glDeleteProgram(program);
            return -1;
        createComputeShader_end:
            // Cleanup
            glDeleteShader(cs);
            automata_engine::platform::freeLoadedFile(f1);
            return program;
        }

        // failure mode for this function is -1
        GLuint createShader(const char *vertFilePath, const char *fragFilePath,
            const char *geoFilePath
        ) {
            uint32_t program;
            // Step 1: Setup our vertex and fragment GLSL shaders!
            loaded_file_t f1 = automata_engine::platform::readEntireFile(vertFilePath);
            loaded_file_t f2 =
                automata_engine::platform::readEntireFile(fragFilePath);
            GL_CALL(program = glCreateProgram());
            uint32_t vs = compileShader(GL_VERTEX_SHADER, (char *)f1.contents);
            uint32_t fs = compileShader(GL_FRAGMENT_SHADER, (char *)f2.contents);
            uint32_t gs = 1;
            auto findAndlogError = [=]() {
              GLsizei length;
              GLint logLength;
              glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
              const char *logMsg = new char[logLength + 1];
              defer(delete[] logMsg);
              static_assert(sizeof(GLchar) == sizeof(char), "odd platform");
              glGetProgramInfoLog(program, logLength, &length,
                                  (GLchar *)logMsg);
              AELoggerError("Program link or validation failure:\n%s", logMsg);
            };
            if (geoFilePath[0]) {
              loaded_file_t f3 =
                  automata_engine::platform::readEntireFile(geoFilePath);
                defer(automata_engine::platform::freeLoadedFile(f3));
                gs = compileShader(GL_GEOMETRY_SHADER, (char *)f3.contents);
                if ((int)gs == -1) {
                    goto createShader_Fail;
                }
                glAttachShader(program, gs);
            }
            if ((int)vs == -1 || (int)fs == -1) {
                goto createShader_Fail;
            }
            glAttachShader(program, vs);
            glAttachShader(program, fs);
            glLinkProgram(program);
            GLint result;
            glGetProgramiv(program, GL_LINK_STATUS, &result);
            if (result == GL_FALSE) {
                findAndlogError();
                goto createShader_Fail;
            }
            glValidateProgram(program);
            glGetProgramiv(program, GL_LINK_STATUS, &result);
            if (result == GL_FALSE) {
                findAndlogError();
                goto createShader_Fail;
            }
            goto createShader_end;
            createShader_Fail:
            glDeleteProgram(program);
            return -1;
            createShader_end:
            // Cleanup
            glDeleteShader(vs);
            glDeleteShader(fs);
            if (gs != -1) {
                glDeleteShader(gs);
            }
            automata_engine::platform::freeLoadedFile(f1);
            automata_engine::platform::freeLoadedFile(f2);
            return program;
        }
        // TODO: Can we remove this? Maybe make it so that if using glew, the game decides so.
        void initGlew() {
            if (!glewIsInit) {
                GLenum err = glewInit();
                if(GLEW_OK != err) {
                    // TODO(Noah): GLEW failed, what the heck do we do?
                    AELoggerError("Something is seriously wrong");
                    assert(false);
                } else {
                    glewIsInit = true;
                }
            }
        }
        // TODO(Noah): Are there performance concerns with always calling GetUniformLocation?
        void setUniformMat4f(GLuint shader, const char *uniformName, ae::math::mat4_t val) {
            GLuint loc;
            GL_CALL(loc = glGetUniformLocation(shader, uniformName));
            GL_CALL(glUniformMatrix4fv(loc, 1, GL_FALSE, (val).matp));
        }
        
        GLuint createTextureFromFile(
            const char *filePath,
            GLint minFilter, GLint magFilter, bool generateMips, GLint wrap
        ) {
            loaded_image_t img = ae::platform::stbImageLoad((char *)filePath);
            GLuint tex = 0;
            if (img.pixelPointer != nullptr) {
                tex = createTexture(
                    img.pixelPointer, img.width, img.height,
                    minFilter, magFilter, generateMips, wrap
                );
                glFlush(); // push all buffered commands to GPU
                glFinish(); // block until GPU is complete
                ae::io::freeLoadedImage(img);
            }
            return tex;
        }

        GLuint createTexture(
            unsigned int *pixelPointer, unsigned int width, unsigned int height,
            GLint minFilter, GLint magFilter, bool generateMips, GLint wrap
        ) {
            GLuint newTexture;
            glGenTextures(1, &newTexture);
            glBindTexture(GL_TEXTURE_2D, newTexture);
            // TODO(Noah): So, when we go about creating textures in the future, 
            // maybe we actually care about setting some different parameters.
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
            glTexImage2D(
                GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixelPointer);
            if (generateMips)
                glGenerateMipmap(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, 0);
            return newTexture;
        }

        // TODO(Noah): add err checking
        vbo_t createAndSetupVbo(
            uint32_t counts,
            ...) {
            vbo_t myVbo = {};
            glGenBuffers(1, &myVbo.glHandle);
            va_list vl;
            va_start(vl, counts);
            for (uint32_t i = 0; i < counts; i++) {
                vertex_attrib_t attrib = va_arg(vl, vertex_attrib_t);
                StretchyBufferPush(myVbo.attribs, attrib);
            }
            va_end(vl);
            return myVbo;
        }

        uint32_t GLenumToBytes(GLenum glType) {
            uint32_t strideDelta = 0;
            switch(glType) {
                case GL_BYTE:
                case GL_UNSIGNED_BYTE:
                    strideDelta = 1;
                    break;
                case GL_SHORT:
                case GL_UNSIGNED_SHORT:
                case GL_HALF_FLOAT:
                    strideDelta = 2;
                    break;
                case GL_INT:
                case GL_UNSIGNED_INT:
                case GL_FIXED:
                case GL_FLOAT:
                    // packed formats
                case GL_INT_2_10_10_10_REV:
                case GL_UNSIGNED_INT_2_10_10_10_REV:
                case GL_UNSIGNED_INT_10F_11F_11F_REV:
                    strideDelta = 4;
                    break;  
                case GL_DOUBLE:
                    strideDelta = 8;
                    break;
            }
            return strideDelta;
        }

        uint32_t vbo_GetOffset(vbo_t &vbo, uint32_t indexK) {
            uint32_t offset = 0;
            for (uint32_t i = 0; i < indexK; i++) {
                vertex_attrib_t attrib = vbo.attribs[i];
                offset += GLenumToBytes(attrib.type) * attrib.count;
            }
            return offset;
        }

        uint32_t vbo_GetStride(vbo_t &vbo) {
            return vbo_GetOffset(vbo, StretchyBufferCount(vbo.attribs));
        }

        // TODO(Noah): Make the indexing into vbo SAFE.
        GLuint createAndSetupVao(uint32_t attribCounts, ...) {
            GLuint vao;
            glGenVertexArrays(1, &vao); glBindVertexArray(vao);
            AELoggerLog("called createAndSetupVao with vao=%d", vao);
            GLuint boundVbo = 0;            
            va_list vl;
            va_start(vl, attribCounts);
            for (uint32_t i = 0; i < attribCounts; i++) {
                vertex_attrib_desc_t attribDesc = va_arg(vl, vertex_attrib_desc_t);
                vbo_t *pVbo = &attribDesc.vbo;
                if (pVbo->attribs == nullptr) {
                    AELoggerError(
                        "Fatal in createAndSetupVao\n"
                        "VBO is likely not initialized\n");
                    automata_engine::setFatalExit();
                } else {
                    if (pVbo->glHandle != boundVbo) {
                        glBindBuffer(GL_ARRAY_BUFFER, pVbo->glHandle);
                    }
                    uint32_t attribIndex = attribDesc.attribIndex;
                    
                    for (uint32_t j = 0; j < StretchyBufferCount(attribDesc.indices); j++) {
                        uint32_t k = attribDesc.indices[j];
                        vertex_attrib_t attrib = pVbo->attribs[k];
                        uint32_t offset = 0;
                        for (uint32_t w = 0; w < (uint32_t)ceilf(attrib.count / 4.0f); w++) {
                            uint32_t attribCount    = attrib.count - (w * 4);
                            uint32_t componentCount = (attribCount > 4) ? 4 : attribCount;
                            intptr_t ptr            = (intptr_t)offset + vbo_GetOffset(*pVbo, k);
                            switch (attrib.type) {
                                case GL_BYTE:
                                case GL_UNSIGNED_BYTE:
                                case GL_SHORT:
                                case GL_UNSIGNED_SHORT:
                                case GL_INT:
                                case GL_UNSIGNED_INT:
                                    glVertexAttribIPointer(attribIndex,
                                        componentCount,
                                        attrib.type,
                                        vbo_GetStride(*pVbo),
                                        (const void *)ptr);
                                    break;
                                default:
                                    glVertexAttribPointer(attribIndex,
                                        componentCount,
                                        attrib.type,
                                        // NOTE: we are basically going to be making this a default - glfalse.
                                        GL_FALSE,
                                        vbo_GetStride(*pVbo),
                                        (const void *)ptr);
                            }
                            AELoggerLog(
                                "did glVertexAttribPointer(%d, %d, type, GL_FALSE, %d, %d);",
                                attribIndex, componentCount, vbo_GetStride(*pVbo), ptr);
                            offset += componentCount * GLenumToBytes(attrib.type);
                            glEnableVertexAttribArray(attribIndex);
                            if ( attribDesc.iterInstance ) {
                                glVertexAttribDivisor(attribIndex, 1);
                            }
                            attribIndex++;
                        }
                    }
                }
                // NOTE(Noah): I'm not sure how I feel about this ...
                // seems like a hack / bad API?
                StretchyBufferFree(attribDesc.indices);
            }
            va_end(vl);
            return vao;
        }

        void objToVao(raw_model_t rawModel, ibo_t *iboOut, vbo_t *vboOut, GLuint *vaoOut) {
            // create the index buffer
            glGenBuffers(1, &iboOut->glHandle);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, iboOut->glHandle);
            iboOut->count = StretchyBufferCount(rawModel.indexData);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, iboOut->count * sizeof(unsigned int),
                rawModel.indexData, GL_STATIC_DRAW);
            // vbo def defines components
            *vboOut = ae::GL::createAndSetupVbo(
                3,
                vertex_attrib_t(GL_FLOAT, 3),
                vertex_attrib_t(GL_FLOAT, 2),
                vertex_attrib_t(GL_FLOAT, 3)
            );
            // taking a BO and selecting vertex components to slot into our VAO desc.
            *vaoOut = ae::GL::createAndSetupVao(
                3,
                vertex_attrib_desc_t(0, {0}, *vboOut, false /* per vertex */),
                vertex_attrib_desc_t(1, {1}, *vboOut, false /* per vertex */),
                vertex_attrib_desc_t(2, {2}, *vboOut, false /* per vertex */)
            );
            // upload vertex data.
            glBindBuffer(GL_ARRAY_BUFFER, vboOut->glHandle);
            // GL_STATIC_DRAW = we won't really update this data.
            glBufferData(GL_ARRAY_BUFFER, sizeof(float) * StretchyBufferCount(rawModel.vertexData),
                rawModel.vertexData, GL_STATIC_DRAW);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }
    }
};


#endif