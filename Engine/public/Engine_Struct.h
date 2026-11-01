#ifndef Engine_Struct_h__
#define Engine_Struct_h__

#include <string>
#include <cstdint>
#include <d3d11.h>
#include <DirectXMath.h>

using namespace DirectX;

namespace Engine
{
	typedef struct tagEngineDesc
	{
		HWND 			hWnd = {};
		HINSTANCE 		hInst = {};
		WINMODE			winMode = {};
		std::wstring	windowTitle = {};
		uint32_t		viewportWidth = {};
		uint32_t		viewportHeight = {};
		uint32_t		startLevel = {};
		uint32_t		levelCount = {};
		bool            useOffscreenRendering {};
		uint32_t         renderTargetCount = {};
		bool			useLog {};	/* Release only: the host keeps the engine log (Editor). Debug always logs. */
	} ENGINE_DESC;

	typedef struct tagLightDesc
	{
		LIGHT type = {};
		Vector4 direction = {};
		Vector4 position = {};
		float range = {};

		Vector4 diffuse = {};
		Vector4 ambient = {};
		Vector4 specular = {};

		/* SPOT: half angles (radians) of the cone around direction; full light inside innerCone, none past outerCone.
		   beam scatters the light toward the eye along the cone (1/m, 0 = no visible beam), drawn while volumetric is on. */
		float innerCone = {};
		float outerCone = {};
		float beam = {};
	} LIGHT_DESC;

	typedef struct tagTransformFrame
	{
		Vector3 scale;
		Quaternion rotation;
		Vector3 position;
	} TRANSFORM_FRAME;

	typedef struct tagKeyFrame
	{
		Vector3 scale;
		Vector4 rotation;
		Vector3 position;
		Float trackPosition; // Animation의 Duration 절대값 위치
	} KEYFRAME;

	typedef struct tagVertexPosition
	{
		Vector3			position;

		inline static const std::wstring Tag = { L"VTX_Pos" };
		static const uint32_t numElements = { 1 };
		static constexpr D3D11_INPUT_ELEMENT_DESC Elements[] = {
			{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0}
		};
	} VTXPOS;

	typedef struct tagVertexTexcoord
	{
		Vector3			position;
		Vector2			texcoord;

		inline static const std::wstring Tag = { L"VTX_Tex" };
		static const uint32_t numElements = { 2 };
		static constexpr D3D11_INPUT_ELEMENT_DESC Elements[] = {
			{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}
		};
	} VTXTEX;

	typedef struct tagVertexNormalTexcoord
	{
		Vector3			position;
		Vector3			normal;
		Vector2			texcoord;

		inline static const std::wstring Tag = { L"VTX_NormalTex" };
		static const uint32_t numElements = { 3 };
		static constexpr D3D11_INPUT_ELEMENT_DESC Elements[] = {
			{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0}
		};
	} VTXNORMTEX;

	typedef struct tagVertexMesh
	{
		Vector3			position;
		Vector3			normal;
		Vector3			tangent;
		Vector2			texcoord;
		Vector2			texcoord1;  // second UV set (FBX UVMap2), MODEL_VERSION 3
		uint32_t		color;      // vertex color RGBA8 (byte order R, G, B, A), sRGB-encoded, MODEL_VERSION 3

		inline static const std::wstring Tag = {L"VTX_Mesh"};
		static const uint32_t numElements = { 6 };
		static constexpr D3D11_INPUT_ELEMENT_DESC Elements[] = {
			{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0, 44, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 52, D3D11_INPUT_PER_VERTEX_DATA, 0}
		};
	} VTXMESH;

	/* 인스턴스 하나의 입력(80바이트). world는 그리는 오브젝트의 로컬 공간 행렬이고, 셰이더가 g_WorldMatrix를 곱한다.
	   params는 엔진이 뜻을 붙이지 않는 값 네 개로, 쓰는 셰이더가 해석한다. */
	typedef struct tagInstanceData
	{
		Matrix			world;
		Vector4			params;
	} INSTANCE_DATA;

	// VTXMESH(slot 0) + INSTANCE_DATA(slot 1, 인스턴스마다): WORLD0~3, INSTPARAM0.
	typedef struct tagVertexMeshInstanceDesc
	{
		inline static const std::wstring Tag = { L"VTX_Mesh_Instance" };
		static const uint32_t numElements = { 11 };
		static constexpr D3D11_INPUT_ELEMENT_DESC Elements[] = {
			{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0, 44, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 52, D3D11_INPUT_PER_VERTEX_DATA, 0},

			{"WORLD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
			{"WORLD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
			{"WORLD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
			{"WORLD", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
			{"INSTPARAM", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 64, D3D11_INPUT_PER_INSTANCE_DATA, 1 }
		};
	} VTXMESH_INSTANCE;

	typedef struct tagVertexAnimationMesh
	{
		Vector3			position;
		Vector3			normal;
		Vector3			tangent;
		Vector2			texcoord;

		XMUINT4			blendIndex; 
		Vector4			blendWeight;

		inline static const std::wstring Tag = { L"VTX_Animation_Mesh" };
		static const uint32_t numElements = { 6 };
		static constexpr D3D11_INPUT_ELEMENT_DESC Elements[] = {
			{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"BLENDINDEX", 0, DXGI_FORMAT_R32G32B32A32_UINT, 0, 44, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"BLENDWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 60, D3D11_INPUT_PER_VERTEX_DATA, 0}
		};

	} VTXANIMMESH;

	typedef struct tagVertexCube
	{
		Vector3				position;
		Vector3				texcoord;

		inline static const std::wstring Tag = { L"VTX_Cube" };
		static const uint32_t numElements = { 2 };
		static constexpr D3D11_INPUT_ELEMENT_DESC	Elements[] = {
			{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"TEXCOORD", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA,0 }
		};
	} VTXCUBE;

	typedef struct tagVertexSphere
	{
		Vector3				position;
		Vector3				texcoord;

		inline static const std::wstring Tag = { L"VTX_Sphere" };
		static const uint32_t numElements = { 2 };
		static constexpr D3D11_INPUT_ELEMENT_DESC	Elements[] = {
			{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"TEXCOORD", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA,0 }
		};
	} VTXSPHERE;

	typedef struct tagVertexParticleInstance
	{
		Vector4 right;
		Vector4 up;
		Vector4 look;
		Vector4 translation;
		Vector2 lifeTime;
	} VTXPARTICLE_INSTANCE;

	typedef struct tagVertexParticlePointDesc
	{
		inline static const std::wstring Tag = { L"VTX_Particle_Point" };
		static const uint32_t numElements = { 6 };
		static constexpr D3D11_INPUT_ELEMENT_DESC Elements[] = {
			{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
			
			{"WORLD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
			{"WORLD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
			{"WORLD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
			{"WORLD", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
			{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 1, 64, D3D11_INPUT_PER_INSTANCE_DATA, 1}
		};
	} VTXPARTICLE_POINT_DESC;

	typedef struct tagVertexParticleRectDesc
	{
		inline static const std::wstring Tag = {L"VTX_Particle_Rect"};
		static const uint32_t numElements = { 7 };
		static constexpr D3D11_INPUT_ELEMENT_DESC Elements[] = {
			{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA,0 },

			{"WORLD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
			{"WORLD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
			{"WORLD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
			{"WORLD", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
			{"TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 1, 64, D3D11_INPUT_PER_INSTANCE_DATA, 1}
		};
	} VTXPARTICLE_RECT_DESC;

	

}

#endif // Engine_Struct_h__
