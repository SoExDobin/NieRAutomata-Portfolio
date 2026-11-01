#ifndef ENGINE_SHADER_VERTEX_HLSLI
#define ENGINE_SHADER_VERTEX_HLSLI

// Vertex layouts that mirror Engine_Struct.h. Member order and types must match the C++ struct and
// its Elements[] byte offsets, or the input layout and any compute read of the same buffer break.
// One definition serves both uses:
//   - VS input: the semantics bind to Elements[] (typedef it as VS_IN in the shader).
//   - Compute: StructuredBuffer<VTXANIMMESH> packs tightly, so the stride equals sizeof(C++ struct).
//     Semantics are ignored there.

// VTXMESH: 56 bytes (offsets 0, 12, 24, 36, 44, 52). MODEL_VERSION 3 added texcoord1 (FBX UVMap2) and
// color (FBX vertex color, RGBA8 UNORM, sRGB-encoded as Blender exports it; decode with Node_VertexColor).
// Version 2 models load with texcoord1 = texcoord and color = white.
struct VTXMESH
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float2 texcoord : TEXCOORD0;
    float2 texcoord1 : TEXCOORD1;
    float4 color : COLOR;
#ifdef MESH_INSTANCED
    // VTXMESH_INSTANCE slot 1 (INSTANCE_DATA, 80 bytes): the instance's matrix in the object's local space (rows) and
    // four values the including shader gives a meaning.
    float4 instanceWorld0 : WORLD0;
    float4 instanceWorld1 : WORLD1;
    float4 instanceWorld2 : WORLD2;
    float4 instanceWorld3 : WORLD3;
    float4 instanceParams : INSTPARAM0;
#endif
};

// VTXANIMMESH: 76 bytes (offsets 0, 12, 24, 36, 44, 60)
struct VTXANIMMESH
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float2 texcoord : TEXCOORD0;
    uint4 blendIndex : BLENDINDEX;
    float4 blendWeight : BLENDWEIGHT;
};

#include "EngineShaderConstantBuffer.hlsli"   // g_BoneMatrices (SkeletalBuffer), g_WorldMatrix, g_ViewMatrix, g_ProjMatrix

// OBJECT_WORLD: the object-to-world matrix the Node_Object* functions use. It is g_WorldMatrix, except in a shader that
// defines MESH_INSTANCED before its includes (VTXMESH_INSTANCE layout): there each vertex shader that draws instances
// starts with BEGIN_OBJECT_WORLD(input), which sets it to the instance matrix times g_WorldMatrix.
#ifdef MESH_INSTANCED
static float4x4 g_ObjectWorld;
#define OBJECT_WORLD g_ObjectWorld
#define BEGIN_OBJECT_WORLD(input) g_ObjectWorld = mul(float4x4(input.instanceWorld0, input.instanceWorld1, \
    input.instanceWorld2, input.instanceWorld3), g_WorldMatrix)
#else
#define OBJECT_WORLD g_WorldMatrix
#define BEGIN_OBJECT_WORLD(input)
#endif

// Object space -> clip space with the object's world (OBJECT_WORLD), view and projection matrices.
float4 Node_ObjectToClip(float4 position)
{
    return mul(position, mul(mul(OBJECT_WORLD, g_ViewMatrix), g_ProjMatrix));
}

float4 Node_ObjectToClip(float3 position)
{
    return Node_ObjectToClip(float4(position, 1.f));
}

// Object-space direction (normal, tangent) -> unit world-space direction.
float3 Node_ObjectToWorldDirection(float3 direction)
{
    return normalize(mul(float4(direction, 0.f), OBJECT_WORLD).xyz);
}

// Linear blend skinning matrix for a VTXANIMMESH vertex. The converter stores three weights;
// the fourth is 1 - (x + y + z).
float4x4 Node_SkinMatrix(VTXANIMMESH input)
{
    float4 weights = input.blendWeight;
    weights.w = 1.f - (weights.x + weights.y + weights.z);
    return g_BoneMatrices[input.blendIndex.x] * weights.x
         + g_BoneMatrices[input.blendIndex.y] * weights.y
         + g_BoneMatrices[input.blendIndex.z] * weights.z
         + g_BoneMatrices[input.blendIndex.w] * weights.w;
}

#endif
