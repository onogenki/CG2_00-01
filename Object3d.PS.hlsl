#include "object3d.hlsli"

struct Material
{
    float32_t4 color;
    int32_t enableLighting;
    float32_t4x4 uvTransform;
    float32_t shininess;
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

struct Camera
{
    float32_t3 worldPosition;
};

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);
ConstantBuffer<Camera> gCamera : register(b2);

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
    
    // 法線
    float3 N = normalize(input.normal);
    // ライト方向
    float3 L = normalize(-gDirectionalLight.direction);
    //視線ベクトル
    float3 V = normalize(gCamera.worldPosition - input.worldPosition);
    
//拡散反射
    float NdotL = saturate(dot(N, L));
    float3 diffuse =
gMaterial.color.rgb * textureColor.rgb * gDirectionalLight.color.rgb * NdotL * gDirectionalLight.intensity;
    
//    float3 specular = 0;
//    if(gMaterial.usePhong!=0)
//    {
//     // 鏡面反射
//        float3 R = reflect(-L, N);
//        float RdotV = saturate(dot(R, V));
//        float spececularPow = pow(RdotV, gMaterial.specularPower); //反射強度
////鏡面反射
//        float3 specular =
//gDirectionalLight.color.rgb * gDirectionalLight.intensity * spececularPow * gMaterial.specularStrength;
//    }

//拡散反射・鏡面反射
    output.color.rgb = diffuse + specular;
//アルファは今まで通り
    output.color.a = gMaterial.color.a * textureColor.a;
   
    if (output.color.a == 0.0)
    {
        discard;
    }
    
    return output;

}