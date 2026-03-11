#include"Particle.hlsli"

struct Material
{
    float32_t4 color;
    int32_t enableLighting;
    float32_t4x4 uvTransform;
};

struct DirectionalLight
{
    float32_t4 color; //ライトの色
    float32_t3 direction; //ライトの向き
    float intensity; //輝度
};

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};
ConstantBuffer<Material> gMaterial : register(b0);

ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    float4 uv = mul(float4(input.texcoord, 0, 1), gMaterial.uvTransform);
    float4 tex = gTexture.Sample(gSampler, uv.xy);

    if (gMaterial.enableLighting != 0)
    {
        float3 normal = normalize(input.normal);
        float3 lightDir = normalize(-gDirectionalLight.direction);
        float NdotL = saturate(dot(normal, lightDir));
        float3 diffuse = gMaterial.color.rgb * tex.rgb * input.color.rgb * gDirectionalLight.color.rgb * NdotL * gDirectionalLight.intensity;
        output.color.rgb = diffuse;
        output.color.a = gMaterial.color.a * tex.a * input.color.a;

    }
    else
    {
        output.color = gMaterial.color * tex * input.color;
    }
    
    if (tex.a <= 0.5f)
        discard;
    return output;
}