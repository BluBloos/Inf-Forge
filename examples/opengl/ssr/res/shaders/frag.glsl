#version 330 core

layout(location = 0) out vec4 color;
layout(location = 1) out vec3 normal;
layout(location = 2) out vec3 pos;
in vec2 vTexCoord;
in vec3 vPos;
in vec3 vNormal;

uniform sampler2D utexture;
uniform mat4 uview;

void main()
{
  vec3 lightPos = vec3(0.0, 1.0, 0.0); // top right of clip space
  vec3 N = normalize(vNormal);
  vec3 L = normalize(lightPos - vPos);
  float d = dot(N, L);
  vec4 texColor = texture(utexture, vTexCoord);
  // vec4 texColor = texture(utexture, vec2(vPos.x, vPos.y));
  // vec4 texColor = vec4(vTexCoord.x, vTexCoord.y, 0.0, 1.0);
  color = texColor * d;

  // dump the normals to gbuffer.
  normal = vNormal;

  // dump positions to gbuffer.
  pos = vec3(uview * vec4(vPos, 1));
};
