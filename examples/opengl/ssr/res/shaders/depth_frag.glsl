#version 330 core

layout(location = 0) out vec4 color;

uniform vec3      iResolution; // viewport resolution (in pixels)

uniform float f; // far plane
uniform float n; // near plane

uniform sampler2D iDepth;   // the depth.
uniform sampler2D iUVs;     // from SSR.

void main()
{
    vec2 uv = gl_FragCoord.xy / iResolution.xy;

    float depthNDC = texture(iDepth, texture(iUVs, uv).xy).r; // between [0,1]
    depthNDC = depthNDC * 2 - 1; // map to [-1, 1]

    // z_ndc = (f + n) / (f -n) + 2 * f * n / (f - n) / z
    // rearrange the above and we get:
    float depthView = 2.0 * f * n / (depthNDC * (f-n) - (f+n));

    // map from the frustrum back to [0,1]
    float depthColor = (-depthView-n)/(f-n);

	color = vec4(  depthColor, depthColor, depthColor, 1.0);
};