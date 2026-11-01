#pragma once

#include "Engine_Enum.h"
#include "Engine_Typedef.h"

namespace Engine
{
	enum MATERIAL_TEXTURE_MASK : uint32
	{
		MATERIAL_TEXTURE_BASE_COLOR = 1u << 0,
		MATERIAL_TEXTURE_NORMAL = 1u << 1,
		MATERIAL_TEXTURE_ORM = 1u << 2,
		MATERIAL_TEXTURE_EMISSIVE = 1u << 3,
		MATERIAL_TEXTURE_OPACITY = 1u << 4,
	};

	// MaterialCB::materialFlags. Same bits as MATERIAL_FLAG_* in EngineShaderMaterial.hlsli.
	enum MATERIAL_FLAG : uint32
	{
		MATERIAL_FLAG_METALLIC_OVERRIDE = 1u << 0,  // NieR g_bMetalicOverWrite
		MATERIAL_FLAG_ROUGHNESS_OVERRIDE = 1u << 1, // NieR g_bGlossinessOverWrite (roughness = 1 - g_Glossiness)
		MATERIAL_FLAG_MASK_SRGB = 1u << 2,          // decode g_MaskMap RGB as sRGB, as Blender does for W_PBS
		MATERIAL_FLAG_UV2 = 1u << 3,                // sample with the second UV set (NieR g_UV2Use)
		MATERIAL_FLAG_UNPREMULTIPLY_ALPHA = 1u << 4, // albedo color / alpha: DDS cutouts filled with black
	};

	struct alignas(16) MaterialCB
	{
		static constexpr ConstantBuffer CBType = ConstantBuffer::Material;
		Vector4 baseColorTint{ 1.f, 1.f, 1.f, 1.f };
		Vector4 emissiveColor{ 0.f, 0.f, 0.f, 1.f };
		Float emissiveIntensity{};
		Float normalStrength{ 1.f };
		Float opacityMaskClipValue{ 0.3f };
		Float aoScale{ 1.f };
		Float roughnessScale{ 1.f };
		Float metalnessScale{ 1.f };
		uint32 textureMask{};
		uint32 materialFlags{};
		Float metallic{};
		Float roughness{ 1.f };
		Float detailNormalTileX{ 1.f };
		Float detailNormalTileY{ 1.f };
		Float detailNormalWeight{ 1.f };
		Float intensity{ 1.f };
		Float uvTileX{ 1.f };
		Float uvTileY{ 1.f };
		Float uvTile2X{ 1.f };
		Float uvTile2Y{ 1.f };
		Float padding[2]{};
	};

	// Per-material values authored in the material settings document ("parameters").
	// Material keeps them and sends them through MaterialCB; omitted keys keep these defaults,
	// so documents written before the parameters existed render exactly as before.
	struct MaterialParameters
	{
		Vector4 baseColorTint{ 1.f, 1.f, 1.f, 1.f };
		Vector4 emissiveColor{ 0.f, 0.f, 0.f, 1.f };
		Float emissiveIntensity{};
		Float normalStrength{ 1.f };
		Float opacityMaskClipValue{ 0.3f };
		Float aoScale{ 1.f };
		Float roughnessScale{ 1.f };
		Float metalnessScale{ 1.f };
		Bool metallicOverride{ false };
		Float metallic{};
		Bool roughnessOverride{ false };
		Float roughness{ 1.f };
		Bool maskSrgb{ false };
		Float detailNormalTileX{ 1.f };
		Float detailNormalTileY{ 1.f };
		Float detailNormalWeight{ 1.f };
		Float intensity{ 1.f };
		Float uvTileX{ 1.f };
		Float uvTileY{ 1.f };
		Float uvTile2X{ 1.f };
		Float uvTile2Y{ 1.f };
		Bool useUV2{ false };
		Bool unpremultiplyAlpha{ false };

		MaterialCB To_MaterialCB(uint32 textureMask) const
		{
			MaterialCB data{};
			data.baseColorTint = baseColorTint;
			data.emissiveColor = emissiveColor;
			data.emissiveIntensity = emissiveIntensity;
			data.normalStrength = normalStrength;
			data.opacityMaskClipValue = opacityMaskClipValue;
			data.aoScale = aoScale;
			data.roughnessScale = roughnessScale;
			data.metalnessScale = metalnessScale;
			data.textureMask = textureMask;
			data.materialFlags = (metallicOverride ? MATERIAL_FLAG_METALLIC_OVERRIDE : 0u) |
				(roughnessOverride ? MATERIAL_FLAG_ROUGHNESS_OVERRIDE : 0u) |
				(maskSrgb ? MATERIAL_FLAG_MASK_SRGB : 0u) | (useUV2 ? MATERIAL_FLAG_UV2 : 0u) |
				(unpremultiplyAlpha ? MATERIAL_FLAG_UNPREMULTIPLY_ALPHA : 0u);
			data.metallic = metallic;
			data.roughness = roughness;
			data.detailNormalTileX = detailNormalTileX;
			data.detailNormalTileY = detailNormalTileY;
			data.detailNormalWeight = detailNormalWeight;
			data.intensity = intensity;
			data.uvTileX = uvTileX;
			data.uvTileY = uvTileY;
			data.uvTile2X = uvTile2X;
			data.uvTile2Y = uvTile2Y;
			return data;
		}
	};

	struct alignas(16) CameraCB
	{
		static constexpr ConstantBuffer CBType = ConstantBuffer::Camera;
		Matrix viewMatrix{ Matrix::Identity };
		Matrix projMatrix{ Matrix::Identity };
		Matrix viewInverseMatrix{ Matrix::Identity };
		Matrix projInverseMatrix{ Matrix::Identity };
		Vector4 cameraPosition{ 0.f, 0.f, 0.f, 1.f };
		Vector4 time{};								// x seconds the main timer has run (time scale applied)
	};

	struct alignas(16) ObjectCB
	{
		static constexpr ConstantBuffer CBType = ConstantBuffer::Object;
		Matrix worldMatrix{ Matrix::Identity };
	};

	struct alignas(16) LightCB
	{
		static constexpr ConstantBuffer CBType = ConstantBuffer::Light;
		Vector4 direction{};
		Vector4 position{};
		Vector4 diffuse{};
		Vector4 ambient{};
		Vector4 specular{};
		Float range{};
		Float3 spot{};								// cos outer cone, cos inner cone, beam scattering (1/m; 0 = none)
	};

	// Screen-space quad pass (deferred lighting, MRT preview): orthographic projection of the viewport.
	struct alignas(16) ScreenCB
	{
		static constexpr ConstantBuffer CBType = ConstantBuffer::Screen;
		Matrix projMatrix{ Matrix::Identity };
		Vector4 texel{};	// 1/w, 1/h, w, h
		Vector4 ssao{};		// 반경, 강도, 바이어스, 켜짐(0/1)
	};

	// 화면 후처리(PostProcessShader): 화면 보정과 볼류메트릭 라이트. 해의 색·방향은 LightCB로 받는다.
	struct alignas(16) PostProcessCB
	{
		static constexpr ConstantBuffer CBType = ConstantBuffer::PostProcess;
		Vector4 grade{ 1.f, 1.f, 1.f, 0.f };	// 노출, 대비, 채도, 블룸 세기
		Vector4 tint{ 1.f, 1.f, 1.f, 1.f };	// 색조(곱)
		Vector4 volumetric{};				// 산란 세기, 밀도, 최대 거리, 켜짐(0/1)
		Vector4 volumetricPhase{};			// x Henyey-Greenstein g
		Vector4 fog{};						// 레벨 안개 색(rgb), 최대 불투명도(w). 0이면 끈다
		Vector4 fogParams{};				// x 안개 거리(m): 이 거리에서 최대의 63%, yzw 해 쪽 안개에 더하는 색
		Vector4 glow{};						// x 이번 View에 RT_EMISSIVE를 그림(0/1), y 발광을 블룸에 넣는 세기, z 글리치 색수차 세기, w 띠 밀림 세기
	};

	// Skinning palette of one SkeletalMesh. Bind_Matrices uploads only the mesh's bones and zeroes the rest,
	// so this struct documents the layout rather than being filled on the CPU.
	struct alignas(16) SkeletalCB
	{
		static constexpr ConstantBuffer CBType = ConstantBuffer::Skeletal;
		Matrix boneMatrices[MODEL_BONE_MAX];
	};

	// Directional light cascaded shadow, filled by Renderer::Render_Lights for the view being lit.
	struct alignas(16) ShadowCB
	{
		static constexpr ConstantBuffer CBType = ConstantBuffer::Shadow;
		Matrix cascadeMatrices[SHADOW_CASCADE_COUNT]{};	// world -> dynamic cascade UV (xy) and light depth (z)
		Matrix bakedMatrix{};							// world -> baked static map UV (xy) and light depth (z)
		Vector4 splitDepths{};							// view depth where each dynamic cascade ends
		Vector4 params{};								// x strength (0 = no shadow this view), y depth bias, z 1 / map size, w blend share
		Vector4 bakedParams{};							// x 1 = baked map valid, y depth bias, z 1 / map size, w texel world size
		Vector4 debug{};								// x 1 = tint the lit scene by cascade (Editor Debug menu)
	};

	static_assert(sizeof(MaterialCB) == 112);
	static_assert(sizeof(CameraCB) == 288);
	static_assert(sizeof(ObjectCB) == 64);
	static_assert(sizeof(LightCB) == 96);
	static_assert(sizeof(ScreenCB) == 96);
	static_assert(sizeof(PostProcessCB) == 112);
	static_assert(sizeof(SkeletalCB) == 64 * MODEL_BONE_MAX);
	static_assert(sizeof(ShadowCB) == 64 * SHADOW_CASCADE_COUNT + 64 + 64);
}
