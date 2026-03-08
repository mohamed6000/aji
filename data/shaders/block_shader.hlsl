sampler2D rm_sampler : register(s0);

struct VS_Output {
    float4 pos   : POSITION;
    float2 uv    : TEXCOORD0;
    float4 color : COLOR;
};

float4 main(VS_Output input) : COLOR {
    float4 texel = tex2D(rm_sampler, input.uv);
    float4 final_color = texel * input.color;

/*
RAINBOW effect.
    if (!any(final_color)) return final_color;

    float step = 1.0/7;

    if      (input.uv.x < (step * 1)) final_color = float4(1,0,0,1);
    else if (input.uv.x < (step * 2)) final_color = float4(1,0.5,0,1);
    else if (input.uv.x < (step * 3)) final_color = float4(1,1,0,1);
    else if (input.uv.x < (step * 4)) final_color = float4(0,1,0,1);
    else if (input.uv.x < (step * 5)) final_color = float4(0,0,1,1);
    else if (input.uv.x < (step * 6)) final_color = float4(.3,0,0.8,1);
    else                              final_color = float4(1,0.8,1,1);
*/

// GRADIENT effect.
    if (final_color.a)
        final_color.rgb *= input.uv.y;

    return final_color;
}
