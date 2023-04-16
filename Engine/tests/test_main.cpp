#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
#include <catch.hpp>

#include <automata_engine.hpp>
// ------------ stubs required to mock a game ----------------
void ae::PreInit(ae::game_memory_t *gameMemory) {}
void ae::InitAsync(ae::game_memory_t *gameMemory) {}
void ae::Init(ae::game_memory_t *gameMemory) {}
void ae::Close(ae::game_memory_t *gameMemory) {}
void ae::HandleWindowResize(ae::game_memory_t *gameMemory, int newWdith, int newHeight) {}
void ae::OnVoiceBufferProcess(
    ae::game_memory_t *gameMemory, intptr_t voiceHandle, float *dst, float *src,
    uint32_t samplesToWrite, int channels, int bytesPerSample) {}
void ae::OnVoiceBufferEnd(ae::game_memory_t *gameMemory, intptr_t voiceHandle) {}
// ---------------------------------------------------

unsigned int Factorial( unsigned int number ) {
    return number <= 1 ? number : Factorial(number-1)*number;
}


namespace utils {
    #include <stdlib.h>

    void Seed(uint32_t seed) {
        // TODO(Noah): Implement a better pseudo-random number generator.
        srand(seed);
    }

    uint32_t RandomUINT32(uint32_t lower, uint32_t upper) {
        assert(upper >= lower);
        if (upper == lower) return upper;
        uint64_t diff = upper - lower;
        assert((uint32_t)RAND_MAX >= diff);
        return (uint32_t)( (uint64_t)rand() % (diff + 1) + (uint64_t)lower );
    }

    float32_t RandomFloat(float begin, float end) {
        assert(RAND_MAX != 0); // this should be obvious ... 
        const float f = (float32_t)rand() / (float32_t)RAND_MAX;
        return begin + f * (end - begin);
    }

}


TEST_CASE("cross product", "[ae:math]") {
    ae::math::vec3_t a = { 1, 0, 0 };
    ae::math::vec3_t b = { 0, 1, 0 };
    ae::math::vec3_t c = ae::math::cross(a, b);
    REQUIRE( c.x == 0 );
    REQUIRE( c.y == 0 );
    REQUIRE( c.z == 1 );

    ae::math::vec3_t d = ae::math::cross(b, a);
    REQUIRE( d.x == 0 );
    REQUIRE( d.y == 0 );
    REQUIRE( d.z == -1 );

    ae::math::vec3_t e = ae::math::cross(a, a);
    REQUIRE( e.x == 0 );
    REQUIRE( e.y == 0 );
    REQUIRE( e.z == 0 );

    ae::math::vec3_t f = { 2, 3, 4 };
    ae::math::vec3_t g = { 5, 6, 7 };
    ae::math::vec3_t h = ae::math::cross(f, g);
    REQUIRE(h.x == -3);
    REQUIRE(h.y == 6);
    REQUIRE(h.z == -3);
}

TEST_CASE("ray x AABB intersection", "[ae::math]") {
    ae::math::aabb_t cube = ae::math::aabb_t::make( { 0, 0, 0 }, { 1, 1, 1 }  );
    ae::math::vec3_t rBegin;
    ae::math::vec3_t rEnd;
    ae::math::vec3_t rDir;
    SECTION( "intersects if ray clips AABB edge" ) {
        // at a 45 deg angle
        rBegin = {0, 0, -2};
        rEnd = {1, 0, -1};
        rDir = ae::math::normalize(rEnd - rBegin);
        REQUIRE( true == doesRayIntersectWithAABB(rBegin, rDir, cube));
        REQUIRE( true == doesRayIntersectWithAABB2(rBegin, rDir, cube));
        // at a non-45 deg angle
        rBegin = {-0.5, 0, -2};
        rEnd = {1, 0, -1};
        rDir = ae::math::normalize(rEnd - rBegin);
        REQUIRE( true == doesRayIntersectWithAABB(rBegin, rDir, cube));
        REQUIRE( true == doesRayIntersectWithAABB2(rBegin, rDir, cube));
    }
    SECTION( "intersects if ray goes through AABB origin" ) {
        rBegin = {0, 0, -2};
        rEnd = {0, 0, 0};
        rDir = ae::math::normalize(rEnd - rBegin);
        REQUIRE( true == doesRayIntersectWithAABB(rBegin, rDir, cube));
        REQUIRE( true == doesRayIntersectWithAABB2(rBegin, rDir, cube));
    }
    SECTION( "intersects if ray goes through AABB generically" ) {
        utils::Seed(__LINE__);
        for (int i=0;i<1000;i++){
            const float theta = utils::RandomFloat(0,2*PI);
            const float phi   = utils::RandomFloat(0,2*PI);
            const float R     = utils::RandomFloat(2,10);
            // a random point somewhere outside of AABB.
            rBegin = {R*ae::math::cos(theta)*ae::math::sin(phi), R*ae::math::sin(theta)*ae::math::sin(phi), R*ae::math::cos(phi)};
            // a random point somewhere inside of AABB.
            rEnd = {utils::RandomFloat(-1, 1), utils::RandomFloat(-1, 1), utils::RandomFloat(-1, 1)};
            rDir = ae::math::normalize(rEnd - rBegin);
            CAPTURE(rBegin.x, rBegin.y, rBegin.z);
            CAPTURE(rEnd.x, rEnd.y, rEnd.z); CAPTURE(i);
            REQUIRE( true == doesRayIntersectWithAABB(rBegin, rDir, cube));
            REQUIRE( true == doesRayIntersectWithAABB2(rBegin, rDir, cube));
        }
    }
    SECTION( "does not intersect" ) {
        utils::Seed(__LINE__);
        int i;
        for (i = 0; i < 1000; i++) {
            // a random point somewhere outside of AABB.
            rBegin = {utils::RandomFloat(-10, -2), utils::RandomFloat(-10, -2), utils::RandomFloat(-10, -2)};
            rEnd = {utils::RandomFloat(-1.5, -1.1), utils::RandomFloat(-1.5, -1.1), utils::RandomFloat(-1.5, -1.1)};
            rDir = -ae::math::normalize(rEnd - rBegin);
            CAPTURE(rBegin.x, rBegin.y, rBegin.z);
            CAPTURE(rDir.x, rDir.y, rDir.z);
            CAPTURE(i);
            REQUIRE( false == doesRayIntersectWithAABB(rBegin, rDir, cube));
            REQUIRE( false == doesRayIntersectWithAABB2(rBegin, rDir, cube));
        }
    }
    SECTION( "if ray is facing away from cube" ) {
        utils::Seed(__LINE__);
        SECTION( "works when ray direction is parallel with ray origin" ) {
            const float theta = utils::RandomFloat(0,2*PI);
            const float phi   = utils::RandomFloat(0,2*PI);
            const float R     = utils::RandomFloat(2,10);
            // a random point somewhere outside of AABB.
            rBegin = {R*ae::math::cos(theta)*ae::math::sin(phi), R*ae::math::sin(theta)*ae::math::sin(phi), R*ae::math::cos(phi)};
            rDir = ae::math::normalize(rBegin);
            CAPTURE(rBegin.x, rBegin.y, rBegin.z);
            CAPTURE(rDir.x, rDir.y, rDir.z);
            bool ee=false;
            REQUIRE( false == doesRayIntersectWithAABB(rBegin, rDir, cube, &ee));
            REQUIRE( false == doesRayIntersectWithAABB2(rBegin, rDir, cube, &ee));
            // TODO:
            //REQUIRE( ee == true );
        }
    }
}

TEST_CASE( "signed angle", "[ae::math]" ) {
    ae::math::vec3_t a = {0.56744,-1.16698,-8.14923};//begin
    ae::math::vec3_t b = {0.06876, -0.14142,-0.98756};//dir
    ae::math::vec3_t R = ae::math::vec3_t{0, 0, 0} - a;
    auto N = ae::math::normalize(ae::math::cross(R,b));
    const float ang = ae::math::signedAngle(R, b, N);
    constexpr float halfPi = PI/2.0f;
    REQUIRE(abs(ang)>halfPi);
}

// TEST_CASE( name, tags )
TEST_CASE( "Factorials are computed", "[factorial]" ) {
    REQUIRE( Factorial(1) == 1 );
    REQUIRE( Factorial(2) == 2 );
    REQUIRE( Factorial(3) == 6 );
    REQUIRE( Factorial(10) == 3628800 );
}

TEST_CASE( "vectors can be sized and resized", "[vector]" ) {

    std::vector<int> v( 5 );

    REQUIRE( v.size() == 5 );
    REQUIRE( v.capacity() >= 5 );

    SECTION( "resizing bigger changes size and capacity" ) {
        v.resize( 10 );

        REQUIRE( v.size() == 10 );
        REQUIRE( v.capacity() >= 10 );
    }
    SECTION( "resizing smaller changes size but not capacity" ) {
        v.resize( 0 );

        REQUIRE( v.size() == 0 );
        REQUIRE( v.capacity() >= 5 );
    }
    SECTION( "reserving bigger changes capacity but not size" ) {
        v.reserve( 10 );

        REQUIRE( v.size() == 5 );
        REQUIRE( v.capacity() >= 10 );
    }
    SECTION( "reserving smaller does not change size or capacity" ) {
        v.reserve( 0 );

        REQUIRE( v.size() == 5 );
        REQUIRE( v.capacity() >= 5 );
    }

    // can be nested.
    SECTION( "reserving bigger changes capacity but not size" ) {
        v.reserve( 10 );

        REQUIRE( v.size() == 5 );
        REQUIRE( v.capacity() >= 10 );

        SECTION( "reserving smaller again does not change capacity" ) {
            v.reserve( 7 );

            REQUIRE( v.capacity() >= 10 );
        }
    }
}