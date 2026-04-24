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

struct PointLight
{
    float32_t4 color;//ライトの色
    float32_t3 position;//ライトの位置
    float intensity;//輝度
    float radius;//ライトの届く最大距離
    float decay;//減衰率
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
ConstantBuffer<PointLight> gPointLight : register(b3);

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
    
    // 法線
    float3 N = normalize(input.normal);
    //視線ベクトル
    float3 toEye = normalize(gCamera.worldPosition - input.worldPosition);
    
    ///
    ///平行光源
    ///
    // ライト方向
    float3 L = gDirectionalLight.direction;
    float cosDirectional = saturate(dot(N, L));
    
    //拡散反射
    float32_t3 diffuseDirectionalLight =
        gMaterial.color.rgb *
        textureColor.rgb *
        gDirectionalLight.color.rgb *
        cosDirectional *
        gDirectionalLight.intensity;

    // 鏡面反射
    float32_t3 halfVectorDirectional = normalize(-L + toEye);
    float NDotHDirectional = dot(N, halfVectorDirectional);
    float specularPowDirectional =
        pow(saturate(NDotHDirectional), gMaterial.shininess);
    float3 specularDirectionalLight =
        gDirectionalLight.color.rgb *
        gDirectionalLight.intensity *
        specularPowDirectional *
        float32_t3(1.0f, 1.0f, 1.0f);

    ///
    ///点光源の計算
    ///
    
    //入射光の方向
    float32_t3 pointLightDirection = normalize(input.worldPosition - gPointLight.position);//ライトから頂点の方向に
    
    //減衰係数の計算
    float32_t distance = length(gPointLight.position - input.worldPosition); // ポイントライトへの距離
    float32_t factor = pow(saturate(-distance / gPointLight.radius + 1.0f), gPointLight.decay); //指数によるコントロール
    
    //拡散反射
    float cosPoint = saturate(dot(N, -pointLightDirection));//頂点からライトへの方向に
    float32_t3 diffusePointLight =
    gMaterial.color.rgb * textureColor.rgb * gPointLight.color.rgb * cosPoint * gPointLight.intensity * factor;
    
    //鏡面反射
    float32_t3 halfVectorPoint = normalize(-pointLightDirection + toEye);
    float NDotHPoint = dot(N, halfVectorPoint);
    float specularPowPoint = pow(saturate(NDotHPoint), gMaterial.shininess);
    float3 specularPointLight =
    gPointLight.color.rgb * gPointLight.intensity * factor * specularPowPoint * float32_t3(1.0f, 1.0f, 1.0f);
    
    //全ての光を合成
    output.color.rgb = diffuseDirectionalLight + specularDirectionalLight + diffusePointLight + specularPointLight;
    
    // アルファ
    output.color.a =
        gMaterial.color.a * textureColor.a;
    
    if (output.color.a == 0.0f)
    {
        discard;
    }
    
   
    return output;
}