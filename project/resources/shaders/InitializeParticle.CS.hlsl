struct Particle
{
    float32_t3 translate;
    float32_t3 scale;
    float lifeTime;
    float32_t3 velocity;
    float currentTime;
    float32_t4 color;
};

static const uint32_t kMaxParticles = 1024;
RWStructuredBuffer<Particle> gParticles : register(u0);

[numthreads(1024, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    const uint32_t particleIndex = DTid.x;
    if (particleIndex < kMaxParticles)
    {
        Particle particle = (Particle)0;
        particle.scale = float32_t3(0.5f, 0.5f, 0.5f);
        particle.color = float32_t4(1.0f, 1.0f, 1.0f, 1.0f);
        gParticles[particleIndex] = particle;
    }
}
