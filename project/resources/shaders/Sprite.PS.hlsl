#include "Sprite.hlsli"

struct Material
{
    float32_t4 color;
    int32_t enableLighting;
    float32_t4x4 uvTransform;
};

ConstantBuffer<Material> gMaterial : register(b0);

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    // UVトランスフォームを適用
    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    // テクスチャの色を取得
    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
    
    //マテリアルカラー × テクスチャカラー を出力（ライティングなし）
    output.color = gMaterial.color * textureColor;

    // アルファが0なら描画しない
    if (output.color.a == 0.0f)
    {
        discard;
    }
    
    return output;
}