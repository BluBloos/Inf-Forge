
[[vk::binding(0)]]
Texture2D tex;

[[vk::binding(1)]]
SamplerState texSampler;

struct UboView
{
	float4x4 umodelTranslate;
	float4x4 umodelRotate;
	float4x4 uprojView;
	float4 lightColor;
	float  ambientStrength;
	float3 lightPos;
	float  specularStrength;
	float3 viewPos; // camera pos.
};

[[vk::binding(2)]]
cbuffer uboView { UboView uboView; };

// TODO: can get v2p common define for both file. don't want to define twice.
struct V2P {
// TODO: looks like this annotation is just to help us read
// what this data is from within a trace application??
  float2 texCoord : TEXCOORD0;
  float3 vPos : POSITION0;
  float3 vNormal : NORMAL0;
};

float4 main(V2P input) : SV_TARGET
{
	// PHONG LIGHTING MODEL.
	// ambient, diffuse, specular.

    // ambient.
	// this is the GI factor where there is just some general amount of light in the scene.
	float4 ambient = uboView.ambientStrength * uboView.lightColor;

	// diffuse.
	// this is the accumulated light due to the direct lighting from the scene onto the object.
	float3 N = normalize(input.vNormal);
	float3 Ldir = normalize(uboView.lightPos - input.vPos);
	float4 diffuse = max(0,dot(N, Ldir)) * uboView.lightColor;

	// specular.
	// this is strongest when the camera is viewing along direction of the reflected light from the surface.
	float3 viewDir = normalize(uboView.viewPos - input.vPos);
	float3 reflectDir = reflect(-Ldir, N);
	float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
	float4 specular = uboView.specularStrength * spec * uboView.lightColor;

	// TODO: our backbuffer that we are rending too is NOT HDR.
	// thus, any value above 1.0 is going to clamp, and we loose that information.
	// hence, we need to put some tonemapping here to go from HDR -> LDR.

	float4 texColor = tex.SampleLevel(texSampler, input.texCoord, 0);
	return texColor * (ambient + diffuse + specular);
}