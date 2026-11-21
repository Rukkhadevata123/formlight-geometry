struct PyramidParameters
{
    uint heightMm;
};

[[vk::binding(0, 0)]] RWByteAddressBuffer vertices;
[[vk::push_constant]] ConstantBuffer<PyramidParameters> parameters;

void writeVertex(uint index, float3 position, float3 normal)
{
    // Match the graphics pipeline's MeshVertex: two tightly packed float3 attributes.
    const uint address = index * 24;
    vertices.Store3(address, asuint(position));
    vertices.Store3(address + 12, asuint(normal));
}

[shader("compute")]
[numthreads(6, 1, 1)]
void computeMain(uint3 thread : SV_DispatchThreadID)
{
    const uint triangle = thread.x;
    const float halfHeight = 0.5 * float(parameters.heightMm);
    const float3 points[5] = {
        float3(-40, -40, -halfHeight), float3(40, -40, -halfHeight),
        float3(40, 40, -halfHeight), float3(-40, 40, -halfHeight), float3(0, 0, halfHeight)
    };
    const uint3 faces[6] = {
        uint3(0, 1, 4), uint3(1, 2, 4), uint3(2, 3, 4), uint3(3, 0, 4),
        uint3(0, 2, 1), uint3(0, 3, 2)
    };
    const uint3 face = faces[triangle];
    const float3 a = points[face.x];
    const float3 b = points[face.y];
    const float3 c = points[face.z];
    const float3 normal = normalize(cross(b - a, c - a));
    writeVertex(3 * triangle, a, normal);
    writeVertex(3 * triangle + 1, b, normal);
    writeVertex(3 * triangle + 2, c, normal);
}
