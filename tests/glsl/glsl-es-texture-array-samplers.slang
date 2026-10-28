//TEST:SIMPLE(filecheck=CHECK): -target glsl -glsl-es -no-mangle -entry fs_main -stage fragment

// -glsl-es pairs each sampled texture with its sampler as one combined sampler; an array of
// textures is paired the same way, element by element: a combined array for each sampler that
// reads it (<texture>_<sampler> when there are several), and the array under its own name when
// it is also read without a sampler.

// CHECK-DAG: uniform highp sampler2D single[2];
// CHECK-DAG: uniform highp sampler2D pair_first[2];
// CHECK-DAG: uniform highp sampler2D pair_second[2];
// CHECK-DAG: uniform highp sampler2D pair[2];
// CHECK-DAG: texture(single[1],
// CHECK-DAG: texture(pair_first[0],
// CHECK-DAG: texture(pair_second[1],
// CHECK-DAG: texelFetch((pair[1])

Texture2D<float4> single[2];
SamplerState single_sampler;
Texture2D<float4> pair[2];
SamplerState first;
SamplerState second;

float4 fs_main(float4 position : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{
    return single[0].Sample(single_sampler, uv) + single[1].Sample(single_sampler, uv)
        + pair[0].Sample(first, uv) + pair[1].Sample(second, uv) + pair[1].Load(int3(0, 0, 0));
}
