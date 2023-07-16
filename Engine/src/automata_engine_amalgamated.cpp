
#define NC_STR_IMPL
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#if defined(AUTOMATA_ENGINE_VK_BACKEND)
#define VOLK_IMPLEMENTATION
#endif

#include "automata_engine_math.cpp"
#include "automata_engine_utils.cpp"
#include "automata_engine.cpp"
#include "automata_engine_io.cpp"

#if defined(AUTOMATA_ENGINE_DX12_BACKEND)
#include "automata_engine_dx.cpp"
#endif

#if defined(AUTOMATA_ENGINE_GL_BACKEND)
#include "automata_engine_gl.cpp"
#endif

#if defined(AUTOMATA_ENGINE_VK_BACKEND)
#include "automata_engine_vk.cpp"
#endif

#if defined(AUTOMATA_ENGINE_DX12_BACKEND) || defined(AUTOMATA_ENGINE_VK_BACKEND)
#include "automata_engine_hlsl.cpp"
#endif