//TEST:SIMPLE(filecheck=METAL): -target metal -legacy-semantics -entry fs_main -stage fragment
//TEST:SIMPLE(filecheck=HLSL): -target hlsl -profile sm_5_0 -legacy-semantics -entry fs_main -stage fragment

// -legacy-semantics: a VPOS parameter beside a struct input whose POSITION is the same system
// value (SV_Position) reads that one, as Metal takes [[position]] once: the parameter is removed
// and its uses read the struct's position (its leading elements for a float2).

// METAL: {{\[\[position\]\]}}
// METAL-NOT: {{\[\[position\]\]}}
// METAL: .xy

// HLSL: SV_Position
// HLSL-NOT: SV_Position

struct FragmentInput
{
    float4 position : POSITION;
    float2 uv : TEXCOORD0;
};

float4 fs_main(FragmentInput input, float2 screen : VPOS) : COLOR
{
    return float4(input.uv, screen / 512.0);
}
