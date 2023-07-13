#version 330 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 texCoord;
layout(location = 2) in vec3 normal;

out vec3 vNormal;
out vec2 vTexCoord;
out vec3 vPos;

uniform mat4 uproj;
uniform mat4 umodel;
uniform mat4 uModelRotate;
uniform mat4 uview;

void main()
{
  // NOTE(Noah): The lack of application of projection matrices to output vertices
  // is sensible to me. When within the fragment shader we more want the worldspace idea
  // of where vertices are. Which is why, in fact, we don't even need the view matrix either.
  // We will have our light pos be a worldspace pos.
  vec4 vModel = umodel * vec4(position.x, position.y, position.z, 1);
  gl_Position = uproj * uview * vModel;
  vTexCoord = texCoord;
  vPos = vec3(vModel);

  // NOTE: we use just using the rotation matrix since tranlation for the normal vectors,
  // which are just direction vectors, doesn't quite have a meaning.
  vNormal = vec3(uModelRotate * vec4(normal.x, normal.y, normal.z, 0));
};
