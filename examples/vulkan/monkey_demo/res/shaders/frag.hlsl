
[[vk::binding(0)]]
Texture2D tex;

[[vk::binding(1)]]
SamplerState texSampler;

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
	float3 lightPos = float3(0.0, 1.0, 0.0); // top right of clip space
	float3 N = normalize(input.vNormal);
	float3 L = normalize(lightPos - input.vPos);

	float d = max(0,dot(N, L));

	float4 texColor = tex.SampleLevel(texSampler, input.texCoord, 0);

	return texColor * d;
}