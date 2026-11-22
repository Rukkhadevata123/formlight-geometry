struct MeshVertex
{
    [[vk::location(0)]] float3 position : POSITION;
    [[vk::location(1)]] float4 normal : NORMAL;
};

struct VertexOutput
{
    float4 position : SV_Position;
    [[vk::location(0)]] float4 normal : NORMAL;
    [[vk::location(1)]] float3 localPosition : TEXCOORD0;
};

struct MeshParameters
{
    float4 xRow;
    float4 yRow;
    float4 zRow;
    float4 wRow;
    uint selected;
};

[[vk::push_constant]] ConstantBuffer<MeshParameters> mesh;

[shader("vertex")]
VertexOutput vertexMain(MeshVertex input)
{
    const float4 position = float4(input.position, 1.0);
    VertexOutput output;
    output.position = float4(dot(position, mesh.xRow), dot(position, mesh.yRow),
                             dot(position, mesh.zRow), dot(position, mesh.wRow));
    // Vulkan supplies W=1 for the existing three-component mesh normals.
    output.normal = float4(input.normal.xyz, input.normal.w == 1 ? 1 : 0);
    output.localPosition = input.position;
    return output;
}

float3 unitOrZero(float3 v)
{
    float scale = max(max(abs(v.x), abs(v.y)), abs(v.z));
    return scale > 0 ? normalize(v / scale) : float3(0);
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input, bool front : SV_IsFrontFace) : SV_Target0
{
    const float3 light = normalize(float3(0.3, 0.65, 1.0));
    // Screen Y increases downward. The geometric fallback faces the viewer on either side.
    float3 facet = cross(unitOrZero(ddy(input.localPosition)), unitOrZero(ddx(input.localPosition)));
    float3 normal = input.normal.w < 0.99999 ? unitOrZero(facet) : unitOrZero(front ? input.normal.xyz : -input.normal.xyz);
    const float brightness = 0.22 + 0.78 * saturate(dot(normal, light));
    const float3 color = mesh.selected != 0 ? float3(1.0, 0.52, 0.08) : float3(0.09, 0.5, 0.68);
    return float4(color * brightness, 1.0);
}
