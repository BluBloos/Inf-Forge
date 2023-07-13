#version 330 core

layout(location = 0) out vec4 color;

uniform vec3      iResolution;           // viewport resolution (in pixels)

void main()
{
    vec2 uv = gl_FragCoord.xy/ vec2(2880, 1755);//iResolution.xy;
  // TODO: for now we just test that we can output red.
  color = vec4(uv.x, uv.y, 0, 1.0);
};
