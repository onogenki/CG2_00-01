#include"Object3d.hlsli"

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
    
    float specularPower;
    float specularStrength;
    float2 padding;
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
    float3 L = gDirectionalLight.direction;
    //視線ベクトル
    float3 toEye = normalize(gCamera.worldPosition - input.worldPosition);
    
  // 拡散反射
    float cos = saturate(dot(N, L));
    //拡散反射
    float32_t3 diffuse =
        gMaterial.color.rgb *
        textureColor.rgb *
        gDirectionalLight.color.rgb *
        cos *
        gDirectionalLight.intensity;

    // 鏡面反射
    float32_t3 halfVector = normalize(-L + toEye);
    float NDotH = dot(N, halfVector);
    float specularPow =
        pow(saturate(NDotH), gMaterial.shininess);
    float3 specular =
        gDirectionalLight.color.rgb *
        gDirectionalLight.intensity *
        specularPow *
        float32_t3(1.0f, 1.0f, 1.0f);

    // 拡散反射+鏡面反射
    output.color.rgb = diffuse + specular;

    // アルファ
    output.color.a =
        gMaterial.color.a * textureColor.a;
    
    if (output.color.a == 0.0f)
    {
        discard;
    }
   
    return output;
}