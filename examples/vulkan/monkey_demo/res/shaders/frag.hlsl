


// TODO: can get v2p common define for both file. don't want to define twice.


struct V2P {
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

	return float4(1,0,0,1) * d;
}