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

struct SpotLight
{
    float32_t4 color;//ライトの色
    float32_t3 position;//ライトの位置
    float32_t intensity;//輝度
    float32_t3 direction;//スポットライトの方向
    float32_t distance;//ライトの届く最大距離
    float32_t decay;//減衰率
    float32_t cosAngle;//スポットライトの余韻
    float32_t cosFalloffStart;//Falloff開始の角度
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
ConstantBuffer<SpotLight> gSpotLight : register(b4);

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
    float32_t pointDistance = length(gPointLight.position - input.worldPosition); // ポイントライトへの距離
    float32_t pointFactor = pow(saturate(-pointDistance / gPointLight.radius + 1.0f), gPointLight.decay); //指数によるコントロール
    
    //拡散反射
    float cosPoint = saturate(dot(N, -pointLightDirection));//頂点からライトへの方向に
    float32_t3 diffusePointLight =
    gMaterial.color.rgb * textureColor.rgb * gPointLight.color.rgb * cosPoint * gPointLight.intensity * pointFactor;
    
    //鏡面反射
    float32_t3 halfVectorPoint = normalize(-pointLightDirection + toEye);
    float NDotHPoint = dot(N, halfVectorPoint);
    float specularPowPoint = pow(saturate(NDotHPoint), gMaterial.shininess);
    float3 specularPointLight =
    gPointLight.color.rgb * gPointLight.intensity * pointFactor * specularPowPoint * float32_t3(1.0f, 1.0f, 1.0f);
    
    ///
    ///スポットライト
    ///
    
    //ライトから頂点への方向
    float32_t3 spotLightDirectionOnSurface = normalize(input.worldPosition - gSpotLight.position);
    
    //距離による減衰
    float32_t distanceSpot = length(gSpotLight.position - input.worldPosition);
    float32_t attenuationFactor = pow(saturate(-distanceSpot / gSpotLight.distance + 1.0f), gSpotLight.decay);
    
    //角度による減衰
    float32_t cosAngle = dot(spotLightDirectionOnSurface, gSpotLight.direction);
     //cosFalloffStartとcosAngleが同じ値になっても0.00001f が残るため、真っ黒・真っ白を防ぐ
    float32_t falloffFactor = saturate((cosAngle - gSpotLight.cosAngle) / max(gSpotLight.cosFalloffStart - gSpotLight.cosAngle, 0.00001f));

    //拡散反射
    float cosSpot = saturate(dot(N, -spotLightDirectionOnSurface));
    float32_t3 diffuseSpotLight = gMaterial.color.rgb * textureColor.rgb * gSpotLight.color.rgb * cosSpot * gSpotLight.intensity * attenuationFactor * falloffFactor;
    
    //鏡面反射
    float32_t3 halfVectorSpot = normalize(-spotLightDirectionOnSurface + toEye);
    float NDotHSpot = dot(N, halfVectorSpot);
    float specularPowSpot = pow(saturate(NDotHSpot), gMaterial.shininess);
    float3 specularSpotLight =
        gSpotLight.color.rgb * gSpotLight.intensity * attenuationFactor * falloffFactor * specularPowSpot * float32_t3(1.0f, 1.0f, 1.0f);
    
    //全ての光を合成
    output.color.rgb = diffuseDirectionalLight + specularDirectionalLight + diffusePointLight + specularPointLight + diffuseSpotLight + specularSpotLight;
    
    // アルファ
    output.color.a =
        gMaterial.color.a * textureColor.a;
    
    if (output.color.a == 0.0f)
    {
        discard;
    }
    
   
    return output;
}