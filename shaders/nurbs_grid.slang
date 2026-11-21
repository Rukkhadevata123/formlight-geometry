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

// Differentiate with respect to the local span coordinate. Its positive scale
// keeps the oriented normal and avoids reciprocals of tiny knot intervals.
void basisAt(uint knots, uint span, uint degree, float t, out float basis[26], out float derivative[26])
{
    float width = scalar(knots + span + 1) - scalar(knots + span);
    basis[0] = 1;
    for (uint j = 1; j <= degree; ++j)
    {
        float saved = 0;
        float previousSlope = 0;
        for (uint r = 0; r < j; ++r)
        {
            float a = scalar(knots + span + r + 1 - j);
            float b = scalar(knots + span + r + 1);
            float n = basis[r];
            if (j == degree)
            {
                float slope = n * (width / (b - a));
                derivative[r] = float(degree) * (previousSlope - slope);
                previousSlope = slope;
            }
            basis[r] = saved + ((b - t) / (b - a)) * n;
            saved = ((t - a) / (b - a)) * n;
        }
        basis[j] = saved;
        if (j == degree)
            derivative[j] = float(degree) * previousSlope;
    }
}

float3 unitDirection(float3 tangent)
{
    float scale = max(max(abs(tangent.x), abs(tangent.y)), abs(tangent.z));
    return scale > 0 ? normalize(tangent / scale) : float3(0);
}

uint strongestBasis(float basis[26], uint degree)
{
    uint best = 0;
    for (uint i = 1; i <= degree; ++i)
        if (basis[i] > basis[best])
            best = i;
    return best;
}

float4 surfaceNormal(float3 position, float4 du, float4 dv)
{
    // The quotient rule leaves a positive common denominator, immaterial to the unit normal.
    float3 tu = du.xyz - position * du.w;
    float3 tv = dv.xyz - position * dv.w;
    if (!all(isfinite(tu)) || !all(isfinite(tv)))
        return float4(0, 0, 0, -1);
    float3 n = cross(unitDirection(tu), unitDirection(tv));
    float magnitude = length(n);
    // Below eight FP32 epsilons the two tangent directions are numerically parallel.
    return magnitude > 9.53674316e-7f ? float4(n / magnitude, 1) : float4(0);
}

[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint3 id : SV_DispatchThreadID)
{
    if (id.x > grid.uSegments || id.y > grid.vSegments)
        return;

    uint columns = grid.uSegments + 1, rows = grid.vSegments + 1;
    uint poles = 8 * columns * rows;
    uint uKnots = poles + 4 * grid.uPoles * grid.vPoles;
    uint uSamples = uKnots + grid.uPoles + grid.uDegree + 1;
    uint vKnots = uSamples + 2 * columns;
    uint vSamples = vKnots + grid.vPoles + grid.vDegree + 1;
    uint uSample = uSamples + 2 * id.x, vSample = vSamples + 2 * id.y;
    uint uInfo = data.Load(4 * (uSample + 1)), vInfo = data.Load(4 * (vSample + 1));
    uint uSpan = uInfo >> 1, vSpan = vInfo >> 1;
    float bu[26], bv[26], dbu[26], dbv[26];
    basisAt(uKnots, uSpan, grid.uDegree, scalar(uSample), bu, dbu);
    basisAt(vKnots, vSpan, grid.vDegree, scalar(vSample), bv, dbv);
    uint u0 = uSpan - grid.uDegree, v0 = vSpan - grid.vDegree;
    // A contributing anchor keeps a collapsed edge exactly constant, even with unequal weights.
    uint anchorPole = (v0 + strongestBasis(bv, grid.vDegree)) * grid.uPoles + u0 + strongestBasis(bu, grid.uDegree);
    float3 anchor = asfloat(data.Load3(4 * (poles + 4 * anchorPole)));
    float4 sum = 0, du = 0, dv = 0;
    for (uint j = 0; j <= grid.vDegree; ++j)
    {
        for (uint i = 0; i <= grid.uDegree; ++i)
        {
            uint pole = (v0 + j) * grid.uPoles + u0 + i;
            float4 p = asfloat(data.Load4(4 * (poles + 4 * pole)));
            float4 offset = float4(p.xyz - anchor, 1);
            sum += offset * (p.w * bu[i] * bv[j]);
            du += offset * (p.w * dbu[i] * bv[j]);
            dv += offset * (p.w * bu[i] * dbv[j]);
        }
    }
    float3 offsetPosition = sum.xyz / sum.w;
    bool crease = ((uInfo | vInfo) & 1) != 0;
    uint address = 32 * (id.y * columns + id.x);
    data.Store4(address, asuint(float4(anchor + offsetPosition, sum.w)));
    data.Store4(address + 16, asuint(crease ? float4(0) : surfaceNormal(offsetPosition, du, dv)));
}
