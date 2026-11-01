#pragma once
#include <windows.h>

#ifdef CLIENT_EXPORTS
#define CLIENT_DLL __declspec(dllexport)
#else
#define CLIENT_DLL __declspec(dllimport)
#endif

namespace Client
{
	// Gameplay stages start at GAMEPLAY (3, City of Ruins); each later stage is its own level (4: amusement park).
	enum class LEVEL { STATIC, LOADING, TITLE, GAMEPLAY, AMUSEMENT_PARK, LEVEL_END };
	enum class CAMERA_EVENT { CITY_ARRIVAL, PARK_ARRIVAL, END };
	enum class ATK_TYPE { LIGHT, HEAVY, POD, END };

	inline constexpr const Char* CameraPosition = "g_CameraPosition";

	// Shader_VtxAnimMesh_Weapon.hlsl (VTXANIMMESH layout + "_Weapon" variant): weapons and sheaths.
	inline constexpr const wchar_t* WeaponShaderTag = L"VTX_Animation_Mesh_Weapon";
	// Shader_VtxMesh_Map.hlsl (VTXMESH layout + "_Map" variant): WorldMap tiles and their _LOD models.
	inline constexpr const wchar_t* MapShaderTag = L"VTX_Mesh_Map";
	// Shader_VtxMeshInstance_Map.hlsl (VTXMESH_INSTANCE layout + "_Map"): the WorldMap Layout props, drawn instanced.
	inline constexpr const wchar_t* MapInstanceShaderTag = L"VTX_Mesh_Instance_Map";

	// 특수 값
	inline constexpr const Char* HpRatio = "g_HpRatio"; // Tex Pass1
}

using namespace Client;
