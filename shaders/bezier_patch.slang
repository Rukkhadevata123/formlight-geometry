struct Grid
{
    uint uSegments;
    uint vSegments;
};

[[vk::push_constant]] ConstantBuffer<Grid> grid;
[[vk::binding(0, 0)]] RWByteAddressBuffer mesh;

float4 bernstein(float t)
{
    const float s = 1.0 - t;
    return float4(s*s*s, 3*t*s*s, 3*t*t*s, t*t*t);
}

float4 derivative(float t)
{
    const float s = 1.0 - t;
    return 3.0 * float4(-s*s, s*s - 2*t*s, 2*t*s - t*t, t*t);
}

[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint3 id : SV_DispatchThreadID)
{
    if (id.x > grid.uSegments || id.y > grid.vSegments)
    {
        return;
    }

    const float u = float(id.x) / grid.uSegments;
    const float v = float(id.y) / grid.vSegments;
    const float4 bu = bernstein(u), bv = bernstein(v);
    const float4 du = derivative(u), dv = derivative(v);
    const uint columns = grid.uSegments + 1;
    // Packed graphics vertices start at byte zero. The 16 float4 poles follow the active vertices.
    const uint poleOffset = columns * (grid.vSegments + 1) * 24;
    float3 p = 0, tangentU = 0, tangentV = 0;
    for (uint j = 0; j < 4; ++j)
    {
        for (uint i = 0; i < 4; ++i)
        {
            const float3 pole = asfloat(mesh.Load3(poleOffset + 16 * (4*j + i)));
            p += pole * (bu[i] * bv[j]);
            tangentU += pole * (du[i] * bv[j]);
            tangentV += pole * (bu[i] * dv[j]);
        }
    }
    const float3 crossUV = cross(tangentU, tangentV);
    const float magnitude = length(crossUV);
    const float3 normal = magnitude > 1e-12 ? crossUV / magnitude : float3(0, 0, 0);
    const uint address = (columns * id.y + id.x) * 24;
    mesh.Store3(address, asuint(p));
    mesh.Store3(address + 12, asuint(normal));
}
