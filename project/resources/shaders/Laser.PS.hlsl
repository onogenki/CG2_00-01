struct LaserData
{
    float32_t4x4 viewProjection;
    float32_t4 color;
};

ConstantBuffer<LaserData> gLaser : register(b0);

float32_t4 main() : SV_TARGET0
{
    return gLaser.color;
}
