//TEST:SIMPLE(filecheck=HLSL): -target hlsl -profile sm_5_0 -relaxed-half -entry fs_main -stage fragment
//TEST:SIMPLE(filecheck=METAL): -target metal -relaxed-half -entry fs_main -stage fragment

// -relaxed-half stores the half fields of a constant buffer's structs as float. A whole array of
// halves written into one (stored into a local copy, or made into one) is written element by
// element, each converted to its float storage, and one read from a local copy is read element
// by element: no target converts an array as one value.

// HLSL: float4 {{ *}}colors{{.*}}[int(2)];
// HLSL-NOT: .colors{{[_0-9]*}} =
// HLSL: colors{{.*}}[int(0)] = float4(
// HLSL: colors{{.*}}[int(1)] = float4(
// HLSL-NOT: .colors{{[_0-9]*}} =

// METAL-NOT: colors{{[_0-9]*}} =
// METAL: colors{{.*}}[int(0)] = float4(
// METAL-NOT: colors{{[_0-9]*}} =

struct Light
{
    half4 colors[2];
    float4 direction;
};

cbuffer Globals
{
    half4 palette[2];
    Light light;
};

half4 fs_main(float4 position : SV_Position) : SV_Target
{
    Light copy = light;
    copy.colors = palette;
    Light made = { palette, light.direction };
    half4 local[2] = { (half4)position, palette[1] };
    made.colors = copy.colors;
    copy.colors = local;
    return copy.colors[0] + made.colors[1] * (half)made.direction.x;
}
