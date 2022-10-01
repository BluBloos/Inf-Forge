#include <automata_engine.h>
#include <automata_engine_math.h>

// NOTE(Noah): Matrices are being stored in column-major form ...
// the math below is representative of this.
namespace automata_engine {
    namespace math {
        static void initMat(float *mat, uint32_t count, std::initializer_list<float> initList) {
            for (uint32_t i = 0; i < initList.size() && i < count; i++) {
                mat[i] = std::data(initList)[i];
            }
            if (initList.size() == 0) {
                for (uint32_t i = 0; i < count; i++) {
                    mat[i] = 0;
                }
            }
        } 
        mat4::mat4(std::initializer_list<float> initList) {
            initMat(matp, 16, initList);
        }
        mat3::mat3(std::initializer_list<float> initList) {
            initMat(matp, 9, initList);
        }
        vec3::vec3(float a, float b, float c) :
            x(a), y(b), z(c) {}
        vec3::vec3() : x(0), y(0), z(0) {}
        vec3_t operator+=(vec3_t &a, vec3_t b) {
            return a = vec3_t(a.x + b.x, a.y + b.y, a.z + b.z);
        }
        // matrix b applies onto vector a
        vec4_t operator*(mat4_t b, vec4_t a) {
            vec4_t c;
            c.x = a.x * b.mat[0][0] + a.y * b.mat[1][0] + a.z * b.mat[2][0] + a.w * b.mat[3][0];
            c.y = a.x * b.mat[0][1] + a.y * b.mat[1][1] + a.z * b.mat[2][1] + a.w * b.mat[3][1];
            c.z = a.x * b.mat[0][2] + a.y * b.mat[1][2] + a.z * b.mat[2][2] + a.w * b.mat[3][2];
            c.w = a.x * b.mat[0][3] + a.y * b.mat[1][3] + a.z * b.mat[2][3] + a.w * b.mat[3][3];
            return c;
        }
        // matrix a applies onto b
        mat4_t operator*(mat4_t a, mat4_t b) {
            mat4_t c = {};
            c.matv[0] = a * b.matv[0];
            c.matv[1] = a * b.matv[1];
            c.matv[2] = a * b.matv[2];
            c.matv[3] = a * b.matv[3];
            return c;
        }
        // matrix b applies onto vector a
        vec3_t operator*(mat3_t b, vec3_t a) {
            vec3_t c;
            c.x = a.x * b.mat[0][0] + a.y * b.mat[1][0] + a.z * b.mat[2][0];
            c.y = a.x * b.mat[0][1] + a.y * b.mat[1][1] + a.z * b.mat[2][1];
            c.z = a.x * b.mat[0][2] + a.y * b.mat[1][2] + a.z * b.mat[2][2];
            return c;
        }
        // NOTE(Noah): I spent more time than I would like to admit formatting the code
        // below ...
        mat4_t buildMat4fFromTransform(transform_t transform) {
            mat4_t result = {
                transform.scale.x,  0,                 0,                  0,
                0,                  transform.scale.y, 0,                  0,
                0,                  0,                  transform.scale.z, 0,
                0,                  0,                  0,                  1
            };
            mat4_t rotationMatrixY = {
                cosf(transform.eulerAngles.y * 
                    DEGREES_TO_RADIANS),        0, sinf(transform.eulerAngles.y * 
                                                        DEGREES_TO_RADIANS),       0,
                0,                              1, 0,                              0,
            -sinf(transform.eulerAngles.y * 
                    DEGREES_TO_RADIANS),        0, cosf(transform.eulerAngles.y * 
                                                        DEGREES_TO_RADIANS),       0,
                0,                              0, 0,                              1
            };
            result = result * rotationMatrixY;
            mat4_t rotationMatrixZ = {
                1,  0,                              0,                              0,
                0,  cosf(transform.eulerAngles.x * 
                        DEGREES_TO_RADIANS),        sinf(transform.eulerAngles.x * 
                                                        DEGREES_TO_RADIANS),        0,
                0, -sinf(transform.eulerAngles.x * 
                        DEGREES_TO_RADIANS),        cosf(transform.eulerAngles.x * 
                                                        DEGREES_TO_RADIANS),        0,
                0,  0,                              0,                              1
            };
            result = result * rotationMatrixZ;
            result.mat[3][0] = transform.pos.x;
            result.mat[3][1] = transform.pos.y;
            result.mat[3][2] = transform.pos.z;
            return result;
        }
    }
}