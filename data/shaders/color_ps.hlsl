sampler2D rm_sampler : register(s0);

struct VS_Output {
    float4 pos   : POSITION;
    float2 uv    : TEXCOORD0;
    float4 color : COLOR;
};

float4 main(VS_Output input) : COLOR {
    float4 texel = tex2D(rm_sampler, input.uv);
    return input.color;
}
