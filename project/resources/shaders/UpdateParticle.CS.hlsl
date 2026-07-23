struct Particle
{
    float32_t3 translate;
    float32_t3 scale;
    float lifeTime;
    float32_t3 velocity;
    float currentTime;
    float32_t4 color;
};

struct PerFrame
{
    float time;
    float deltaTime;
};

static const uint32_t kMaxParticles = 1024;
RWStructuredBuffer<Particle> gParticles : register(u0);
ConstantBuffer<PerFrame> gPerFrame : register(b1);

[numthreads(1024, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    const uint32_t particleIndex = DTid.x;
    if (particleIndex < kMaxParticles && gParticles[particleIndex].color.a != 0.0f)
    {
        Particle particle = gParticles[particleIndex];
        particle.translate += particle.velocity * gPerFrame.deltaTime;
        particle.currentTime += gPerFrame.deltaTime;
        float alpha = 1.0f - particle.currentTime / particle.lifeTime;
        particle.color.a = saturate(alpha);
        gParticles[particleIndex] = particle;
    }
}
