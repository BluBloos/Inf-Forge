// TODO: can get v2p common define for both file. don't want to define twice.

struct V2P {
  float4 Pos : SV_Position;
  /*    [[vk::location(0)]]  float2 texCoord : TEXCOORD0;
      [[vk::location(1)]]  float3 vPos : POSITION0;
      [[vk::location(2)]]  float3 vNormal : NORMAL0;
      */
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

V2P main(uint vertexId : SV_VertexID) {
  V2P result;
  result.Pos =
      float4((vertexId & 1) * 4.0f - 1.0f, (vertexId & 2) * -2.0f + 1.0f, 0, 1);
  return result;
}

/*
V2P main(VertexInput input)
{

// NOTE: in HLSL matrices are stored with a row-major layout.
// whereas in GLSL matrices are stored with a colum-major layout.
// the Automata Engine math library encodes matrices with column-major layout.

// TODO: look into why the below is true.
// we just need to transpose the matrix
// before the matrix-vector multiply op. and it turns that mathematically this
can
// be achieved by flipping the order.

        //float4 vModel = mul(float4(input.position.xyz, 1.0), cb.umodel);
        float4 vModel = float4(input.position.xyz, 1.0);

        V2P output = (V2P)0;
        output.Pos = vModel; //mul(vModel, cb.uprojView);

//output.texCoord = input.texCoord;
        //output.vPos = vModel.xyz;
//	output.vNormal = mul(cb.umodel, float4(input.normal.xyz, 0)).xyz;
        return output;
}*/