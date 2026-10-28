//TEST:SIMPLE(filecheck=CHECK): -target glsl -glsl-es -entry vs_main -stage vertex -matrix-layout-column-major
//TEST:SIMPLE(filecheck=DESKTOP): -target glsl -entry vs_main -stage vertex -matrix-layout-column-major

// -glsl-es: a column of a matrix that is no local variable (a uniform block's, which is row_major
// there, or a function parameter such a matrix is handed to) is taken from a copy of the matrix.
// PowerVR Rogue's GLSL ES compiler refuses `Globals_cb.m[0]` and the same column of a parameter
// given it ("Compile failed"); a whole element and a column of a copy compile. A local
// variable's column and a whole element are written as before, and desktop GLSL is unchanged.

// CHECK: return mat3x3(mat4x4(m_{{[0-9]+}})[0].xyz, mat4x4(m_{{[0-9]+}})[1].xyz, mat4x4(m_{{[0-9]+}})[2].xyz);
// CHECK: mat3x3(mat4x4(Globals_cb.world_to_object{{(_0)?}})[0].xyz, mat4x4(Globals_cb.world_to_object{{(_0)?}})[1].xyz, mat4x4(Globals_cb.world_to_object{{(_0)?}})[2].xyz)
// CHECK: mat4x4(Globals_cb.world_to_object{{(_0)?}})[2] + {{.*}}Globals_cb.world_to_object{{(_0)?}}[0][3]

// DESKTOP: return mat3x3(m_{{[0-9]+}}[0].xyz, m_{{[0-9]+}}[1].xyz, m_{{[0-9]+}}[2].xyz);
// DESKTOP-NOT: mat4x4(Globals

cbuffer Globals
{
    float4x4 projection;
    float4x4 world_to_object;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float3 a : TEXCOORD0;
    float3 b : TEXCOORD1;
    float4 c : TEXCOORD2;
};

float3x3 upper(float4x4 m)
{
    return (float3x3)m;
}

VertexOutput vs_main(float3 position : POSITION, float3 normal : NORMAL)
{
    VertexOutput output;
    output.position = mul(projection, float4(position, 1.0));
    output.a = mul(normal, upper(world_to_object));
    output.b = mul((float3x3)world_to_object, normal);
    output.c = world_to_object[2] + float4(world_to_object[0][3], 0, 0, 0);
    return output;
}
