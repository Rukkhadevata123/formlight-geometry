struct CheckParameters
{
    uint elementCount;
};

[[vk::binding(0, 0)]] RWStructuredBuffer<uint> outputValues;
[[vk::push_constant]] ConstantBuffer<CheckParameters> parameters;

[shader("compute")]
[numthreads(64, 1, 1)]
void computeMain(uint3 thread : SV_DispatchThreadID)
{
    uint index = thread.x;
    if (index < parameters.elementCount)
    {
        outputValues[index] = 3u * index + 7u;
    }
}
