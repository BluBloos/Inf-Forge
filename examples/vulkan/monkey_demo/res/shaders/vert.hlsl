// TODO: can get v2p common define for both file. don't want to define twice.

struct V2P {
  float4 Pos : SV_Position;

  float2 texCoord : TEXCOORD0;
  float3 vPos : POSITION0;
  float3 vNormal : NORMAL0;
};

struct VertexInput {
  [[vk::location(0)]] float3 position : POSITION0;
  [[vk::location(1)]] float2 texCoord : TEXCOORD0;
  [[vk::location(2)]] float3 normal : NORMAL0;
};

struct CBData {
  float4x4 umodel;
  float4x4 uprojView;
};

[[vk::push_constant]] CBData cb;

V2P main(VertexInput input)
{
  float4 vModel = mul(cb.umodel, float4(input.position.xyz, 1.0));

  V2P output = (V2P)0;
  output.Pos = mul(cb.uprojView, vModel);

  output.texCoord = input.texCoord;
  output.vPos = vModel.xyz;

	// NOTE: for our contrived sample, it is OK to transform this normals like this.
  // in the general case, this may not work. e.g., if the matrix transform scaled
  // the object in the X dimension with a different factor to the scaling in the Y dim.
	output.vNormal = mul(cb.umodel, float4(input.normal.xyz, 0)).xyz;

	return output;
}