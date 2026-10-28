//TEST:SIMPLE(filecheck=HLSL): -target hlsl -profile sm_5_0 -no-mangle -entry fs_main -stage fragment
//TEST:SIMPLE(filecheck=GLSL): -target glsl -no-mangle -entry fs_main -stage fragment
//TEST:SIMPLE(filecheck=METAL): -target metal -no-mangle -entry fs_main -stage fragment

// -no-mangle gives a constant buffer's fields and every global their own names. A field and a
// global of the same name (here a texture in a namespace) are still two symbols: the field is
// linked under its type, as any field is, and not merged with the global, which crashed.

// HLSL: float metallic_map;
// HLSL: Texture2D<float4 > metallic_map
// HLSL: Globals.metallic_map
// HLSL: metallic_map.Sample(

// GLSL: float metallic_map;
// GLSL: metallic_map

// METAL: float metallic_map;
// METAL: metallic_map

cbuffer Globals
{
    float metallic_map;
};

namespace textures
{
    Texture2D<float4> metallic_map;
}

SamplerState metallic_sampler;

float4 fs_main(float4 position : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{
    if (metallic_map > 0.5)
        return textures::metallic_map.Sample(metallic_sampler, uv);
    return 0;
}
