#version 330 core

layout(location = 0) out vec4 color;

uniform vec3      iResolution; // viewport resolution (in pixels)

uniform sampler2D iNormals;
uniform sampler2D iAlbedo;
uniform sampler2D iPos;     // in view space.
uniform sampler2D iUVs;     // from SSR.

// uniform sampler2D iDepth;   // ndc.

float ambientStrength = 0.1;
float specularStrength = 0.5;
vec3 lightColor = vec3(1.0, 1.0, 1.0);

void main()
{
    vec2 uv = gl_FragCoord.xy / iResolution.xy;

	vec3 lightPos = vec3(0.0, 1.0, 0.0); // in view space.

    // PHONG LIGHTING MODEL.
	// ambient, diffuse, specular.

    // ambient.
	// this is the GI factor where there is just some general amount of light in the scene.
	vec3 ambient = ambientStrength * lightColor;

	// diffuse.
	// this is the accumulated light due to the direct lighting from the scene onto the object.
	vec3 N = normalize(texture(iNormals, uv).xyz);
    vec3 vPos = texture(iPos, uv).xyz;
	vec3 Ldir = normalize(lightPos - vPos);
	vec3 diffuse = max(0,dot(N, Ldir)) * lightColor;

	// specular.
	// this is strongest when the camera is viewing along direction of the reflected light from the surface.
	vec3 viewDir = normalize(-vPos);
	vec3 reflectDir = reflect(-Ldir, N);
	float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
	vec3 specular = specularStrength * spec * lightColor;

    // TODO: proper handle HDR?
    vec3 maybeReflectedColor = texture(iAlbedo, texture(iUVs, uv).xy).xyz;
	color = vec4(maybeReflectedColor * (ambient + diffuse + specular), 1.0);
};