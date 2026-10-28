//TEST:SIMPLE(filecheck=HLSL): -target hlsl -profile sm_5_0 -legacy-semantics -entry fs_main -stage fragment
//TEST:SIMPLE(filecheck=GLSL): -target glsl -legacy-semantics -entry fs_main -stage fragment
//TEST:SIMPLE(filecheck=METAL): -target metal -legacy-semantics -entry fs_main -stage fragment

// -legacy-semantics: a `float face : VFACE` field of a fragment input struct is the front-face
// system value, handed as its bool and read as VFACE's +1 or -1, as a VFACE parameter is.

// HLSL: bool face{{.*}} : SV_IsFrontFace;
// HLSL: ? 1.0{{.*}} : -1.0

// GLSL: gl_FrontFacing
// GLSL: ? 1.0 : -1.0

// METAL: bool {{.*}}front_facing
// METAL: select(-1.0{{.*}}, 1.0{{.*}}, face

struct FragmentInput
{
    float2 uv : TEXCOORD0;
    float face : VFACE;
};

float4 fs_main(FragmentInput input) : COLOR
{
    return float4(input.uv, input.face, 1.0);
}
