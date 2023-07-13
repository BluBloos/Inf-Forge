#version 330 core

layout(location = 0) out vec4 color;

uniform vec3      iResolution;           // viewport resolution (in pixels)

uniform sampler2D iNormals;
// uniform sampler2D iDepth;

void main()
{
    // TODO: need to sample the scene using the existing depth buffer.

    // TODO: we also need to know the normals of the scene.


    vec2 uv = gl_FragCoord.xy / iResolution.xy; // vec2(2880, 1755); // iResolution.xy;

    // TODO: for now we just test that we can output red.
    // color = vec4(uv.x, uv.y, 0, 1.0);

    color = texture(iNormals, uv);
};
