#version 330 core

layout(location = 0) out vec4 color;
layout(location = 1) out vec3 normal;
layout(location = 2) out vec3 pos;

// input from vertex shader.
in vec2 vTexCoord;
in vec3 vPos;

// TODO: could use normal maps.
in vec3 vNormal;

uniform sampler2D utexture;
uniform mat4 uview;

void main()
{
  // load.
  vec4 texColor = texture(utexture, vTexCoord);

  // dump the gbuffer contents.
  color = texColor;
  normal = vNormal;
  pos = vec3(uview * vec4(vPos, 1));
};
