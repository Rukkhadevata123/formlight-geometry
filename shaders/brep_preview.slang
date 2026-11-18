struct MeshVertex
{
    [[vk::location(0)]] float3 position : POSITION;
    [[vk::location(1)]] float3 normal : NORMAL;
};

struct VertexOutput
{
    float4 position : SV_Position;
    [[vk::location(0)]] float3 normal : NORMAL;
};

struct ClipTransform
{
    float4 xRow;
    float4 yRow;
    float4 zRow;
    float4 wRow;
};

[[vk::push_constant]] ConstantBuffer<ClipTransform> clipTransform;

[shader("vertex")]
VertexOutput vertexMain(MeshVertex input)
{
    const float4 position = float4(input.position, 1.0);
    VertexOutput output;
    output.position = float4(dot(position, clipTransform.xRow), dot(position, clipTransform.yRow),
                             dot(position, clipTransform.zRow), dot(position, clipTransform.wRow));
    output.normal = input.normal;
    return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target0
{
    const float3 light = normalize(float3(0.3, 0.65, 1.0));
    const float brightness = 0.22 + 0.78 * saturate(dot(normalize(input.normal), light));
    return float4(float3(0.09, 0.5, 0.68) * brightness, 1.0);
}
