#pragma once

#include <array>
#include <string_view>

namespace Engine
{
	namespace ShaderCB
	{
		inline constexpr std::string_view Material = "MaterialBuffer";
		inline constexpr std::string_view Camera = "CameraBuffer";
		inline constexpr std::string_view Object = "ObjectBuffer";
		inline constexpr std::string_view Light = "LightBuffer";
		inline constexpr std::string_view Screen = "ScreenBuffer";
		inline constexpr std::string_view Skeletal = "SkeletalBuffer";
		inline constexpr std::string_view Shadow = "ShadowBuffer";
		inline constexpr std::string_view PostProcess = "PostProcessBuffer";

		inline constexpr std::array<std::string_view, ETOI(ConstantBuffer::END)> Names =
		{
			Material,
			Camera,
			Object,
			Light,
			Screen,
			Skeletal,
			Shadow,
			PostProcess,
		};
	}

	constexpr const Char* WorldMatrix = "g_WorldMatrix";
	constexpr const Char* ViewMatrix = "g_ViewMatrix";
	constexpr const Char* ProjMatrix = "g_ProjMatrix";
	constexpr const Char* BoneMatrices = "g_BoneMatrices";


	constexpr const Char* DefaultMap = "g_Texture";
	constexpr const Char* DiffuseMap = "g_DiffuseTexture";
	constexpr const Char* NormalMap = "g_NormalTexture";
	constexpr const Char* ShadeMap = "g_ShadeTexture";
	constexpr const Char* MaterialMap = "g_MaterialTexture";
	constexpr const Char* DepthMap = "g_DepthTexture";
	constexpr const Char* SpecularMap = "g_SpecularTexture";
	constexpr const Char* ShadowMap = "g_ShadowTexture";
	constexpr const Char* BakedShadowMap = "g_BakedShadowTexture";
	constexpr const Char* AOMap = "g_AOTexture";
	constexpr const Char* VolumetricMap = "g_VolumetricTexture";
	constexpr const Char* BloomMap = "g_BloomTexture";
	constexpr const Char* EmissiveMap = "g_EmissiveTexture";
	constexpr const Char* SceneMap = "g_SceneTexture";


	constexpr const Char* DirectionLight = "g_LightDir";
	constexpr const Char* DiffuseLight = "g_LightDiffuse";
	constexpr const Char* SpecularLight = "g_LightSpecular";
	constexpr const Char* AmbientLight = "g_LightAmbient";



	const std::wstring RT_DIFFUSE = L"RenderTargetDiffuse";
	const std::wstring RT_NORMAL = L"RenderTargetNormal";
	const std::wstring RT_SHADE = L"RenderTargetShade";
	const std::wstring RT_MATERIAL = L"RenderTargetMaterial";
	const std::wstring RT_DEPTH = L"RenderTargetDepth";
	const std::wstring RT_SPECULAR = L"RenderTargetSpecular";
	// Copy of the lit screen taken before RENDERGROUP::DISTORTION, which samples it with offsets.
	const std::wstring RT_SCENE = L"RenderTargetScene";


	const std::wstring MRT_GameObject = L"MultiRenderTargetGameObjects";
	const std::wstring MRT_LIGHT = L"MultiRenderTargetLights";
	const std::wstring RT_AO_RAW = L"RenderTargetAORaw";	// SSAO 원본 (노이즈 있음)
	const std::wstring RT_AO = L"RenderTargetAO";			// 깊이 인지 블러 후
	const std::wstring MRT_SSAO = L"MultiRenderTargetSSAO";
	const std::wstring MRT_SSAO_BLUR = L"MultiRenderTargetSSAOBlur";
	const std::wstring RT_VOLUMETRIC_RAW = L"RenderTargetVolumetricRaw";	// 볼류메트릭 산란 원본 (노이즈 있음)
	const std::wstring RT_VOLUMETRIC = L"RenderTargetVolumetric";			// 블러 후
	const std::wstring MRT_VOLUMETRIC = L"MultiRenderTargetVolumetric";
	const std::wstring MRT_VOLUMETRIC_BLUR = L"MultiRenderTargetVolumetricBlur";
	const std::wstring RT_BLOOM_H = L"RenderTargetBloomH";	// 블룸: 밝은 부분을 가로로 흐린 것
	const std::wstring RT_BLOOM = L"RenderTargetBloom";		// 세로까지 흐린 결과 (화면 보정이 더한다)
	const std::wstring MRT_BLOOM_H = L"MultiRenderTargetBloomH";
	const std::wstring MRT_BLOOM = L"MultiRenderTargetBloom";
	// RENDERGROUP::EMISSIVE draws here (HDR, over the scene depth); the grade adds it and feeds it to the bloom unthresholded.
	const std::wstring RT_EMISSIVE = L"RenderTargetEmissive";
	const std::wstring MRT_EMISSIVE = L"MultiRenderTargetEmissive";

	/* Directional light shadows. Static casters (STATIC group, world tiles) are baked once per level into one map over
	   the level's static collision; moving casters (NONBLEND) go into camera-centred cascades every view.
	   Both are fixed size, so they are not resized with the views. */
	inline constexpr uint32 SHADOW_CASCADE_COUNT = 2;		// dynamic cascades (one depth texture array, one slice each)
	inline constexpr uint32 SHADOW_MAP_SIZE = 2048;
	inline constexpr Float SHADOW_DISTANCE = 60.f;			// view depth the dynamic cascades cover; characters farther cast nothing
	inline constexpr Float SHADOW_SPLIT_LAMBDA = 0.7f;		// practical split: 1 = logarithmic, 0 = uniform
	inline constexpr Float SHADOW_CASCADE_OVERLAP = 0.3f;	// share of each cascade, at its far end, blended into the next (the last fades out)
	inline constexpr Float SHADOW_CASTER_MARGIN = 500.f;	// light-space depth kept in front of a shadow box for off-screen casters
	inline constexpr Float SHADOW_MIN_CASTER_TEXELS = 2.f;	// a caster whose half-diagonal is below this many texels is skipped
	inline constexpr Float SHADOW_DEPTH_BIAS = 0.03f;		// compare bias in metres along the light. It was 0.0005 of the cascade depth
															// range (2 radius + caster margin, 0.26~0.33 m), which cut the shadow off below the ankles
	inline constexpr uint32 BAKED_SHADOW_MAP_SIZE = 8192;	// static map, D16 (128MB); Level 3 gives about 0.1 m texels
	inline constexpr Float BAKED_SHADOW_DEPTH_BIAS = 0.0001f;	// compare bias over the baked depth range (about 1 km: 0.1 m)

	/* 가림 컬링(F6): 화면 패스 깊이를 칸마다 가장 먼 값으로 줄여 CPU가 1~2프레임 늦게 읽는다.
	   DeferredShader의 PS_DepthReduce가 같은 칸 크기를 쓴다. */
	inline constexpr uint32 OCCLUSION_DEPTH_BLOCK = 8;		// 줄인 깊이 텍셀 하나가 덮는 View 픽셀 (가로·세로)
	inline constexpr uint32 OCCLUSION_STAGING_COUNT = 3;	// GPU를 기다리지 않고 읽으려고 돌려 쓰는 스테이징 수
	inline constexpr Float OCCLUSION_MAX_EYE_SHIFT = 1.f;	// 깊이를 그린 카메라에서 이만큼(m) 넘게 옮겨진 View(컷 전환)는 그 깊이로 판정하지 않는다

	/* 화면 공간 환경광 차폐. 환경광 · 환경 반사에 곱하고, 직사광 diffuse에는 절반만 곱한다(EngineShaderMaterial의 SSAO_DIRECT_SHARE). */
	inline constexpr Float SSAO_RADIUS = 1.5f;		// 샘플 반구 반지름 (월드 m)
	inline constexpr Float SSAO_INTENSITY = 2.f;	// 차폐 비율에 곱하는 세기
	inline constexpr Float SSAO_BIAS = 0.02f;		// 자기 차폐를 막는 View 깊이 여유 (m)

	/* 볼류메트릭 라이트(Ctrl+F4): 시선을 따라 캐스케이드 그림자 맵을 찍어 방향광이 공기 중에 닿는 구간만 밝힌다
	   (건물 틈의 빛줄기). 같은 밀도로 거리 헤이즈(환경광 색)도 더한다. 화면 보정 패스가 합성한다. */
	inline constexpr Float VOLUMETRIC_INTENSITY = 0.15f;	// 해 색에 곱하는 산란 세기
	inline constexpr Float VOLUMETRIC_DENSITY = 0.006f;		// 공기 밀도 (1/m): 산란과 헤이즈 감쇠
	inline constexpr Float VOLUMETRIC_DISTANCE = 150.f;		// 시선을 따라가는 최대 거리 (m), 헤이즈도 여기서 멈춘다
	inline constexpr Float VOLUMETRIC_ANISOTROPY = 0.5f;	// Henyey-Greenstein g: 해 쪽을 볼수록 밝다
	inline constexpr Float GLOW_BLOOM = 3.f;				// 발광(RT_EMISSIVE)을 블룸에 넣는 세기, 레벨 블룸 세기와 따로

	/* Ctrl+F1 "MRT Preview": centre and size of each tile in view pixels plus its label. Renderer::Render_Debug draws the
	   tiles from this table and the Editor and Launcher label them with it. A label is one line per '\n': the target and
	   its format, the MRT (and slot) it is bound in, the pass that writes it and the passes that read it. */
	struct RenderTargetDebugTile
	{
		const std::wstring& tag;
		const char* label;
		float x;
		float y;
		float size;
	};
	const RenderTargetDebugTile RenderTargetDebugTiles[] =
	{
		{ RT_DIFFUSE, "RT_DIFFUSE  RGBA8\nMRT_GameObject #0: albedo\nwrite STATIC, NONBLEND, DECAL\nread Render_Lights", 150.f, 150.f, 300.f },
		{ RT_NORMAL, "RT_NORMAL  RGBA16\nMRT_GameObject #1: n*0.5+0.5\nwrite STATIC, NONBLEND, DECAL\nread Render_SSAO, Render_Lights", 150.f, 450.f, 300.f },
		{ RT_MATERIAL, "RT_MATERIAL  RGBA8\nMRT_GameObject #2: metal, rough, AO\nwrite STATIC, NONBLEND, DECAL\nread Render_Lights", 150.f, 750.f, 300.f },
		{ RT_SHADE, "RT_SHADE  RGBA16\nMRT_LIGHT #0: diffuse\nwrite Render_Lights\nread Render_Deferred: COMBINED", 450.f, 150.f, 300.f },
		{ RT_SPECULAR, "RT_SPECULAR  RGBA16F\nMRT_LIGHT #1: GGX specular\nwrite Render_Lights\nread Render_Deferred: COMBINED", 450.f, 450.f, 300.f },
		{ RT_DEPTH, "RT_DEPTH  R32F\nMRT_GameObject #3: depth 0..1\nwrite STATIC, NONBLEND, DECAL\nread SSAO, Volumetric, Lights, Grade, DEPTH_REDUCE", 450.f, 750.f, 300.f },
		{ RT_AO_RAW, "RT_AO_RAW  R8\nMRT_SSAO\nwrite Render_SSAO: SSAO\nread SSAO_BLUR", 750.f, 150.f, 300.f },
		{ RT_AO, "RT_AO  R8\nMRT_SSAO_BLUR\nwrite Render_SSAO: SSAO_BLUR\nread Render_Lights: DIRECTIONAL", 750.f, 450.f, 300.f },
		{ RT_VOLUMETRIC_RAW, "RT_VOLUMETRIC_RAW  R16F\nMRT_VOLUMETRIC\nwrite Render_Volumetric: VOLUMETRIC\nread VOLUMETRIC_BLUR", 1050.f, 150.f, 300.f },
		{ RT_VOLUMETRIC, "RT_VOLUMETRIC  R16F\nMRT_VOLUMETRIC_BLUR\nwrite Render_Volumetric: VOLUMETRIC_BLUR\nread Render_Grade: GRADE", 1050.f, 450.f, 300.f },
		{ RT_EMISSIVE, "RT_EMISSIVE  R11G11B10F\nMRT_EMISSIVE\nwrite RENDERGROUP::EMISSIVE\nread Render_Grade: BLOOM_H, GRADE", 1050.f, 750.f, 300.f },
		{ RT_BLOOM_H, "RT_BLOOM_H  R11G11B10F\nMRT_BLOOM_H\nwrite Render_Grade: BLOOM_H\nread BLOOM_V", 1350.f, 150.f, 300.f },
		{ RT_BLOOM, "RT_BLOOM  R11G11B10F\nMRT_BLOOM\nwrite Render_Grade: BLOOM_V\nread GRADE", 1350.f, 450.f, 300.f },
	};
	/* The shadow map is a depth array outside RenderTargetManager, so Renderer::Render_Debug draws its
	   cascade 0 slice itself at this tile; the Editor and Launcher label it together with the table above. */
	const std::wstring SHADOW_MAP_DEBUG = L"BakedShadowMap";
	const RenderTargetDebugTile ShadowMapDebugTile{ SHADOW_MAP_DEBUG,
		"BakedShadowMap  D16\ncascade 0 slice\nwrite Render_Shadows: STATIC bake\nread Lights, Volumetric, Grade", 750.f, 750.f, 300.f };
}
