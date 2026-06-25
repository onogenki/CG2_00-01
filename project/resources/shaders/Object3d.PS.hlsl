#include"Object3d.hlsli"

struct Material
{
    float32_t4 color;
    int32_t enableLighting;
    float32_t4x4 uvTransform;
    float shininess;
    float environmentCoefficient;
    float32_t2 padding;
};

struct DirectionalLight
{
    float32_t4 color; //ライト�E色
    float32_t3 direction; //ライト�E向き
    float intensity; //輝度
    
    float specularPower;
    float specularStrength;
    float32_t2 padding;
};

struct PointLight
{
    float32_t4 color;//ライト�E色
    float32_t3 position;//ライト�E位置
    float intensity;//輝度
    float radius;//ライト�E届く最大距離
    float decay;//減衰玁E
};

struct SpotLight
{
    float32_t4 color;//ライト�E色
    float32_t3 position;//ライト�E位置
    float intensity;//輝度
    float32_t3 direction;//スポットライト�E方吁E
    float distance;//ライト�E届く最大距離
    float decay;//減衰玁E
    float cosAngle;//スポットライト�E余韻
    float cosFalloffStart;//Falloff開始�E角度
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
TextureCube<float32_t4> gEnvironmentTexture : register(t1);

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    float32_t4 transformedUV = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float32_t4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
    
    // 法緁E
    float32_t3 N = normalize(input.normal);
    //視線�Eクトル
    float32_t3 toEye = normalize(gCamera.worldPosition - input.worldPosition);
    
    ///
    ///平行�E溁E
    ///
    // ライト方吁E
    float32_t3 L = gDirectionalLight.direction;
    float cosDirectional = saturate(dot(N, L));
    
    //拡散反封E
    float32_t3 diffuseDirectionalLight =
        gMaterial.color.rgb *
        textureColor.rgb *
        gDirectionalLight.color.rgb *
        cosDirectional *
        gDirectionalLight.intensity;

    // 鏡面反封E
    float32_t3 halfVectorDirectional = normalize(-L + toEye);
    float NDotHDirectional = dot(N, halfVectorDirectional);
    float specularPowDirectional =
        pow(saturate(NDotHDirectional), gMaterial.shininess);
    float32_t3 specularDirectionalLight =
        gDirectionalLight.color.rgb *
        gDirectionalLight.intensity *
        specularPowDirectional *
        float32_t3(1.0f, 1.0f, 1.0f);

    ///
    ///点光源�E計箁E
    ///
    
    //入封E�Eの方吁E
    float32_t3 pointLightDirection = normalize(input.worldPosition - gPointLight.position);//ライトから頂点の方向に
    
    //減衰係数の計箁E
    float pointDistance = length(gPointLight.position - input.worldPosition); // ポイントライトへの距離
    float pointFactor = pow(saturate(-pointDistance / gPointLight.radius + 1.0f), gPointLight.decay); //持E��によるコントロール
    
    //拡散反封E
    float cosPoint = saturate(dot(N, -pointLightDirection));//頂点からライトへの方向に
    float32_t3 diffusePointLight =
    gMaterial.color.rgb * textureColor.rgb * gPointLight.color.rgb * cosPoint * gPointLight.intensity * pointFactor;
    
    //鏡面反封E
    float32_t3 halfVectorPoint = normalize(-pointLightDirection + toEye);
    float NDotHPoint = dot(N, halfVectorPoint);
    float specularPowPoint = pow(saturate(NDotHPoint), gMaterial.shininess);
    float32_t3 specularPointLight =
    gPointLight.color.rgb * gPointLight.intensity * pointFactor * specularPowPoint * float32_t3(1.0f, 1.0f, 1.0f);
    
    ///
    ///スポットライチE
    ///
    
    //ライトから頂点への方吁E
    float32_t3 spotLightDirectionOnSurface = normalize(input.worldPosition - gSpotLight.position);
    
    //距離による減衰
    float distanceSpot = length(gSpotLight.position - input.worldPosition);
    float attenuationFactor = pow(saturate(-distanceSpot / gSpotLight.distance + 1.0f), gSpotLight.decay);
    
    //角度による減衰
    float cosAngle = dot(spotLightDirectionOnSurface, gSpotLight.direction);
     //cosFalloffStartとcosAngleが同じ値になってめE.00001f が残るため、真っ黒�E真っ白を防ぁE
    float falloffFactor = saturate((cosAngle - gSpotLight.cosAngle) / max(gSpotLight.cosFalloffStart - gSpotLight.cosAngle, 0.00001f));

    //拡散反封E
    float cosSpot = saturate(dot(N, -spotLightDirectionOnSurface));
    float32_t3 diffuseSpotLight = gMaterial.color.rgb * textureColor.rgb * gSpotLight.color.rgb * cosSpot * gSpotLight.intensity * attenuationFactor * falloffFactor;
    
    //鏡面反封E
    float32_t3 halfVectorSpot = normalize(-spotLightDirectionOnSurface + toEye);
    float NDotHSpot = dot(N, halfVectorSpot);
    float specularPowSpot = pow(saturate(NDotHSpot), gMaterial.shininess);
    float32_t3 specularSpotLight =
        gSpotLight.color.rgb * gSpotLight.intensity * attenuationFactor * falloffFactor * specularPowSpot * float32_t3(1.0f, 1.0f, 1.0f);
    
    //全ての光を合�E
    output.color.rgb = diffuseDirectionalLight + specularDirectionalLight + diffusePointLight + specularPointLight + diffuseSpotLight + specularSpotLight;
    
    // Sample the environment map only when this material needs reflection.
    if (gMaterial.environmentCoefficient > 0.0f)
    {
        float32_t3 cameraToPosition = normalize(input.worldPosition - gCamera.worldPosition);
        float32_t3 reflectedVector = reflect(cameraToPosition, N);
        float32_t4 enviromentColor = gEnvironmentTexture.Sample(gSampler, reflectedVector);
    
        output.color.rgb += enviromentColor.rgb * gMaterial.environmentCoefficient;
    }
    // アルファ
    output.color.a =
        gMaterial.color.a * textureColor.a;
    
    if (output.color.a == 0.0f)
    {
        discard;
    }
    
   
    return output;
}
