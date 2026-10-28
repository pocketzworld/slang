//TEST:SIMPLE(filecheck=CHECK): -target glsl -glsl-es -entry fs_main -stage fragment
//TEST:SIMPLE(filecheck=DESKTOP): -target glsl -entry fs_main -stage fragment

// -glsl-es: a texture is a sampler, so texelFetch and textureSize of a texture read without a
// sampler are GLSL ES 3.00's own and no GL_EXT_samplerless_texture_functions is asked for (ES
// has no such extension). Desktop GLSL still asks for it.

// CHECK: #version 300 es
// CHECK-NOT: GL_EXT_samplerless_texture_functions
// CHECK: texelFetch(

// DESKTOP: #extension GL_EXT_samplerless_texture_functions : require

Texture2D<float4> raw;

float4 fs_main(float4 position : SV_Position) : SV_Target
{
    uint width, height;
    raw.GetDimensions(width, height);
    return raw.Load(int3(int2(position.xy) % int2(width, height), 0));
}
