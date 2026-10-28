//TEST:SIMPLE(filecheck=CHECK): -target glsl -glsl-es -relaxed-half -no-mangle -entry fs_main -stage fragment

// -relaxed-half under -glsl-es: a texture whose every read is converted to half (a cast of the
// read, of a swizzle or of an element of it, or an assignment to a half) is declared mediump, as
// a Texture2D<half4> is; one read at float anywhere keeps it highp. A query that reads no texels
// (GetDimensions) counts neither way.

// CHECK-DAG: uniform mediump sampler2D base_map;
// CHECK-DAG: uniform mediump sampler2D mask_map;
// CHECK-DAG: uniform mediump sampler2D raw_map;
// CHECK-DAG: uniform mediump sampler2DArray layers;
// CHECK-DAG: uniform highp sampler2D full_map;

Texture2D<float4> base_map;
SamplerState base_sampler;
Texture2D<float4> mask_map;
SamplerState mask_sampler;
Texture2D<float4> full_map;
SamplerState full_sampler;
Texture2DArray<float4> layers;
SamplerState layer_sampler;
Texture2D<float4> raw_map;

half4 fs_main(float4 position : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{
    uint width, height;
    raw_map.GetDimensions(width, height);
    half4 color = (half4)base_map.Sample(base_sampler, uv);
    half mask = (half)mask_map.Sample(mask_sampler, uv).r;
    half3 raw = raw_map.Load(int3(int2(position.xy) % int2(width, height), 0)).rgb;
    half4 layer = (half4)layers.Sample(layer_sampler, float3(uv, 1.0));
    float4 full = full_map.Sample(full_sampler, uv);
    return color * mask * half4(raw, 1.0) * layer * (half4)(full * 2.0);
}
