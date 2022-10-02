#include <automata_engine.h>
#include <automata_engine_math.h>
// TODO(Noah): Write unit tests for this math code. Getting this stuff wrong would
// likely be really annoying from a debugging perspective.
// NOTE(Noah): Matrices are being stored in column-major form ...
// the math below is representative of this.
namespace automata_engine {
    namespace math {
        // NOTE(Noah): we are making a presumption that all matrices are square.
        // NxN matrices!
        static void initMat(float *mat, uint32_t N, std::initializer_list<float> initList) {
            for (uint32_t i = 0; i < initList.size() && i < N * N; i++) {
                mat[i] = std::data(initList)[i];
            }
            // init as identity.
            if (initList.size() == 0) {
                for (uint32_t i = 0; i < N * N; i++) {
                    mat[i] = 0.0f;
                }
                for (uint32_t i = 0; i < N; i++) {
                    mat[i * N + i] = 1.0f;
                }
            }
        } 
        mat4::mat4(std::initializer_list<float> initList) {
            initMat(matp, 4, initList);
        }
        mat3::mat3(std::initializer_list<float> initList) {
            initMat(matp, 3, initList);
        }
        mat3::mat3(mat4_t b) {
            this->matv[0] = vec3(b.matv[0]);
            this->matv[1] = vec3(b.matv[1]);
            this->matv[2] = vec3(b.matv[2]);
        }
        vec3_t vec3::operator-() {
            return vec3_t(-this->x, -this->y, -this->z);
        }
        vec3::vec3(vec4_t a) : x(a.x), y(a.y), z(a.z) {}
        vec3::vec3(float a, float b, float c) :
            x(a), y(b), z(c) {}
        vec3::vec3() : x(0), y(0), z(0) {}
        vec4::vec4() : x(0), y(0), z(0), w(0) {}
        vec4::vec4(float a, float b, float c, float d) :
            x(a), y(b), z(c), w(d) {}
        vec4::vec4(vec3_t a, float b) : x(a.x), y(a.y), z(a.z), w(b) {}
        vec3_t operator+=(vec3_t &a, vec3_t b) {
            return a = vec3_t(a.x + b.x, a.y + b.y, a.z + b.z);
        }
        vec4_t operator+=(vec4_t &a, vec4_t b) {
            return a = vec4_t(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w);
        }
        vec4_t operator*=(vec4_t &a, float scalar) {
            return a = vec4_t(a.x * scalar, a.y * scalar, a.z * scalar, a.w * scalar);
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
        // TODO(Noah): So, Casey mentioned in that one blog post that we can get rid of 
        // the conversion from degrees to radians here altogether. Shall we?
        // NOTE(Noah): Our eulerAngles are composed by rot order of: Z, Y, X
        mat4_t buildRotMat4(vec3_t eulerAngles) {
            mat4_t result = {}; // start with identity.
            mat4_t rotationMatrixZ = {
                cosf(eulerAngles.z * 
                    DEGREES_TO_RADIANS),        -sinf(eulerAngles.z *
                                                    DEGREES_TO_RADIANS),        0, 0,
                sinf(eulerAngles.z *
                    DEGREES_TO_RADIANS),        cosf(eulerAngles.z *
                                                    DEGREES_TO_RADIANS),        0, 0,
                0,                              0,                              1, 0,
                0,                              0,                              0, 1
            };
            result = result * rotationMatrixZ;
            mat4_t rotationMatrixY = {
                cosf(eulerAngles.y * 
                    DEGREES_TO_RADIANS),        0, sinf(eulerAngles.y * 
                                                        DEGREES_TO_RADIANS),       0,
                0,                              1, 0,                              0,
               -sinf(eulerAngles.y * 
                    DEGREES_TO_RADIANS),        0, cosf(eulerAngles.y * 
                                                        DEGREES_TO_RADIANS),       0,
                0,                              0, 0,                              1
            };
            result = result * rotationMatrixY;
            mat4_t rotationMatrixX = {
                1,  0,                              0,                              0,
                0,  cosf(eulerAngles.x * 
                        DEGREES_TO_RADIANS),        sinf(eulerAngles.x * 
                                                        DEGREES_TO_RADIANS),        0,
                0, -sinf(eulerAngles.x * 
                        DEGREES_TO_RADIANS),        cosf(eulerAngles.x * 
                                                        DEGREES_TO_RADIANS),        0,
                0,  0,                              0,                              1
            };
            result = result * rotationMatrixX;
            return result;
        }
        // NOTE(Noah): I spent more time than I would like to admit formatting the code
        // below ...
        mat4_t buildMat4fFromTransform(transform_t transform) {
            mat4_t mat = {}; // identity.
            mat4_t rotMat = buildRotMat4(transform.eulerAngles);
            mat = rotMat * mat; // .matv[0] = vec4_t(rotMat.matv[0], 0.0f);
            mat.matv[0] *= transform.scale.x;
            mat.matv[1] *= transform.scale.y;
            mat.matv[2] *= transform.scale.z;
            mat.matv[3] = vec4_t(transform.pos, 1.0f);
            return mat;
        }
        // TODO(Noah): Review why this works ...
        mat4_t buildProjMat(camera_t cam) {
            float n = cam.nearPlane;
            float f = cam.farPlane;
            game_window_info_t winInfo = ae::platform::getWindowInfo();
            float aspectRatio = (float)winInfo.height / (float)winInfo.width;
            float r = tanf(cam.fov * DEGREES_TO_RADIANS / 2.0f) * n;
            float l = -r;
            float t = r * aspectRatio;
            float b = -t;
            // Matrix in reference to: http://www.songho.ca/opengl/gl_projectionmatrix.html
            // Modified to be column-order.
            ae::math::mat4_t result = {
                2 * n / (r - l),  0,                0,                   0,
                0,                2 * n / (t -b),   0,                   0,
                (r + l) / (r -l), (t + b) / (t -b), -(f + n) / (f -n),  -1,
                0,                0,                -2 * f * n / (f -n), 0
            };
            return result;
        }
        mat4_t transposeMat4(mat4_t  mat) {
            // the rows of the incoming matrix become the columns of the outgoing matrix.
            return {
                mat.mat[0][0], mat.mat[1][0], mat.mat[2][0], mat.mat[3][0],  
                mat.mat[0][1], mat.mat[1][1], mat.mat[2][1], mat.mat[3][1],
                mat.mat[0][2], mat.mat[1][2], mat.mat[2][2], mat.mat[3][2],
                mat.mat[0][3], mat.mat[1][3], mat.mat[2][3], mat.mat[3][3]
            };
        }
        mat4_t buildViewMat(camera_t cam) {
            mat4_t rotMat4 = buildRotMat4(cam.trans.eulerAngles);
            // TODO(Noah): can overloads be done for these sort of operations?
            rotMat4 = transposeMat4(rotMat4);
            mat4_t transMat = {};
            transMat.matv[3] = vec4(-cam.trans.pos, 1.0f);
            return rotMat4 * transMat;
        }
    }
}