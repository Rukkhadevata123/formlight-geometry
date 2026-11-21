struct Grid
{
    uint uSegments, vSegments;
    uint uPoles, vPoles;
    uint uDegree, vDegree;
};

[[vk::push_constant]] ConstantBuffer<Grid> grid;
[[vk::binding(0, 0)]] RWByteAddressBuffer data;

float scalar(uint word)
{
    return asfloat(data.Load(4 * word));
}

// Return the first contributing pole and its degree+1 basis coefficients.
uint basisAt(uint knots, uint count, uint degree, float t, out float basis[26])
{
    uint low = degree, high = count;
    while (low + 1 < high)
    {
        uint mid = low + (high - low) / 2;
        if (t < scalar(knots + mid))
            high = mid;
        else
            low = mid;
    }
    uint span = low; // At the upper endpoint use the final nonempty span.
    basis[0] = 1;
    for (uint j = 1; j <= degree; ++j)
    {
        float saved = 0;
        for (uint r = 0; r < j; ++r)
        {
            float a = scalar(knots + span + r + 1 - j);
            float b = scalar(knots + span + r + 1);
            float n = basis[r];
            basis[r] = saved + ((b - t) / (b - a)) * n;
            saved = ((t - a) / (b - a)) * n;
        }
        basis[j] = saved;
    }
    return span - degree;
}

[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint3 id : SV_DispatchThreadID)
{
    if (id.x > grid.uSegments || id.y > grid.vSegments)
        return;

    uint columns = grid.uSegments + 1, rows = grid.vSegments + 1;
    uint poles = 4 * columns * rows;
    uint uKnots = poles + 4 * grid.uPoles * grid.vPoles;
    uint uSamples = uKnots + grid.uPoles + grid.uDegree + 1;
    uint vKnots = uSamples + columns;
    uint vSamples = vKnots + grid.vPoles + grid.vDegree + 1;
    float bu[26], bv[26];
    uint u0 = basisAt(uKnots, grid.uPoles, grid.uDegree, scalar(uSamples + id.x), bu);
    uint v0 = basisAt(vKnots, grid.vPoles, grid.vDegree, scalar(vSamples + id.y), bv);
    float4 sum = 0;
    for (uint j = 0; j <= grid.vDegree; ++j)
    {
        for (uint i = 0; i <= grid.uDegree; ++i)
        {
            uint pole = (v0 + j) * grid.uPoles + u0 + i;
            float4 p = asfloat(data.Load4(4 * (poles + 4 * pole)));
            sum += float4(p.xyz, 1) * (p.w * bu[i] * bv[j]);
        }
    }
    data.Store4(16 * (id.y * columns + id.x), asuint(float4(sum.xyz / sum.w, sum.w)));
}
