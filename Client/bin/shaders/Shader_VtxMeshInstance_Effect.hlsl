// Instanced bullets (Bullet batches, one draw per kind): Shader_VtxMesh_Effect.hlsl with the VTXMESH_INSTANCE input,
// registered as VTX_Mesh_Instance_Effect. The bullet passes read the age from instance params.x.
#define MESH_INSTANCED
#include "Shader_VtxMesh_Effect.hlsl"
