struct Particle
{
    float32_t3 translate;
    float32_t3 scale;
    float lifeTime;
    float32_t3 velocity;
    float currentTime;
    float32_t4 color;
};

struct EmitterSphere
{
    float32_t3 translate;
    float radius;
    uint32_t count;
    float frequency;
    float frequencyTime;
    uint32_t emit;
};

struct PerFrame
{
    float time;
    float deltaTime;
};

float rand3dTo1d(float32_t3 value, float32_t3 dotDirection)
{
    float smallValue = sin(dot(value, dotDirection));
    return frac(smallValue * 143758.5453f);
}

float32_t3 rand3dTo3d(float32_t3 value)
{
    return float32_t3(
        rand3dTo1d(value, float32_t3(12.9898f, 78.233f, 37.719f)),
        rand3dTo1d(value, float32_t3(39.3468f, 11.135f, 83.155f)),
        rand3dTo1d(value, float32_t3(73.156f, 52.235f, 9.151f)));
}

class RandomGenerator
{
    float32_t3 seed;

    float32_t3 Generate3d()
    {
        seed = rand3dTo3d(seed);
        return seed;
    }

    float Generate1d()
    {
        float result = rand3dTo1d(seed, float32_t3(41.731f, 17.903f, 91.147f));
        seed.x = result;
        return result;
    }
};

static const uint32_t kMaxParticles = 1024;
RWStructuredBuffer<Particle> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeCounter : register(u1);
ConstantBuffer<EmitterSphere> gEmitter : register(b0);
ConstantBuffer<PerFrame> gPerFrame : register(b1);

[numthreads(1, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    if (gEmitter.emit != 0)
    {
        RandomGenerator generator;
        generator.seed = (float32_t3(DTid) + gPerFrame.time) * gPerFrame.time;
        for (uint32_t countIndex = 0; countIndex < gEmitter.count && countIndex < kMaxParticles; ++countIndex)
        {
            int32_t particleIndex;
            InterlockedAdd(gFreeCounter[0], 1, particleIndex);
            if (particleIndex < kMaxParticles)
            {
                Particle particle = (Particle)0;
                particle.scale = generator.Generate3d();
                particle.lifeTime = 3.0f;
                float32_t3 direction = generator.Generate3d() * 2.0f - 1.0f;
                float directionLengthSquared = dot(direction, direction);
                if (directionLengthSquared < 0.0001f)
                {
                    direction = float32_t3(0.0f, 1.0f, 0.0f);
                }
                else
                {
                    direction *= rsqrt(directionLengthSquared);
                }
                float radius = pow(generator.Generate1d(), 1.0f / 3.0f) * gEmitter.radius;
                particle.translate = gEmitter.translate + direction * radius;
                particle.velocity = (generator.Generate3d() - 0.5f) * 0.5f;
                particle.color.rgb = generator.Generate3d();
                particle.color.a = 1.0f;
                gParticles[particleIndex] = particle;
            }
        }
    }
}
