#include <automata_engine.hpp>

// TODO(Noah): roll out our own intrinsics for these things below.
// we want to remove dependency on std:: and math.h.

// TODO(Noah): Write unit tests for this math code. Getting this stuff wrong would
// likely be really annoying from a debugging perspective.

// NOTE(Noah): Matrices are being stored in column-major form ...
// the math below is representative of this.

namespace automata_engine {
    namespace math {
        float *value_ptr(vec3_t &a) {
            return &a.x;
        }
        // NOTE(Noah): we are making a presumption that all matrices are square.
        // NxN matrices!
        static void initMat(float *mat, uint32_t N, std::initializer_list<float> initList) {
            // TODO(Noah): this fun in particular is slow.
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
        mat4_t::mat4_t() {
            /*
            matv[0] = vec4_t();
            matv[1] = vec4_t();
            matv[2] = vec4_t();
            matv[3] = vec4_t();
            for (uint32_t i = 0; i < 16; i++) {
                matp[i] = 0.0f;
            }
            for (uint32_t i = 0; i < 4; i++) {
                matp[i * 4 + i] = 1.0f;}
            */
            mat[0][0] = 1.f; mat[0][1] = 0.f; mat[0][2] = 0.f; mat[0][3] = 0.f;
            mat[1][0] = 0.f; mat[1][1] = 1.f; mat[1][2] = 0.f; mat[1][3] = 0.f;
            mat[2][0] = 0.f; mat[2][1] = 0.f; mat[2][2] = 1.f; mat[2][3] = 0.f;
            mat[3][0] = 0.f; mat[3][1] = 0.f; mat[3][2] = 0.f; mat[3][3] = 1.f;
        }
        mat4_t::mat4_t(std::initializer_list<float> initList) {
            initMat(matp, 4, initList);
        }
        mat3_t::mat3_t(std::initializer_list<float> initList) {
            initMat(matp, 3, initList);
        }
        mat3_t::mat3_t(mat4_t b) {
            this->matv[0] = vec3_t(b.matv[0]);
            this->matv[1] = vec3_t(b.matv[1]);
            this->matv[2] = vec3_t(b.matv[2]);
        }
        vec3_t vec3_t::operator-() {
            return vec3_t(-this->x, -this->y, -this->z);
        }
        vec3_t::vec3_t(vec4_t a) : x(a.x), y(a.y), z(a.z) {}
        vec4_t vec4_t::operator-() {
            return vec4_t(-this->x, -this->y, -this->z, -this->w);
        }
        vec4_t::vec4_t(vec3_t a, float b) : x(a.x), y(a.y), z(a.z), w(b) {}
        vec3_t operator+=(vec3_t &a, vec3_t b) {
            return a = vec3_t(a.x + b.x, a.y + b.y, a.z + b.z);
        }
        vec3_t operator*(vec3_t b, float a) {
            return vec3_t(b.x * a, b.y * a, b.z * a);
        }
        vec3_t operator+(vec3_t b, vec3_t a) {
            return vec3_t(b.x + a.x, b.y + a.y, b.z + a.z);
        }
        vec4_t operator+(vec4_t b, vec4_t a) {
            return vec4_t(b.x + a.x, b.y + a.y, b.z + a.z, b.w + a.w);
        }
        vec4_t operator*(vec4_t b, float a) {
            return vec4_t(b.x * a, b.y * a, b.z * a, b.w * a);
        }
        vec3_t operator-(vec3_t b, vec3_t a) {
            return b + (-a);
        }
        float pow(float base, float exp) { return powf(base, exp);}
        float  lerp(float a, float b, float t) { return a * (1.f - t) + b * t; }
        float &vec3_t::operator[](int index) {
            return (&this->x)[index];
        }
        float &vec4_t::operator[](int index) {
            return (&this->x)[index];
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
                cosf(eulerAngles.z),           -sinf(eulerAngles.z),          0, 0,
                sinf(eulerAngles.z),            cosf(eulerAngles.z),            0, 0,
                0,                              0,                              1, 0,
                0,                              0,                              0, 1
            };
            result = result * rotationMatrixZ;
            mat4_t rotationMatrixY = {
                cosf(eulerAngles.y),            0, sinf(eulerAngles.y),            0,
                0,                              1, 0,                              0,
               -sinf(eulerAngles.y),            0, cosf(eulerAngles.y),            0,
                0,                              0, 0,                              1
            };
            result = result * rotationMatrixY;
            mat4_t rotationMatrixX = {
                1,  0,                              0,                              0,
                0,  cosf(eulerAngles.x),            sinf(eulerAngles.x),            0,
                0, -sinf(eulerAngles.x),            cosf(eulerAngles.x),            0,
                0,  0,                              0,                              1
            };
            result = result * rotationMatrixX;
            return result;
        }
        vec3_t lookAt(vec3_t origin, vec3_t target) {
            // return the eulerAngles such that a body at origin is looking at target
            vec3_t direction = target - origin;
            float pitch =
                atan2(direction.y, sqrt(direction.x * direction.x + direction.z * direction.z));
            float yaw =
                atan2(direction.x, -direction.z);
            return vec3_t(pitch, yaw, 0.0f);
        }
        // NOTE(Noah): I spent more time than I would like to admit formatting the code
        // above ...
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
        // TODO(Noah): Implement a general matrix inverse function using
        // adjugate matrix. For now, we do whatever ...
        mat4_t buildInverseOrthoMat(camera_t cam) {
            ae::math::mat4_t transToCenter = {};
            transToCenter.matv[3] = vec4_t(0.0f, 0.0f, 
                -((cam.farPlane - cam.nearPlane) / 2.0f + cam.nearPlane), 1.0f);
            ae::math::mat4_t scaleAndFlip = {};
            scaleAndFlip.matv[0][0] = cam.width / 2.0f;
            scaleAndFlip.matv[1][1] = cam.height / 2.0f;
            scaleAndFlip.matv[2][2] = (cam.farPlane - cam.nearPlane) / -2.0f;
            return transToCenter * scaleAndFlip;
        }
        // TODO(Noah): Handle the case where farPlan == nearPlane. Will get a divide by
        // zero error here.
        mat4_t buildOrthoMat(camera_t cam) {
            ae::math::mat4_t transToCenter = {};
            transToCenter.matv[3] = vec4_t(0.0f, 0.0f, 
                (cam.farPlane - cam.nearPlane) / 2.0f + cam.nearPlane, 1.0f);
            ae::math::mat4_t scaleAndFlip = {};
            scaleAndFlip.matv[0][0] = 2.0f / cam.width;
            scaleAndFlip.matv[1][1] = 2.0f / cam.height;
            scaleAndFlip.matv[2][2] = -2.0f / (cam.farPlane - cam.nearPlane);
            return scaleAndFlip * transToCenter;
        }

        // TODO(Noah): Review in more detail how the projection matrix is mapping these radial lines in
        // world space to axis aligned lines in NDC space.
        template <bool forDxVk>
        mat4_t buildProjMatImpl(camera_t cam) {
            // NOTE: we are using a simplified matrix form where the center of the screen is located
            // at the center of the image plane of the frustrum. this makes things symmetric and the
            // math easier.
            float n = cam.nearPlane;
            float f = cam.farPlane;
            float aspectRatio = (float)cam.height / (float)cam.width;
            float r = tanf(cam.fov * DEGREES_TO_RADIANS / 2.0f) * n; // TODO: I saw in the realtime rendering book that this took a different form.
            float t = r * aspectRatio;
            // NOTE: when forDxVk is true, we map the near plane to NDC_z=0 instead of -1 (for GL).
            ae::math::mat4_t result = {
                n / r,            0,                0,                   0, // col 1
                0,                n / t,            0,                   0, // col 2
                0,                0,                (!forDxVk) ? -(f + n) / (f -n)   : -f/(f-n),   -1, // col 3
                0,                0,                (!forDxVk) ? -2 * f * n / (f -n) : -n*f/(f-n),  0  // col 4 
            };
            return result;
        }

        mat4_t buildProjMat(camera_t cam)
        {
            constexpr bool forDxVk = false;
            return buildProjMatImpl<forDxVk>(cam);
        }

        mat4_t buildProjMatForVk(camera_t cam)
        {
            constexpr bool forDxVk = true;
            return buildProjMatImpl<forDxVk>(cam);
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
            transMat.matv[3] = vec4_t(-cam.trans.pos, 1.0f);
            mat4_t scaleMat = {};
            scaleMat.matv[0][0] = 1.0f / cam.trans.scale.x;
            scaleMat.matv[1][1] = 1.0f / cam.trans.scale.y;
            return scaleMat * rotMat4 * transMat;
        }
        float atan2(float a, float b) {
            return std::atan2f(a, b);
        }
        float acos(float a) {
            return std::acosf(a);
        }
        float sqrt(float a) {
            // TODO(Noah): replace with our own intrinsic.
            return ::sqrtf(a);
        }
        float dist(vec3_t a, vec3_t b) {
            return sqrt(square(a.x - b.x) + square(a.y - b.y) + square(a.z - b.z));
        }
        float magnitude(vec3_t a) {
            return dist(a, vec3_t());
        }
        float dot(vec3_t a, vec3_t b) {
            return a.x * b.x + a.y * b.y + a.z * b.z;
        }
        int sign(int x) {
            // NOTE: this is branchless. that makes it fast because branch prediction is expensive to fail.
            // it is expensive to fail due to the opportunity cost of the preemptive execution.
            return (x > 0) - (x < 0);
        }
        float round(float a) {
            return std::round(a);
        }
        vec3_t normalize(vec3_t a) {
            float mag = magnitude(a);
            if (mag == 0.f)
                return vec3_t();
            return vec3_t(a.x / mag, a.y / mag, a.z / mag);
        }
        float project(vec3_t a, vec3_t b) {
            b = normalize(b);
            return (dot(a, b)); // * b;
        }
        float log10(float a) {
            return std::log10(a);
        }
        float log2(float a) {
            return std::log2(a);
        }
        float abs(float a) {
            return std::abs(a);
        }
        float ceil(float a) {
            return std::ceil(a);
        }
        float floor(float a) {
            return float(int64_t(a));
        }
        float deg2rad(float deg) {
            return deg * DEGREES_TO_RADIANS;
        }
        float sin(float a) {
            return std::sin(a);
        }
        float cos(float a) {
            return std::cos(a);
        }
        float signedAngle(vec3_t a, vec3_t b, vec3_t N) {
            return atan2(dot(N, cross(a, b)), dot(a, b));
        }
        float angle(vec3_t a, vec3_t b) {
            return acos(dot(a, b) / (magnitude(a) * magnitude(b)));
        }
        vec3_t cross(vec3_t a, vec3_t b){
            return {
                a.y * b.z - a.z * b.y,
                a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x
            };
        }

        bool doesRayIntersectWithAABB2(
            const vec3_t &rayOrigin, const vec3_t &rayDir,
            const aabb_t &candidateBox, bool *exitedEarly, int*faceHitIdx
        ) {
            assert(rayDir.x != 0.f || rayDir.y != 0.f || rayDir.z != 0.f);
            // simplified version of the original function.
            // maybe faster. maybe slower.

            // how it works:

            // find all planes that are "facing us". should at most be three.
            // intersect ray with these.
            // check if point is in bounds.

            // if the ray somehow intersects two planes (hits precisely vertex of AABB),
            // then deterministically give back some Idx to ensure no visual jitter frame-frame
            // if a consumer were to use faceHitIdx to render something. this is to say given a constant
            // ray, the result of this function should be temporally stable.

            int potentialFaces[3]={};  //stores face Idx's.
            int faceCount=0;

            // find faces.
            constexpr int aabbFaceCount=6;
            constexpr ae::math::vec3_t faceNormals[] = {
                // front, back, left, right, top, bottom.
                {0.f,0.f,-1.f}, {0.f,0.f,1.f}, {-1.f,0.f,0.f}, {1.f,0.f,0.f}, {0.f,1.f,0.f}, {0.f,-1.f,0.f}
            };
            assert(sizeof(faceNormals)/sizeof(ae::math::vec3_t)==aabbFaceCount);
            for (int i=0;i<aabbFaceCount;i++){
                float d = dot(rayDir, faceNormals[i]);
                if (d<0) {
                    potentialFaces[faceCount++]=i;
                }
            }
            assert(faceCount<=3);
            assert(aabbFaceCount==6);

            for (int i=0;i<faceCount;i++) {

                float planeCord;
                auto checkPointInBounds = [&](const vec3_t &p) {
                    return p.x >= candidateBox.min.x && p.x <= candidateBox.max.x &&
                           p.y >= candidateBox.min.y && p.y <= candidateBox.max.y &&
                           p.z >= candidateBox.min.z && p.z <= candidateBox.max.z;
                };
                const int j=potentialFaces[i];
                // TODO: need faceHitIdx to use enum for face Idx's. make this standard.
                switch(j) {
                    case 0: // front, back: (which Z).
                    case 1:
                    {
                        planeCord = (j==0)?candidateBox.min.z:candidateBox.max.z;
                        if (rayDir.z==0.f) continue; // TODO: maybe.
                        float tHit = (planeCord - rayOrigin.z) / rayDir.z;
                        if (tHit<0.f) continue;
                        float y=rayOrigin.y + rayDir.y * tHit;
                        float x=rayOrigin.x + rayDir.x * tHit;
                        if (checkPointInBounds(vec3_t(x,y,planeCord))) {
                            if (faceHitIdx) *faceHitIdx=potentialFaces[i];
                            return true;
                        }
                    }
                    break;
                    case 2: // left, right: (which X).
                    case 3:
                    {
                        planeCord = (j==2)?candidateBox.min.x:candidateBox.max.x;
                        if (rayDir.x==0.f) continue;
                        float tHit = (planeCord - rayOrigin.x) / rayDir.x;
                        if (tHit<0.f) continue;
                        float y=rayOrigin.y + rayDir.y * tHit;
                        float z=rayOrigin.z + rayDir.z * tHit;
                        if (checkPointInBounds(vec3_t(planeCord,y,z))) {
                            if (faceHitIdx) *faceHitIdx=potentialFaces[i];
                            return true;
                        }
                    }
                    break;
                    case 4: // top, bottom: (which Y).
                    case 5:
                    {
                        planeCord = (j==4)?candidateBox.max.y:candidateBox.min.y;
                        if (rayDir.y==0.f) continue;
                        float tHit = (planeCord - rayOrigin.y) / rayDir.y;
                        if (tHit<0.f) continue;
                        float x=rayOrigin.x + rayDir.x * tHit;
                        float z=rayOrigin.z + rayDir.z * tHit;
                        if (checkPointInBounds(vec3_t(x,planeCord,z))) {
                            if (faceHitIdx) *faceHitIdx=potentialFaces[i];
                            return true;
                        }
                    }
                    break;
                }

            }

            return false;

        }

        // TODO: Seems I made something slow. in a way, it's almost like a rube goldberg machine.
        //       it was fun, anyways.
        bool doesRayIntersectWithAABB(
            const vec3_t &rayOrigin, const vec3_t &rayDir,
            const aabb_t &candidateBox, bool *exitedEarly, int*faceHitIdx
        ) {
            assert(rayDir.x != 0.f || rayDir.y != 0.f || rayDir.z != 0.f);
            const vec3_t R = candidateBox.origin - rayOrigin;
            vec3_t N = cross(rayDir, R);
            if (N.x == 0.f && N.y == 0.f && N.z == 0.f) {
                if (exitedEarly) *exitedEarly = true; // TODO: this bool should be optimized out at compile-time if not unit testing. 
                return dot(R, rayDir) >= 0.f;
            }
            N = normalize(N);
            const float a0 = signedAngle(rayDir, R, N);
            constexpr float piHalf = PI/2.0f;
            // we can exit early if angle is too large.
            if (abs(a0) >(piHalf)) {
                if (exitedEarly) *exitedEarly = true;
                return false;
            }
            // intersect the plane with the box and get back a set of intersection points.
            vec3_t planeCubeHitPoints[24];
            int planeCubeHitPointsCount=0;
            typedef struct axis_line_info_t {
                float x,y,z;
                int axis; // 0=x, 1=y, 2=z.
            } axis_line_info_t;
            constexpr size_t boxLineCount=12;
            // TODO: this could be faster if we pull out the candidateBox vals and make this table constexpr.
            // then just do MUL at runtime.
            const axis_line_info_t cbLines[boxLineCount] = {
                {candidateBox.min.x,0,candidateBox.min.z,1}, //note y=0 doesn't matter as this is a line along Y axis.
                {candidateBox.max.x,0,candidateBox.min.z,1},
                {candidateBox.min.x,0,candidateBox.max.z,1},
                {candidateBox.max.x,0,candidateBox.max.z,1},
                {0,candidateBox.min.y,candidateBox.min.z,0},
                {0,candidateBox.max.y,candidateBox.min.z,0},
                {0,candidateBox.min.y,candidateBox.max.z,0},
                {0,candidateBox.max.y,candidateBox.max.z,0},
                {candidateBox.min.x,candidateBox.min.y,0,2},
                {candidateBox.max.x,candidateBox.min.y,0,2},
                {candidateBox.min.x,candidateBox.max.y,0,2},
                {candidateBox.max.x,candidateBox.max.y,0,2},
            };
            for (int i=0;i<boxLineCount;i++){
                axis_line_info_t al = cbLines[i];
                // TODO: below looks RIPE for optimization.
                switch(al.axis){
                    case 0: // x axis
                    // NOTE: we can skip the 0 case since points returned by this are caught by other AABB lines.
                    if (N.x!=0.0f) {
                        float x = (-N.y*(al.y-candidateBox.origin.y)-N.z*(al.z-candidateBox.origin.z))/N.x+candidateBox.origin.x;
                        if (x>=candidateBox.min.x && x<=candidateBox.max.x)
                            planeCubeHitPoints[planeCubeHitPointsCount++]={x,al.y,al.z};
                    }
                    break;
                    case 1: // y axis
                    if (N.y!=0.0f) {
                        float y = (-N.x*(al.x-candidateBox.origin.x)-N.z*(al.z-candidateBox.origin.z))/N.y+candidateBox.origin.y;
                        if (y>=candidateBox.min.y && y<=candidateBox.max.y)
                            planeCubeHitPoints[planeCubeHitPointsCount++]={al.x,y,al.z};
                    }
                    break;
                    case 2: // z axis
                    if (N.z!=0.0f) {
                        float z = (-N.x*(al.x-candidateBox.origin.x)-N.y*(al.y-candidateBox.origin.y))/N.z+candidateBox.origin.z;
                        if (z>=candidateBox.min.z && z<=candidateBox.max.z)
                            planeCubeHitPoints[planeCubeHitPointsCount++]={al.x,al.y,z};
                    }
                    break;
                }
            }
            //assert(planeCubeHitPointsCount<=3);
            // then make rays from the rayOrigin to all the intersection points.
            // find of those rays those that give the min/max angle.
                // the angle in this case is that of the ray and R.
            float minAngle = piHalf;
            float maxAngle = -piHalf;
            for (int i = 0; i < planeCubeHitPointsCount; ++i) {
                const vec3_t rN = planeCubeHitPoints[i] - rayOrigin;
                const float aN=signedAngle(rN, R, N);
                if (aN < minAngle) minAngle = aN;
                if (aN > maxAngle) maxAngle = aN;
            }
            // these min/max form the permissible range that our ray could make with R.
            return (a0 >= minAngle) && (a0 <= maxAngle);
        }
        aabb_t aabb_t::make(vec3_t origin,vec3_t halfDim)
        {
          aabb_t r = {};
          r.origin = origin;
          r.halfDim = halfDim;
          r.min = origin - halfDim;
          r.max = origin + halfDim;
          return r;
        }
        // NOTE(Noah): All this stuff below is prob slow. Abstraction makes things slow. later we can speed up.
        aabb_t aabb_t::fromCube(vec3_t bottomLeft, float width) {
            const vec3_t halfDim = { width / 2.0f, width / 2.0f, width / 2.0f };
            const vec3_t origin = bottomLeft + halfDim;
            return aabb_t::make(origin, halfDim);
        }
        aabb_t aabb_t::fromLine(vec3_t p0, vec3_t p1) {
            const vec3_t halfDelta = {(p1.x-p0.x)/2.0f, (p1.y-p0.y)/2.0f, (p1.z-p0.z)/2.0f}; 
            const vec3_t origin = p0 + halfDelta;
            const vec3_t halfDim = {abs(halfDelta.x), abs(halfDelta.y),
                                    abs(halfDelta.z)};
            return aabb_t::make(origin, halfDim);
        }
    }
}
