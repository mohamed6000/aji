float4x4 wvp : register(c0);

struct VS_Output {
    float4 pos   : POSITION;
    float2 uv    : TEXCOORD0;
    float4 color : COLOR;
};

VS_Output main(float3 pos : POSITION, float2 uv : TEXCOORD0, float4 color : COLOR) {
    VS_Output result;
    result.pos   = mul(float4(pos, 1.0f), wvp);
    result.uv    = uv;
    result.color = color;
    return result;
}
