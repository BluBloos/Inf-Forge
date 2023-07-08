// TODO: can get v2p common define for both file. don't want to define twice.

struct V2P {
  float4 Pos : SV_Position;

  float2 texCoord : TEXCOORD0;
  float3 vPos : POSITION0;
  float3 vNormal : NORMAL0;
};

struct VertexInput {
  // NOTE: the semantic string is required for these input variables. why?
  [[vk::location(0)]] float3 position : THIS_DOESNT_MATTER;
  [[vk::location(1)]] float2 texCoord : THE_TEXCOORD_BOY;
  [[vk::location(2)]] float3 normal : THE_NORMAL_BOY;
};

struct UboView
{
  float4x4 modelMatrix;
  float4x4 modelRotate;
  float4x4 projView;
};

[[vk::binding(2)]]
cbuffer uboView { UboView uboView; };

V2P main(VertexInput input)
{
  V2P output = (V2P)0;

  float4 vModel = mul(uboView.modelMatrix, float4(input.position.xyz, 1.0));

  output.Pos = mul(uboView.projView, vModel);
  output.texCoord = input.texCoord;
  output.vPos = vModel.xyz;

  // TODO: we could use a normal map to get more accurate normals across the surface
  // of the model. currently the normals are linearly interpolated from the vertices which
  // is technically not the best approach.

	// NOTE: we use just using the rotation matrix since tranlation for the normal vectors,
  // which are just direction vectors, doesn't quite have a meaning.
	output.vNormal = mul(uboView.modelRotate, float4(input.normal.xyz, 0)).xyz;

	return output;
}