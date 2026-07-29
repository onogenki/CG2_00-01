#include "Fullscreen.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct RandomNoiseData
{
    float32_t time;
    float32_t intensity;
    float32_t2 padding;
};

ConstantBuffer<RandomNoiseData> gRandomNoiseData : register(b2);

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

float32_t rand2Dto1d(float32_t2 value)
{
    float32_t smallValue = sin(dot(value, float32_t2(12.9898f, 78.233f)));
    return frac(smallValue * 43758.5453f);
}

PixelShaderOutput main(VertexShaderOutput input)
{
    float32_t random = rand2Dto1d(input.texcoord + gRandomNoiseData.time);
    float32_t4 sourceColor = gTexture.Sample(gSampler, input.texcoord);

    PixelShaderOutput output;
    output.color.rgb = sourceColor.rgb * lerp(1.0f, random, gRandomNoiseData.intensity);
    output.color.a = sourceColor.a;
    return output;
}
