struct PixelShaderInput
{
    float32_t4 position : SV_POSITION;
    float32_t2 texcoord : TEXCOORD0;
    float32_t4 color : COLOR0;
};

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

float32_t4 main(PixelShaderInput input) : SV_TARGET0
{
    return gTexture.Sample(gSampler, input.texcoord) * input.color;
}
