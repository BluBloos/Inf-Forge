#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
#include <catch.hpp>

#include <automata_engine.h>
// ------------ stubs required to mock a game ----------------
void ae::PreInit(ae::game_memory_t *gameMemory) {}
void ae::Init(ae::game_memory_t *gameMemory) {}
void ae::Close(ae::game_memory_t *gameMemory) {}
void ae::HandleWindowResize(ae::game_memory_t *gameMemory, int newWdith, int newHeight) {}
void ae::OnVoiceBufferProcess(
    ae::game_memory_t *gameMemory, intptr_t voiceHandle, void *dst, void *src,
    uint32_t samplesToWrite, int channels, int bytesPerSample) {}
void ae::OnVoiceBufferEnd(ae::game_memory_t *gameMemory, intptr_t voiceHandle) {}
// ---------------------------------------------------

unsigned int Factorial( unsigned int number ) {
    return number <= 1 ? number : Factorial(number-1)*number;
}

TEST_CASE("ray x AABB intersection", "[ae::math]") {
    ae::math::aabb_t cube = ae::math::aabb_t::make( { 0, 0, 0 }, { 1, 1, 1 }  );
    const float rayLen = 6.f;
    float T;
    ae::math::vec3_t rBegin;
    ae::math::vec3_t rEnd;
    ae::math::vec3_t rDir;
    SECTION( "intersects if ray clips AABB edge" ) {
        // at a 45 deg angle
        rBegin = {0, 0, -2};
        rEnd = {1, 0, -1};
        rDir = ae::math::normalize(rEnd - rBegin);
        REQUIRE( true == doesRayIntersectWithAABB(rBegin, rDir, rayLen, cube, &T));
        // at a non-45 deg angle
        rBegin = {-0.5, 0, -2};
        rEnd = {1, 0, -1};
        rDir = ae::math::normalize(rEnd - rBegin);
        REQUIRE( true == doesRayIntersectWithAABB(rBegin, rDir, rayLen, cube, &T));
    }
    SECTION( "intersects if ray goes through AABB origin" ) {
        rBegin = {0, 0, -2};
        rEnd = {0, 0, 0};
        rDir = ae::math::normalize(rEnd - rBegin);
        REQUIRE( true == doesRayIntersectWithAABB(rBegin, rDir, rayLen, cube, &T));
    }
    SECTION( "intersects if ray goes through AABB generically" ) {
        rBegin = {0, 0, -2};   // somewhere not in AABB.
        rEnd  = {0.5, 0, 0.5}; // somewhere in AABB.
        rDir = ae::math::normalize(rEnd - rBegin);
        REQUIRE( true == doesRayIntersectWithAABB(rBegin, rDir, rayLen, cube, &T));
    }
    SECTION( "does not intersect" ) {
        rBegin = {0, 0, -2};
        rEnd = {-1.2, 0, -1.2};
        rDir = ae::math::normalize(rEnd - rBegin);
        REQUIRE( false == doesRayIntersectWithAABB(rBegin, rDir, rayLen, cube, &T));
    }
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