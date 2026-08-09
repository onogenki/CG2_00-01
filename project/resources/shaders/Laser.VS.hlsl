struct LaserData
{
    float32_t4x4 viewProjection;
    float32_t4 color;
};

ConstantBuffer<LaserData> gLaser : register(b0);

struct VertexShaderInput
{
    float32_t4 position : POSITION0;
};

struct VertexShaderOutput
{
    float32_t4 position : SV_POSITION;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    output.position = mul(input.position, gLaser.viewProjection);
    return output;
}
