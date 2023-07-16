#version 330 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 texCoord;
layout(location = 2) in vec3 normal;

// to the frag shader.
out vec3 vNormal;
out vec2 vTexCoord;
out vec3 vPos;

uniform mat4 uproj;
uniform mat4 umodel;
uniform mat4 uModelRotate;
uniform mat4 uview;

void main()
{
  vec4 vModel = umodel * vec4(position.x, position.y, position.z, 1);

  gl_Position = uproj * uview * vModel;

  vTexCoord = texCoord;
  vPos = vec3(vModel); // output these in world space. these get interpolated.
  // NOTE: we use just using the rotation matrix since tranlation for the normal vectors,
  // which are just direction vectors, doesn't quite have a meaning.
  vNormal = vec3(uModelRotate * vec4(normal.x, normal.y, normal.z, 0));
};
