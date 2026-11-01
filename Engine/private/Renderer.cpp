#include "Renderer.h"
#include "Game.h"
#include "GameObject.h"
#include "Transform.h"
#include "PartObject.h"
#include "Model.h"
#include "VIBuffer_Rect.h"
#include "SpdLogger.h"

Renderer::Renderer(const ComPtr<ID3D11Device> &device,
                   const ComPtr<ID3D11DeviceContext> &context)
    : m_Device(device), m_Context(context) {}

void Renderer::Add_RenderGroup(RENDERGROUP renderGroup, const Shared<GameObject>& gameObject) {
	// A child that submits itself is drawn in its own group; its parent's traversal skips it (Render_Recursive).
	if (gameObject->Get_Parent())
		m_SubmittedChildren.insert(gameObject.get());

	m_RenderGroup[ETOI(renderGroup)].push_back(gameObject);
}

void Renderer::Draw() {
	Draw_NoClearing();
	Clear_RenderGroup();
}

void Renderer::Draw_NoClearing(uint32 screenIndex) {
	// 가림 판정은 이 View가 전에 그린 깊이로 한다. 그림자 패스는 판정하지 않으므로 순서는 상관없다.
	m_OcclusionView = &m_OcclusionViews[screenIndex];
	Read_OcclusionDepth(*m_OcclusionView);

	Render_Shadows();

	UINT count = 1;
	D3D11_VIEWPORT viewport{};
	m_Context->RSGetViewports(&count, &viewport);
	m_WorldMatrix = Matrix::CreateScale(viewport.Width, viewport.Height, 1.f);
	// One screen projection per view; the light, combine and preview quads all share it.
	ScreenCB screenBuffer{};
	screenBuffer.projMatrix = Matrix::CreateOrthographic(viewport.Width, viewport.Height, 0.f, 1.f);
	screenBuffer.texel = Vector4{ 1.f / viewport.Width, 1.f / viewport.Height, viewport.Width, viewport.Height };
	screenBuffer.ssao = Vector4{ m_SSAOParams.x, m_SSAOParams.y, m_SSAOParams.z, m_SSAO ? 1.f : 0.f };
	if (FAILED(m_Shader->Bind_CBufferData(screenBuffer)) || FAILED(m_PostShader->Bind_CBufferData(screenBuffer)))
		return;
	// 볼류메트릭은 활성 방향광(LightCB)의 색·방향으로 산란시킨다. 방향광이 없는 레벨(타이틀·로딩)은 끈다.
	m_VolumetricReady = m_Volumetric && GAME_INSTANCE->Find_ActiveLight(LIGHT::DIRECTIONAL);
	// Ctrl+F5~F7로 끈 항목은 레벨 값을 중립으로 둔다. 레벨이 매 프레임 다시 넣고 프레임 끝에 어차피 중립으로 돌아간다.
	if (!m_FogOn)
		m_FogOpacity = 0.f;
	if (!m_BloomOn)
		m_Bloom = 0.f;
	if (!m_ColorGradeOn)
	{
		m_ColorGrade = Vector3{ 1.f, 1.f, 1.f };
		m_ColorTint = Color{ 1.f, 1.f, 1.f, 1.f };
	}
	PostProcessCB postBuffer{};
	postBuffer.grade = Vector4{ m_ColorGrade.x, m_ColorGrade.y, m_ColorGrade.z, m_Bloom };
	postBuffer.tint = Vector4{ m_ColorTint.x, m_ColorTint.y, m_ColorTint.z, 1.f };
	postBuffer.volumetric = Vector4{ m_VolumetricParams.x, m_VolumetricParams.y, m_VolumetricParams.z, m_VolumetricReady ? 1.f : 0.f };
	postBuffer.volumetricPhase = Vector4{ VOLUMETRIC_ANISOTROPY, 0.f, 0.f, 0.f };
	postBuffer.fog = Vector4{ m_FogColor.x, m_FogColor.y, m_FogColor.z, m_FogDistance > 0.f ? m_FogOpacity : 0.f };
	postBuffer.fogParams = Vector4{ (std::max)(m_FogDistance, 1e-3f), m_FogSunColor.x, m_FogSunColor.y, m_FogSunColor.z };
	// 그룹은 View를 그리기 전에 다 모여 있으므로, 발광을 그릴지 여기서 안다.
	m_GlowReady = !m_RenderGroup[ETOI(RENDERGROUP::EMISSIVE)].empty();
	m_Glitch = Get_GlitchStrength(GAME_INSTANCE->Get_TotalTime() - m_GlitchStart);
	postBuffer.glow = Vector4{ m_GlowReady ? 1.f : 0.f, GLOW_BLOOM, m_Glitch, m_GlitchBands ? m_Glitch : 0.f };
	if (FAILED(m_PostShader->Bind_CBufferData(postBuffer)))
		return;
	Render_Group(ETOI(RENDERGROUP::PRIORITY));

	Render_Group(ETOI(RENDERGROUP::NONLIGHT));

	// G-buffer: Begin_MultiRenderTarget clears the targets, so STATIC and NONBLEND share one open.
	if (SUCCEEDED(GAME_INSTANCE->Begin_MultiRenderTarget(MRT_GameObject)))
	{
		Render_Group(ETOI(RENDERGROUP::STATIC));
		Render_Group(ETOI(RENDERGROUP::NONBLEND));
		// 데칼은 모든 불투명 메시 뒤에 같은 G-buffer로 섞는다(다른 타일의 바닥이 나중에 덮어쓰지 않게).
		Render_Group(ETOI(RENDERGROUP::DECAL));
		GAME_INSTANCE->End_MultiRenderTarget();
		if (m_Occlusion)
			Capture_OcclusionDepth(*m_OcclusionView, viewport);
	}

	Render_SSAO();

	Render_Volumetric();

	Render_Lights();

	Render_Deferred();

	Render_Group(ETOI(RENDERGROUP::BLEND));
	// Distortion samples the finished screen through offsets, so it reads a copy taken here; no copy when nothing distorts.
	if (!m_RenderGroup[ETOI(RENDERGROUP::DISTORTION)].empty() &&
		SUCCEEDED(GAME_INSTANCE->Copy_BoundRenderTarget(RT_SCENE)))
		Render_Group(ETOI(RENDERGROUP::DISTORTION));
	// 발광은 장면 깊이로 가려지며 RT_EMISSIVE에 따로 그린다. 화면에는 화면 보정이 더한다.
	if (m_GlowReady)
	{
		if (SUCCEEDED(GAME_INSTANCE->Begin_MultiRenderTarget(MRT_EMISSIVE)))
		{
			Render_Group(ETOI(RENDERGROUP::EMISSIVE));
			GAME_INSTANCE->End_MultiRenderTarget();
		}
		else
			m_GlowReady = false;
	}
	// 화면 보정은 하늘·이펙트까지 끝난 화면에 한 번, UI 전에 건다.
	Render_Grade();
	Render_Group(ETOI(RENDERGROUP::WORLDUI));
	Render_Group(ETOI(RENDERGROUP::UI));
	Render_Group(ETOI(RENDERGROUP::UI_OVERLAY));

#ifdef _DEBUG
	Render_Debug();
#endif

}

HRESULT Renderer::Clear_RenderGroup()
{
    // 프레임마다 모든 View를 그린 뒤 한 번 불리므로 여기서 프레임 draw 통계를 넘긴다.
    m_LastDrawCount = m_DrawCount;
    m_LastTriangleCount = m_TriangleCount;
    m_LastInstanceCount = m_InstanceCount;
    m_DrawCount = 0;
    m_TriangleCount = 0;
    m_InstanceCount = 0;
    m_LastCulledCount = m_CulledCount;
    m_CulledCount = 0;
    m_LastOccludedCount = m_OccludedCount;
    m_OccludedCount = 0;
    m_LastLodTileCount = m_LodTileCount;
    m_LodTileCount = 0;
#ifdef _DEBUG
    m_LastCascadeDraws = m_CascadeDraws;
    m_CascadeDraws.fill(0);
    m_LastCascadeMs = m_CascadeMs;
    m_CascadeMs.fill(0.f);
    m_LastShadowBakes = m_ShadowBakes;
    m_ShadowBakes = 0;
#endif

    m_LayerMask = ETOI(LAYER::ALL_LAYER);
    m_ColorGrade = Vector3{ 1.f, 1.f, 1.f };
    m_ColorTint = Color{ 1.f, 1.f, 1.f, 1.f };
    m_FogOpacity = 0.f;
    m_Bloom = 0.f;

    for (auto& group : m_RenderGroup)
        group.clear();
    m_SubmittedChildren.clear();

    return S_OK;
}

HRESULT Renderer::Initialize(void *arg) {
	m_LayerMask = ETOI(LAYER::ALL_LAYER);

	if (FAILED(EngineManager::Initialize(arg)))
	{
		LOG_ERROR(L"Failed to Initialize Renderer");
		return E_FAIL;
	}

	uint32			numViewports = { 1 };
	D3D11_VIEWPORT	viewportDesc{};

	m_Context->RSGetViewports(&numViewports, &viewportDesc);
	if (FAILED(GAME_INSTANCE->Add_RenderTarget(RT_DIFFUSE, viewportDesc.Width, viewportDesc.Height,
		DXGI_FORMAT_R8G8B8A8_UNORM, Vector4::Zero)))
	return E_FAIL;
	if (FAILED(GAME_INSTANCE->Add_RenderTarget(RT_NORMAL, viewportDesc.Width, viewportDesc.Height,
		DXGI_FORMAT_R16G16B16A16_UNORM, Vector4::Zero)))
	return E_FAIL;
	// Clears to the no-mask surface (metal 0, roughness 1, AO 1) and to the far plane.
	if (FAILED(GAME_INSTANCE->Add_RenderTarget(RT_MATERIAL, viewportDesc.Width, viewportDesc.Height,
		DXGI_FORMAT_R8G8B8A8_UNORM, Vector4{ 0.f, 1.f, 1.f, 0.f })))
	return E_FAIL;
	if (FAILED(GAME_INSTANCE->Add_RenderTarget(RT_DEPTH, viewportDesc.Width, viewportDesc.Height,
		DXGI_FORMAT_R32_FLOAT, Vector4::One)))
	return E_FAIL;
	if (FAILED(GAME_INSTANCE->Add_RenderTarget(RT_SHADE, viewportDesc.Width, viewportDesc.Height,
		DXGI_FORMAT_R16G16B16A16_FLOAT, Vector4::Zero)))
	return E_FAIL;
	// GGX highlights go above 1, so specular keeps a float format instead of UNORM.
	if (FAILED(GAME_INSTANCE->Add_RenderTarget(RT_SPECULAR, viewportDesc.Width, viewportDesc.Height,
		DXGI_FORMAT_R16G16B16A16_FLOAT, Vector4::Zero)))
	return E_FAIL;
	// SSAO: 1(가려지지 않음)로 클리어해서, 패스를 건너뛴 픽셀이 어두워지지 않게 한다.
	if (FAILED(GAME_INSTANCE->Add_RenderTarget(RT_AO_RAW, viewportDesc.Width, viewportDesc.Height,
		DXGI_FORMAT_R8_UNORM, Vector4::One)))
		return E_FAIL;
	if (FAILED(GAME_INSTANCE->Add_RenderTarget(RT_AO, viewportDesc.Width, viewportDesc.Height,
		DXGI_FORMAT_R8_UNORM, Vector4::One)))
		return E_FAIL;
	// 볼류메트릭 산란량(해 색에 곱할 값). 1을 넘을 수 있어 float이다.
	if (FAILED(GAME_INSTANCE->Add_RenderTarget(RT_VOLUMETRIC_RAW, viewportDesc.Width, viewportDesc.Height,
		DXGI_FORMAT_R16_FLOAT, Vector4::Zero)))
		return E_FAIL;
	if (FAILED(GAME_INSTANCE->Add_RenderTarget(RT_VOLUMETRIC, viewportDesc.Width, viewportDesc.Height,
		DXGI_FORMAT_R16_FLOAT, Vector4::Zero)))
		return E_FAIL;
	// 블룸은 보정 전 화면의 밝은 부분에 발광(RT_EMISSIVE, 1을 넘는다)을 더해 흐린 것이라 float이다.
	if (FAILED(GAME_INSTANCE->Add_RenderTarget(RT_BLOOM_H, viewportDesc.Width, viewportDesc.Height,
		DXGI_FORMAT_R11G11B10_FLOAT, Vector4::Zero)))
		return E_FAIL;
	if (FAILED(GAME_INSTANCE->Add_RenderTarget(RT_BLOOM, viewportDesc.Width, viewportDesc.Height,
		DXGI_FORMAT_R11G11B10_FLOAT, Vector4::Zero)))
		return E_FAIL;
	// 발광(기계 눈): 8비트 화면에 담기지 않는 1 넘는 빛을 따로 받아 화면 보정이 더하고 블룸에 넣는다.
	if (FAILED(GAME_INSTANCE->Add_RenderTarget(RT_EMISSIVE, viewportDesc.Width, viewportDesc.Height,
		DXGI_FORMAT_R11G11B10_FLOAT, Vector4::Zero)))
		return E_FAIL;
	// Same format as the back buffer and the editor view textures, so the screen can be copied into it.
	if (FAILED(GAME_INSTANCE->Add_RenderTarget(RT_SCENE, viewportDesc.Width, viewportDesc.Height,
		DXGI_FORMAT_R8G8B8A8_UNORM, Vector4::Zero)))
		return E_FAIL;

	// The order is the SV_TARGET order of PS_GBUFFER_OUT and PS_LIGHT_OUT.
	if (FAILED(GAME_INSTANCE->Add_MultiRenderTarget(MRT_GameObject, RT_DIFFUSE)))
		return E_FAIL;
	if (FAILED(GAME_INSTANCE->Add_MultiRenderTarget(MRT_GameObject, RT_NORMAL)))
		return E_FAIL;
	if (FAILED(GAME_INSTANCE->Add_MultiRenderTarget(MRT_GameObject, RT_MATERIAL)))
		return E_FAIL;
	if (FAILED(GAME_INSTANCE->Add_MultiRenderTarget(MRT_GameObject, RT_DEPTH)))
		return E_FAIL;

	if (FAILED(GAME_INSTANCE->Add_MultiRenderTarget(MRT_LIGHT, RT_SHADE)))
		return E_FAIL;
	if (FAILED(GAME_INSTANCE->Add_MultiRenderTarget(MRT_LIGHT, RT_SPECULAR)))
		return E_FAIL;

	if (FAILED(GAME_INSTANCE->Add_MultiRenderTarget(MRT_SSAO, RT_AO_RAW)))
		return E_FAIL;
	if (FAILED(GAME_INSTANCE->Add_MultiRenderTarget(MRT_SSAO_BLUR, RT_AO)))
		return E_FAIL;
	if (FAILED(GAME_INSTANCE->Add_MultiRenderTarget(MRT_VOLUMETRIC, RT_VOLUMETRIC_RAW)))
		return E_FAIL;
	if (FAILED(GAME_INSTANCE->Add_MultiRenderTarget(MRT_VOLUMETRIC_BLUR, RT_VOLUMETRIC)))
		return E_FAIL;
	if (FAILED(GAME_INSTANCE->Add_MultiRenderTarget(MRT_BLOOM_H, RT_BLOOM_H)))
		return E_FAIL;
	if (FAILED(GAME_INSTANCE->Add_MultiRenderTarget(MRT_BLOOM, RT_BLOOM)))
		return E_FAIL;
	if (FAILED(GAME_INSTANCE->Add_MultiRenderTarget(MRT_EMISSIVE, RT_EMISSIVE)))
		return E_FAIL;

	/* Cascaded shadow map. Depth-only and fixed-size, so it stays out of RenderTargetManager (colour
	   targets resized per view): the passes write a slice through its DSV, the light pass reads the array. */
	D3D11_TEXTURE2D_DESC shadowDesc{};
	shadowDesc.Width = SHADOW_MAP_SIZE;
	shadowDesc.Height = SHADOW_MAP_SIZE;
	shadowDesc.MipLevels = 1;
	shadowDesc.ArraySize = SHADOW_CASCADE_COUNT;
	shadowDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	shadowDesc.SampleDesc.Count = 1;
	shadowDesc.Usage = D3D11_USAGE_DEFAULT;
	shadowDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	if (FAILED(m_Device->CreateTexture2D(&shadowDesc, nullptr, m_ShadowTexture.GetAddressOf())))
	{
		LOG_ERROR(L"Failed to Initialize Renderer: shadow map texture could not be created");
		return E_FAIL;
	}
	for (uint32 cascade = 0; cascade < SHADOW_CASCADE_COUNT; ++cascade)
	{
		D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
		dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
		dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
		dsvDesc.Texture2DArray.FirstArraySlice = cascade;
		dsvDesc.Texture2DArray.ArraySize = 1;
		if (FAILED(m_Device->CreateDepthStencilView(m_ShadowTexture.Get(), &dsvDesc, m_ShadowDSVs[cascade].GetAddressOf())))
		{
			LOG_ERROR(L"Failed to Initialize Renderer: shadow map DSV {} could not be created", cascade);
			return E_FAIL;
		}
	}
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
	srvDesc.Texture2DArray.MipLevels = 1;
	srvDesc.Texture2DArray.ArraySize = SHADOW_CASCADE_COUNT;
	if (FAILED(m_Device->CreateShaderResourceView(m_ShadowTexture.Get(), &srvDesc, m_ShadowSRV.GetAddressOf())))
	{
		LOG_ERROR(L"Failed to Initialize Renderer: shadow map SRV could not be created");
		return E_FAIL;
	}
	m_ShadowViewport = { 0.f, 0.f, static_cast<Float>(SHADOW_MAP_SIZE), static_cast<Float>(SHADOW_MAP_SIZE), 0.f, 1.f };

	/* 정적 그림자 맵. 레벨 전체를 한 장에 담아 크므로 16비트 깊이로 둔다(8192² = 128MB). */
	D3D11_TEXTURE2D_DESC bakedDesc = shadowDesc;
	bakedDesc.Width = BAKED_SHADOW_MAP_SIZE;
	bakedDesc.Height = BAKED_SHADOW_MAP_SIZE;
	bakedDesc.ArraySize = 1;
	bakedDesc.Format = DXGI_FORMAT_R16_TYPELESS;
	if (FAILED(m_Device->CreateTexture2D(&bakedDesc, nullptr, m_BakedTexture.GetAddressOf())))
	{
		LOG_ERROR(L"Failed to Initialize Renderer: baked shadow map texture could not be created");
		return E_FAIL;
	}
	D3D11_DEPTH_STENCIL_VIEW_DESC bakedDsvDesc{};
	bakedDsvDesc.Format = DXGI_FORMAT_D16_UNORM;
	bakedDsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	D3D11_SHADER_RESOURCE_VIEW_DESC bakedSrvDesc{};
	bakedSrvDesc.Format = DXGI_FORMAT_R16_UNORM;
	bakedSrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	bakedSrvDesc.Texture2D.MipLevels = 1;
	if (FAILED(m_Device->CreateDepthStencilView(m_BakedTexture.Get(), &bakedDsvDesc, m_BakedDSV.GetAddressOf())) ||
		FAILED(m_Device->CreateShaderResourceView(m_BakedTexture.Get(), &bakedSrvDesc, m_BakedSRV.GetAddressOf())))
	{
		LOG_ERROR(L"Failed to Initialize Renderer: baked shadow map views could not be created");
		return E_FAIL;
	}

	m_Buffer = VIBuffer_Rect::Create(m_Device, m_Context);
	m_Shader = Shader::Create(m_Device, m_Context, L"shaders/DeferredShader.hlsl", VTXTEX::Elements, VTXTEX::numElements);
	m_PostShader = Shader::Create(m_Device, m_Context, L"shaders/PostProcessShader.hlsl", VTXTEX::Elements, VTXTEX::numElements);

	/* Every frame goes through Render_Lights, which dereferences both of these. Leaving them
	   null here turns a load failure into an access violation deep in Shader::Bind_Matrix. */
	if (nullptr == m_Buffer)
	{
		LOG_ERROR(L"Failed to Initialize Renderer: screen quad buffer could not be created");
		return E_FAIL;
	}
	if (nullptr == m_Shader)
	{
		LOG_ERROR(L"Failed to Initialize Renderer: 'shaders/DeferredShader.hlsl' could not be loaded; "
			L"check that the shaders folder is deployed next to the executable");
		return E_FAIL;
	}
	if (nullptr == m_PostShader)
	{
		LOG_ERROR(L"Failed to Initialize Renderer: 'shaders/PostProcessShader.hlsl' could not be loaded");
		return E_FAIL;
	}

	return S_OK;
}

void Renderer::On_Destroy() {
  for (auto &group : m_RenderGroup)
    group.clear();
  m_SubmittedChildren.clear();
}

void Renderer::On_Disable() { EngineManager::On_Disable(); }

void Renderer::On_Enable() { EngineManager::On_Enable(); }

void Renderer::Set_Active(Bool isActive) { EngineManager::Set_Active(isActive); }

void Renderer::Render_Shadows()
{
	m_ShadowReady = false;
	// The previous view's light pass left the shadow maps bound as PS inputs; they become depth targets here.
	ID3D11ShaderResourceView* nullViews[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT]{};
	m_Context->PSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullViews);
	if (!m_Shadow)
	{
		m_FarCascadeView = nullptr;
		return;
	}
	const LIGHT_DESC* light = GAME_INSTANCE->Find_ActiveLight(LIGHT::DIRECTIONAL);
	if (nullptr == light)
		return;
	Vector3 lightDirection{ light->direction.x, light->direction.y, light->direction.z };
	if (lightDirection.LengthSquared() < 1e-6f)
		return;
	lightDirection.Normalize();

	/* near/far는 카메라 객체가 아니라 PROJ에서 구한다. Editor 카메라는 행렬만 올린다.
	   원근 LH 투영: _33 = f/(f-n), _43 = -n·f/(f-n). */
	const Matrix cameraView = GAME_INSTANCE->Get_Transform(D3DTS::VIEW);
	const Matrix cameraProj = GAME_INSTANCE->Get_Transform(D3DTS::PROJ);
	if (cameraProj._34 == 0.f || cameraProj._33 == 0.f || cameraProj._33 == 1.f)
		return;
	const Float nearZ = -cameraProj._43 / cameraProj._33;
	const Float farZ = std::min(-cameraProj._43 / (cameraProj._33 - 1.f), SHADOW_DISTANCE);
	if (!(nearZ > 0.f) || !(farZ > nearZ))
		return;

	// 들어올 때의 View 렌더 타깃과 뷰포트. 그림자를 그린 뒤 그대로 되돌린다.
	ID3D11RenderTargetView* savedRTVs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT]{};
	ID3D11DepthStencilView* savedDSV{ nullptr };
	m_Context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, savedRTVs, &savedDSV);
	UINT savedViewportCount = 1;
	D3D11_VIEWPORT savedViewport{};
	m_Context->RSGetViewports(&savedViewportCount, &savedViewport);

	const Vector3 up = std::abs(lightDirection.y) > 0.99f ? Vector3::UnitZ : Vector3::UnitY;
	// 투영 결과(NDC)를 그림자 맵 UV로: x·y -1..1 → 0..1, y 뒤집기. 깊이는 그대로.
	const Matrix toTexture{ 0.5f, 0.f, 0.f, 0.f, 0.f, -0.5f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.5f, 0.5f, 0.f, 1.f };
	m_ShadowPass = true;

	/* 정적 그림자는 View·카메라와 상관없이 레벨에 한 장이다. 방향광이나 정적 그룹이 바뀔 때만 굽고,
	   카메라가 움직여도 다시 굽지 않는다. */
	const size_t staticSignature = Hash_StaticGroup();
	if (!m_BakedReady || m_BakedLightDirection != lightDirection || m_BakedSignature != staticSignature)
	{
		m_BakedLightDirection = lightDirection;
		m_BakedSignature = staticSignature;
		Bake_StaticShadow(lightDirection, up, toTexture);
		m_FarCascadeView = nullptr;
	}

	// practical split: 로그 분할과 균등 분할을 λ로 섞는다. splits[i]는 캐스케이드 i가 끝나는 View 깊이다.
	array<Float, SHADOW_CASCADE_COUNT + 1> splits{};
	for (uint32 i = 0; i <= SHADOW_CASCADE_COUNT; ++i)
	{
		const Float t = static_cast<Float>(i) / SHADOW_CASCADE_COUNT;
		splits[i] = SHADOW_SPLIT_LAMBDA * nearZ * std::pow(farZ / nearZ, t) +
			(1.f - SHADOW_SPLIT_LAMBDA) * (nearZ + (farZ - nearZ) * t);
	}

	/* 움직이는 캐스터(NONBLEND: 플레이어·적·무기)만 카메라 가까이의 캐스케이드에 매 프레임 그린다.
	   캐스케이드는 카메라 위치 중심의 구로 맞춘다. View 깊이 d의 프러스텀 모서리는 카메라에서 d·√(1 + tanX² + tanY²)
	   떨어져 있으므로 이 반경이면 캐스케이드가 맡는 깊이 전부를 덮는다. */
	const Vector3 cameraPosition = cameraView.Invert().Translation();
	const Float tanX = 1.f / cameraProj._11;
	const Float tanY = 1.f / cameraProj._22;
	const Float cornerScale = std::sqrt(1.f + tanX * tanX + tanY * tanY);
	// 원점에서 빛 방향을 보는 회전만 있는 View. 박스 중심을 텍셀 격자에 맞출 때 쓴다.
	const Matrix lightRotation = XMMatrixLookAtLH(Vector3::Zero, lightDirection, up);
	const Matrix lightRotationInverse = lightRotation.Invert();
	m_Context->RSSetViewports(1, &m_ShadowViewport);
	for (uint32 cascade = 0; cascade < SHADOW_CASCADE_COUNT; ++cascade)
	{
		// 먼 캐스케이드를 건너뛰는 프레임은 지난 프레임의 깊이·행렬·분할을 그대로 쓴다(캐릭터 그림자가 한 프레임 늦는다).
		if (0 < cascade && SHADOW_CASCADE_COUNT - 1 == cascade)
		{
			m_FarCascadeReused = !m_FarCascadeReused && m_FarCascadeView == m_OcclusionView && m_FarCascadeLight == lightDirection;
			if (m_FarCascadeReused)
				continue;
			m_FarCascadeView = m_OcclusionView;
			m_FarCascadeLight = lightDirection;
		}
#ifdef _DEBUG
		const uint32 drawsBefore = m_DrawCount;
		const auto timeBefore = std::chrono::steady_clock::now();
#endif
		const Float radius = std::ceil(splits[cascade + 1] * cornerScale * 16.f) / 16.f;
		const Float texel = 2.f * radius / SHADOW_MAP_SIZE;
		m_ShadowCascade = cascade;
		m_ShadowTexelSize = texel;

		// 중심을 라이트 공간 텍셀 격자에 맞춰 카메라가 움직여도 그림자 가장자리가 떨리지 않게 한다.
		Vector3 lightCenter = Vector3::Transform(cameraPosition, lightRotation);
		lightCenter.x = std::floor(lightCenter.x / texel) * texel;
		lightCenter.y = std::floor(lightCenter.y / texel) * texel;
		const Vector3 center = Vector3::Transform(lightCenter, lightRotationInverse);
		const Vector3 eye = center - lightDirection * (radius + SHADOW_CASTER_MARGIN);
		const Matrix lightView = XMMatrixLookAtLH(eye, center, up);
		const Matrix lightProj = XMMatrixOrthographicOffCenterLH(-radius, radius, -radius, radius,
			0.f, 2.f * radius + SHADOW_CASTER_MARGIN);

		// Pipeline이 라이트 행렬로 CameraBuffer를 채우고, 직교 투영이므로 컬링은 OBB로 판정한다.
		GAME_INSTANCE->Set_Transform(D3DTS::VIEW, lightView);
		GAME_INSTANCE->Set_Transform(D3DTS::PROJ, lightProj);
		GAME_INSTANCE->Update_Pipeline();
		m_Context->ClearDepthStencilView(m_ShadowDSVs[cascade].Get(), D3D11_CLEAR_DEPTH, 1.f, 0);
		m_Context->OMSetRenderTargets(0, nullptr, m_ShadowDSVs[cascade].Get());
		Render_Group(ETOI(RENDERGROUP::NONBLEND));

		m_ShadowMatrices[cascade] = lightView * lightProj * toTexture;
		(&m_ShadowSplits.x)[cascade] = splits[cascade + 1];
#ifdef _DEBUG
		m_CascadeDraws[cascade] += m_DrawCount - drawsBefore;
		m_CascadeMs[cascade] += std::chrono::duration<Float, std::milli>(std::chrono::steady_clock::now() - timeBefore).count();
#endif
	}
	m_ShadowPass = false;
	m_ShadowReady = true;

	GAME_INSTANCE->Set_Transform(D3DTS::VIEW, cameraView);
	GAME_INSTANCE->Set_Transform(D3DTS::PROJ, cameraProj);
	GAME_INSTANCE->Update_Pipeline();
	m_Context->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, savedRTVs, savedDSV);
	m_Context->RSSetViewports(savedViewportCount, &savedViewport);
	for (ID3D11RenderTargetView* rtv : savedRTVs)
		if (rtv) rtv->Release();
	if (savedDSV) savedDSV->Release();
}

void Renderer::Bake_StaticShadow(const Vector3& lightDirection, const Vector3& up, const Matrix& toTexture)
{
	/* 굽는 범위는 레벨의 정적 충돌(COL) 경계다. 타일 모델 경계에는 수 km 밖의 원경 링·판이 들어 있어 쓰지 않는다.
	   빛 공간 xy는 받는 쪽(플레이어가 다니는 곳)만 덮으면 되고, 그보다 빛 쪽에 있는 캐스터(높은 건물)는
	   깊이 여유(SHADOW_CASTER_MARGIN)와 깊이 클립을 끈 래스터라이저가 담는다. */
	m_BakedReady = false;
	BoundingBox receivers{};
	if (!GAME_INSTANCE->Get_StaticCollisionBounds(receivers))
	{
		LOG_WARN(L"[Shadow] No static collision in this level; static shadows are not baked");
		return;
	}
	const Matrix lightRotation = XMMatrixLookAtLH(Vector3::Zero, lightDirection, up);
	XMFLOAT3 corners[BoundingBox::CORNER_COUNT]{};
	receivers.GetCorners(corners);
	Vector3 lightMin{ FLT_MAX, FLT_MAX, FLT_MAX }, lightMax{ -FLT_MAX, -FLT_MAX, -FLT_MAX };
	for (const XMFLOAT3& corner : corners)
	{
		const Vector3 lightCorner = Vector3::Transform(Vector3{ corner }, lightRotation);
		lightMin = Vector3::Min(lightMin, lightCorner);
		lightMax = Vector3::Max(lightMax, lightCorner);
	}
	const Float width = lightMax.x - lightMin.x;
	const Float height = lightMax.y - lightMin.y;
	const Matrix lightProj = XMMatrixOrthographicOffCenterLH(lightMin.x, lightMax.x, lightMin.y, lightMax.y,
		lightMin.z - SHADOW_CASTER_MARGIN, lightMax.z);
	const Float texel = std::max(width, height) / BAKED_SHADOW_MAP_SIZE;

#ifdef _DEBUG
	const uint32 drawsBefore = m_DrawCount;
	const auto timeBefore = std::chrono::steady_clock::now();
#endif
	// 원본(밴드 0) 모델로 굽는다. 텍셀보다 작은 캐스터는 WorldMap이 이 텍셀 크기로 건너뛴다.
	m_ShadowCascade = 0;
	m_ShadowTexelSize = texel;
	const D3D11_VIEWPORT viewport{ 0.f, 0.f, static_cast<Float>(BAKED_SHADOW_MAP_SIZE), static_cast<Float>(BAKED_SHADOW_MAP_SIZE), 0.f, 1.f };
	m_Context->RSSetViewports(1, &viewport);
	GAME_INSTANCE->Set_Transform(D3DTS::VIEW, lightRotation);
	GAME_INSTANCE->Set_Transform(D3DTS::PROJ, lightProj);
	GAME_INSTANCE->Update_Pipeline();
	m_Context->ClearDepthStencilView(m_BakedDSV.Get(), D3D11_CLEAR_DEPTH, 1.f, 0);
	m_Context->OMSetRenderTargets(0, nullptr, m_BakedDSV.Get());
	Render_Group(ETOI(RENDERGROUP::STATIC));
	m_Context->OMSetRenderTargets(0, nullptr, nullptr);

	m_BakedMatrix = lightRotation * lightProj * toTexture;
	m_BakedTexelWorld = texel;
	m_BakedReady = true;
#ifdef _DEBUG
	++m_ShadowBakes;
	LOG_INFO(L"[Shadow] Baked static shadow {:.0f} x {:.0f} m, texel {:.3f} m, {} draws, {:.1f} ms", width, height, texel,
		m_DrawCount - drawsBefore, std::chrono::duration<Float, std::milli>(std::chrono::steady_clock::now() - timeBefore).count());
#endif
}

size_t Renderer::Hash_StaticGroup() const
{
	// FNV-1a over what decides the cached depth: which objects draw and where they are.
	size_t hash = 14695981039346656037ull;
	const auto mix = [&hash](const void* data, size_t size) {
		const uint8* bytes = static_cast<const uint8*>(data);
		for (size_t i = 0; i < size; ++i)
			hash = (hash ^ bytes[i]) * 1099511628211ull;
	};
	for (const auto& object : m_RenderGroup[ETOI(RENDERGROUP::STATIC)])
	{
		const GameObject* pointer = object.get();
		const Bool drawn = object->Is_Active() && !object->Is_Destroy();
		mix(&pointer, sizeof(pointer));
		mix(&drawn, sizeof(drawn));
		if (const Shared<Transform> transform = object->Get_Transform())
		{
			const Matrix world = transform->Get_WorldMatrix();
			mix(&world, sizeof(world));
		}
	}
	return hash;
}

void Renderer::Render_Deferred() const
{
	// The screen quad and the G-buffer inputs are still bound from Render_Lights of this view;
	// only the light results are new here.
	if (FAILED(GAME_INSTANCE->Bind_RenderTarget_ShaderResource(m_Shader, ShadeMap, RT_SHADE)))
		return;
	if (FAILED(GAME_INSTANCE->Bind_RenderTarget_ShaderResource(m_Shader, SpecularMap, RT_SPECULAR)))
		return;

	m_Shader->Begin(ETOI(DEFERRED::COMBINED));
	m_Buffer->Bind_Resources();
	m_Buffer->Render();
}

void Renderer::Render_Volumetric() const
{
	if (!m_VolumetricReady)
		return;

	if (FAILED(m_PostShader->Bind_Matrix(WorldMatrix, &m_WorldMatrix)))
		return;
	// 시선 위 점을 그림자 맵과 View 깊이로 옮기려면 이 View의 카메라, 활성 방향광, 캐스케이드, 장면 깊이가 필요하다.
	if (FAILED(GAME_INSTANCE->Bind_TransformMatrix(m_PostShader, ViewMatrix, D3DTS::VIEW)))
		return;
	if (FAILED(Bind_ForwardLighting(m_PostShader)))
		return;
	if (FAILED(m_Buffer->Bind_Resources()))
		return;

	// 1) 시선마다 산란량 (픽셀마다 시작점을 흔들어 계단 무늬 대신 노이즈를 낸다)
	if (SUCCEEDED(GAME_INSTANCE->Begin_MultiRenderTarget(MRT_VOLUMETRIC)))
	{
		m_PostShader->Begin(ETOI(POSTPROCESS::VOLUMETRIC));
		m_Buffer->Render();
		GAME_INSTANCE->End_MultiRenderTarget();
	}

	// 2) 노이즈 제거
	if (FAILED(GAME_INSTANCE->Bind_RenderTarget_ShaderResource(m_PostShader, DefaultMap, RT_VOLUMETRIC_RAW)))
		return;
	if (SUCCEEDED(GAME_INSTANCE->Begin_MultiRenderTarget(MRT_VOLUMETRIC_BLUR)))
	{
		m_PostShader->Begin(ETOI(POSTPROCESS::VOLUMETRIC_BLUR));
		m_Buffer->Render();
		GAME_INSTANCE->End_MultiRenderTarget();
	}
}

void Renderer::Start_ScreenGlitch(Float strength, Float duration, Bool bands)
{
	const Float now = GAME_INSTANCE->Get_TotalTime();
	if (duration <= 0.f || strength <= Get_GlitchStrength(now - m_GlitchStart))
		return;
	m_GlitchStart = now;
	m_GlitchStrength = strength;
	m_GlitchDuration = duration;
	m_GlitchBands = bands;
}

// 처음에 세게 튀고 빨리 가라앉게 남은 비율의 제곱으로 줄인다.
Float Renderer::Get_GlitchStrength(Float elapsed) const
{
	if (elapsed < 0.f || elapsed >= m_GlitchDuration)
		return 0.f;
	const Float remain = 1.f - elapsed / m_GlitchDuration;
	return m_GlitchStrength * remain * remain;
}

void Renderer::Render_Grade() const
{
	if (!m_VolumetricReady && m_ColorGrade == Vector3{ 1.f, 1.f, 1.f } && m_ColorTint == Color{ 1.f, 1.f, 1.f, 1.f } &&
		(m_FogDistance <= 0.f || m_FogOpacity <= 0.f) && m_Bloom <= 0.f && !m_GlowReady && m_Glitch <= 0.f)
		return;
	// 보정은 지금 화면을 읽어 같은 화면에 쓰므로 RT_SCENE 사본을 읽는다(DISTORTION과 같은 사본).
	if (FAILED(GAME_INSTANCE->Copy_BoundRenderTarget(RT_SCENE)))
		return;
	if (FAILED(m_PostShader->Bind_Matrix(WorldMatrix, &m_WorldMatrix)))
		return;
	if (FAILED(GAME_INSTANCE->Bind_RenderTarget_ShaderResource(m_PostShader, DefaultMap, RT_SCENE)))
		return;
	// 안개(블룸 원본과 보정)는 이 View의 카메라·장면 깊이·활성 방향광이 필요하다.
	if (FAILED(GAME_INSTANCE->Bind_TransformMatrix(m_PostShader, ViewMatrix, D3DTS::VIEW)))
		return;
	if (FAILED(Bind_ForwardLighting(m_PostShader)))
		return;
	if (FAILED(m_Buffer->Bind_Resources()))
		return;
	// 블룸: 안개를 씌운 화면의 밝은 부분(레벨 블룸 세기를 곱해)과 발광을 가로·세로로 흐려 RT_BLOOM에 둔다
	// (입력 텍스처만 바꿔 묶는다). 화면 보정은 발광 자체와 이 블룸을 더한다.
	if (m_GlowReady && FAILED(GAME_INSTANCE->Bind_RenderTarget_ShaderResource(m_PostShader, EmissiveMap, RT_EMISSIVE)))
		return;
	if (m_Bloom > 0.f || m_GlowReady)
	{
		if (SUCCEEDED(GAME_INSTANCE->Begin_MultiRenderTarget(MRT_BLOOM_H)))
		{
			m_PostShader->Begin(ETOI(POSTPROCESS::BLOOM_H));
			m_Buffer->Render();
			GAME_INSTANCE->End_MultiRenderTarget();
		}
		if (FAILED(GAME_INSTANCE->Bind_RenderTarget_ShaderResource(m_PostShader, DefaultMap, RT_BLOOM_H)))
			return;
		if (SUCCEEDED(GAME_INSTANCE->Begin_MultiRenderTarget(MRT_BLOOM)))
		{
			m_PostShader->Begin(ETOI(POSTPROCESS::BLOOM_V));
			m_Buffer->Render();
			GAME_INSTANCE->End_MultiRenderTarget();
		}
		if (FAILED(GAME_INSTANCE->Bind_RenderTarget_ShaderResource(m_PostShader, DefaultMap, RT_SCENE)))
			return;
	}
	if (FAILED(GAME_INSTANCE->Bind_RenderTarget_ShaderResource(m_PostShader, BloomMap, RT_BLOOM)))
		return;
	// 볼류메트릭 합성: 헤이즈는 장면 깊이로 거리를 재고 방향광 환경광 색으로, 산란은 블러한 결과에 방향광 색을 곱해 더한다.
	if (FAILED(GAME_INSTANCE->Bind_RenderTarget_ShaderResource(m_PostShader, VolumetricMap, RT_VOLUMETRIC)))
		return;
	m_PostShader->Begin(ETOI(POSTPROCESS::GRADE));
	m_Buffer->Render();
}

void Renderer::Render_SSAO() const
{
	// 꺼져 있으면 ScreenCB의 켜짐 값이 0이라 PS_Directional이 AO를 1로 쓴다.
	if (!m_SSAO)
		return;

	if (FAILED(m_Shader->Bind_Matrix(WorldMatrix, &m_WorldMatrix)))
		return;
	if (FAILED(GAME_INSTANCE->Bind_RenderTarget_ShaderResource(m_Shader, NormalMap, RT_NORMAL)))
		return;
	if (FAILED(GAME_INSTANCE->Bind_RenderTarget_ShaderResource(m_Shader, DepthMap, RT_DEPTH)))
		return;
	// 샘플을 화면에 다시 투영하려면 이 View의 카메라 행렬이 필요하다.
	if (FAILED(GAME_INSTANCE->Bind_TransformMatrix(m_Shader, ViewMatrix, D3DTS::VIEW)))
		return;
	if (FAILED(m_Buffer->Bind_Resources()))
		return;

	// 1) 법선·깊이 → 거친 차폐값
	if (SUCCEEDED(GAME_INSTANCE->Begin_MultiRenderTarget(MRT_SSAO)))
	{
		m_Shader->Begin(ETOI(DEFERRED::SSAO));
		m_Buffer->Render();
		GAME_INSTANCE->End_MultiRenderTarget();
	}

	// 2) 깊이 인지 블러로 노이즈 제거
	if (FAILED(GAME_INSTANCE->Bind_RenderTarget_ShaderResource(m_Shader, DefaultMap, RT_AO_RAW)))
		return;
	if (SUCCEEDED(GAME_INSTANCE->Begin_MultiRenderTarget(MRT_SSAO_BLUR)))
	{
		m_Shader->Begin(ETOI(DEFERRED::SSAO_BLUR));
		m_Buffer->Render();
		GAME_INSTANCE->End_MultiRenderTarget();
	}
}

void Renderer::Render_Lights() const
{
	if (FAILED(GAME_INSTANCE->Begin_MultiRenderTarget(MRT_LIGHT)))
		return;

	const auto drawLights = [&]() {
	if (FAILED(m_Shader->Bind_Matrix(WorldMatrix, &m_WorldMatrix)))
		return;

	if (FAILED(GAME_INSTANCE->Bind_RenderTarget_ShaderResource(m_Shader, NormalMap, RT_NORMAL)))
		return;
	if (FAILED(GAME_INSTANCE->Bind_RenderTarget_ShaderResource(m_Shader, DiffuseMap, RT_DIFFUSE)))
		return;
	if (FAILED(GAME_INSTANCE->Bind_RenderTarget_ShaderResource(m_Shader, MaterialMap, RT_MATERIAL)))
		return;
	if (FAILED(GAME_INSTANCE->Bind_RenderTarget_ShaderResource(m_Shader, DepthMap, RT_DEPTH)))
		return;
	if (FAILED(GAME_INSTANCE->Bind_RenderTarget_ShaderResource(m_Shader, AOMap, RT_AO)))
		return;

	// Specular rebuilds world positions from depth with this view's scene camera.
	// The shader has CameraBuffer, so this uploads the whole CameraCB, inverses included.
	if (FAILED(GAME_INSTANCE->Bind_TransformMatrix(m_Shader, ViewMatrix, D3DTS::VIEW)))
		return;

	if (FAILED(Bind_Shadow(m_Shader)))
		return;

	if (FAILED(m_Buffer->Bind_Resources()))
		return;

	if (FAILED(GAME_INSTANCE->Render_Lights(m_Shader, m_Buffer)))
		return;
	};
	drawLights();

	if (FAILED(GAME_INSTANCE->End_MultiRenderTarget()))
		return;
}

HRESULT Renderer::Bind_Shadow(const Shared<Shader>& shader) const
{
	// Render_Shadows of this view; strength 0 (shadows off, no light) leaves the light unshadowed.
	ShadowCB shadowBuffer{};
	for (uint32 cascade = 0; cascade < SHADOW_CASCADE_COUNT; ++cascade)
		shadowBuffer.cascadeMatrices[cascade] = m_ShadowMatrices[cascade];
	shadowBuffer.bakedMatrix = m_BakedMatrix;
	shadowBuffer.splitDepths = m_ShadowSplits;
	shadowBuffer.params = Vector4{ m_ShadowReady ? 1.f : 0.f, SHADOW_DEPTH_BIAS, 1.f / SHADOW_MAP_SIZE, SHADOW_CASCADE_OVERLAP };
	shadowBuffer.bakedParams = Vector4{ m_ShadowReady && m_BakedReady ? 1.f : 0.f, BAKED_SHADOW_DEPTH_BIAS,
		1.f / BAKED_SHADOW_MAP_SIZE, m_BakedTexelWorld };
	shadowBuffer.debug = Vector4{ m_CascadeDebug && m_ShadowReady ? 1.f : 0.f, 0.f, 0.f, 0.f };
	if (FAILED(shader->Bind_CBufferData(shadowBuffer)) || FAILED(shader->Bind_SRV(ShadowMap, m_ShadowSRV)) ||
		FAILED(shader->Bind_SRV(BakedShadowMap, m_BakedSRV)))
		return E_FAIL;
	return S_OK;
}

HRESULT Renderer::Bind_ForwardLighting(const Shared<Shader>& shader) const
{
	if (!shader)
		return E_INVALIDARG;
	// Light::Render가 디퍼드 패스에 넣는 값과 같다. 활성 방향광이 없으면 0(빛 없음)이다.
	LightCB lightBuffer{};
	if (const LIGHT_DESC* light = GAME_INSTANCE->Find_ActiveLight(LIGHT::DIRECTIONAL))
	{
		lightBuffer.direction = light->direction;
		lightBuffer.position = light->position;
		lightBuffer.diffuse = light->diffuse;
		lightBuffer.ambient = light->ambient;
		lightBuffer.specular = light->specular;
		lightBuffer.range = light->range;
	}
	if (FAILED(shader->Bind_CBufferData(lightBuffer)) || FAILED(Bind_Shadow(shader)))
		return E_FAIL;
	return GAME_INSTANCE->Bind_RenderTarget_ShaderResource(shader, DepthMap, RT_DEPTH);
}

void Renderer::Read_OcclusionDepth(OCCLUSION_VIEW& view)
{
	// 꺼 두면 지난 깊이를 버려, 다시 켰을 때 오래된 깊이로 가리지 않게 한다.
	if (!m_Occlusion)
	{
		view.ready = false;
		view.usable = false;
		view.readSerial = view.nextSerial - 1;
		return;
	}
	// 아직 읽지 않은 복사본을 새 것부터 본다. GPU가 덜 끝낸 것은 기다리지 않고 넘어간다.
	array<uint32, OCCLUSION_STAGING_COUNT> order{};
	for (uint32 i = 0; i < OCCLUSION_STAGING_COUNT; ++i)
		order[i] = i;
	std::ranges::sort(order, [&](uint32 lhs, uint32 rhs) { return view.stagingSerial[lhs] > view.stagingSerial[rhs]; });
	for (const uint32 slot : order)
	{
		if (view.stagingSerial[slot] <= view.readSerial || !view.staging[slot])
			break;
		D3D11_MAPPED_SUBRESOURCE mapped{};
		const HRESULT result = m_Context->Map(view.staging[slot].Get(), 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &mapped);
		if (DXGI_ERROR_WAS_STILL_DRAWING == result)
			continue;
		if (FAILED(result))
			break;

		view.levels.resize(1);
		view.levelSizes.assign(1, { view.width, view.height });
		vector<Float>& base = view.levels[0];
		base.resize(static_cast<size_t>(view.width) * view.height);
		for (uint32 y = 0; y < view.height; ++y)
			memcpy(base.data() + static_cast<size_t>(y) * view.width,
				static_cast<const Byte*>(mapped.pData) + static_cast<size_t>(y) * mapped.RowPitch, view.width * sizeof(Float));
		m_Context->Unmap(view.staging[slot].Get(), 0);

		// 2×2 최댓값으로 1×1까지 줄인다. 홀수 끝 텍셀은 마지막 열·행을 한 번 더 본다(더 멀게만 보므로 안전하다).
		while (view.levelSizes.back().first > 1 || view.levelSizes.back().second > 1)
		{
			const auto [width, height] = view.levelSizes.back();
			const uint32 nextWidth = (std::max)(1u, (width + 1) / 2), nextHeight = (std::max)(1u, (height + 1) / 2);
			vector<Float> next(static_cast<size_t>(nextWidth) * nextHeight);
			const vector<Float>& source = view.levels.back();
			for (uint32 y = 0; y < nextHeight; ++y)
				for (uint32 x = 0; x < nextWidth; ++x)
				{
					const uint32 x0 = (std::min)(2 * x, width - 1), x1 = (std::min)(2 * x + 1, width - 1);
					const uint32 y0 = (std::min)(2 * y, height - 1), y1 = (std::min)(2 * y + 1, height - 1);
					next[static_cast<size_t>(y) * nextWidth + x] = (std::max)(
						(std::max)(source[static_cast<size_t>(y0) * width + x0], source[static_cast<size_t>(y0) * width + x1]),
						(std::max)(source[static_cast<size_t>(y1) * width + x0], source[static_cast<size_t>(y1) * width + x1]));
				}
			view.levels.push_back(std::move(next));
			view.levelSizes.emplace_back(nextWidth, nextHeight);
		}
		view.viewProj = view.stagingViewProj[slot];
		view.eye = view.stagingEye[slot];
		view.readSerial = view.stagingSerial[slot];
		view.ready = true;
		break;
	}
	/* 컷처럼 카메라가 한 번에 옮겨지면 그 깊이는 다른 자리에서 본 것이라, 시차 탓에 새 자리에서 드러난 벽·천장까지
	   가렸다고 판정한다(그 프레임에 하늘이 비친다). 새 자리의 깊이를 읽을 때까지 쓰지 않는다.
	   제자리 회전은 그 깊이로 다시 투영해도 맞으므로 위치만 본다. */
	const Vector4 eye = GAME_INSTANCE->Get_CamTransform();
	view.usable = view.ready && Vector3::Distance(Vector3{ eye.x, eye.y, eye.z }, view.eye) <= OCCLUSION_MAX_EYE_SHIFT;
}

void Renderer::Capture_OcclusionDepth(OCCLUSION_VIEW& view, const D3D11_VIEWPORT& viewport)
{
	const uint32 viewWidth = static_cast<uint32>(viewport.Width), viewHeight = static_cast<uint32>(viewport.Height);
	if (0 == viewWidth || 0 == viewHeight)
		return;
	// View 크기가 바뀌면 줄인 깊이와 스테이징을 다시 만들고 지난 깊이를 버린다.
	if (!view.target || view.viewWidth != viewWidth || view.viewHeight != viewHeight)
	{
		view = OCCLUSION_VIEW{};
		view.viewWidth = viewWidth;
		view.viewHeight = viewHeight;
		view.width = (viewWidth + OCCLUSION_DEPTH_BLOCK - 1) / OCCLUSION_DEPTH_BLOCK;
		view.height = (viewHeight + OCCLUSION_DEPTH_BLOCK - 1) / OCCLUSION_DEPTH_BLOCK;
		D3D11_TEXTURE2D_DESC desc{};
		desc.Width = view.width;
		desc.Height = view.height;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_R32_FLOAT;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_RENDER_TARGET;
		if (FAILED(m_Device->CreateTexture2D(&desc, nullptr, view.target.GetAddressOf())) ||
			FAILED(m_Device->CreateRenderTargetView(view.target.Get(), nullptr, view.targetRTV.GetAddressOf())))
		{
			LOG_ERROR(L"Renderer: occlusion depth target {}x{} could not be created", view.width, view.height);
			view.target = nullptr;
			return;
		}
		desc.Usage = D3D11_USAGE_STAGING;
		desc.BindFlags = 0;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		for (auto& staging : view.staging)
			if (FAILED(m_Device->CreateTexture2D(&desc, nullptr, staging.GetAddressOf())))
			{
				LOG_ERROR(L"Renderer: occlusion staging {}x{} could not be created", view.width, view.height);
				view.target = nullptr;
				return;
			}
	}

	ComPtr<ID3D11RenderTargetView> boundTarget;
	ComPtr<ID3D11DepthStencilView> boundDepth;
	m_Context->OMGetRenderTargets(1, boundTarget.GetAddressOf(), boundDepth.GetAddressOf());
	const D3D11_VIEWPORT reduced{ 0.f, 0.f, static_cast<Float>(view.width), static_cast<Float>(view.height), 0.f, 1.f };
	m_Context->OMSetRenderTargets(1, view.targetRTV.GetAddressOf(), nullptr);
	m_Context->RSSetViewports(1, &reduced);
	// 화면 사각형은 View 크기 행렬로 NDC 전체를 덮으므로 작은 뷰포트에도 그대로 맞는다.
	if (SUCCEEDED(m_Shader->Bind_Matrix(WorldMatrix, &m_WorldMatrix)) &&
		SUCCEEDED(GAME_INSTANCE->Bind_RenderTarget_ShaderResource(m_Shader, DepthMap, RT_DEPTH)) &&
		SUCCEEDED(m_Buffer->Bind_Resources()))
	{
		m_Shader->Begin(ETOI(DEFERRED::DEPTH_REDUCE));
		m_Buffer->Render();
		const uint32 slot = static_cast<uint32>(view.nextSerial % OCCLUSION_STAGING_COUNT);
		m_Context->CopyResource(view.staging[slot].Get(), view.target.Get());
		view.stagingViewProj[slot] = GAME_INSTANCE->Get_Transform(D3DTS::VIEW) * GAME_INSTANCE->Get_Transform(D3DTS::PROJ);
		const Vector4 eye = GAME_INSTANCE->Get_CamTransform();
		view.stagingEye[slot] = Vector3{ eye.x, eye.y, eye.z };
		view.stagingSerial[slot] = view.nextSerial++;
	}
	m_Context->OMSetRenderTargets(1, boundTarget.GetAddressOf(), boundDepth.Get());
	m_Context->RSSetViewports(1, &viewport);
}

Bool Renderer::Is_Occluded(const BoundingBox& worldBounds) const
{
	if (!m_Occlusion || m_ShadowPass || RENDERGROUP::END == m_DrawingGroup || !m_OcclusionView || !m_OcclusionView->usable)
		return false;
	const OCCLUSION_VIEW& view = *m_OcclusionView;

	// 그 깊이를 그린 카메라로 상자를 투영한다. 카메라 뒤로 넘어가거나 그때 화면 밖이면 판정하지 않는다.
	Vector3 corners[BoundingBox::CORNER_COUNT];
	worldBounds.GetCorners(corners);
	Float minX = FLT_MAX, minY = FLT_MAX, minZ = FLT_MAX, maxX = -FLT_MAX, maxY = -FLT_MAX;
	for (const Vector3& corner : corners)
	{
		const Vector4 clip = Vector4::Transform(Vector4{ corner.x, corner.y, corner.z, 1.f }, view.viewProj);
		if (clip.w <= 1e-4f)
			return false;
		const Float x = clip.x / clip.w, y = clip.y / clip.w, z = clip.z / clip.w;
		minX = (std::min)(minX, x); maxX = (std::max)(maxX, x);
		minY = (std::min)(minY, y); maxY = (std::max)(maxY, y);
		minZ = (std::min)(minZ, z);
	}
	if (minZ <= 0.f || maxX < -1.f || minX > 1.f || maxY < -1.f || minY > 1.f)
		return false;

	// NDC → 줄인 깊이(레벨 0) 텍셀. 사각형이 한 변 4텍셀 안에 들 때까지 위 레벨로 올라간다.
	const Float scaleX = view.viewWidth * 0.5f / OCCLUSION_DEPTH_BLOCK, scaleY = view.viewHeight * 0.5f / OCCLUSION_DEPTH_BLOCK;
	Float x0 = ((std::max)(minX, -1.f) + 1.f) * scaleX, x1 = ((std::min)(maxX, 1.f) + 1.f) * scaleX;
	Float y0 = (1.f - (std::min)(maxY, 1.f)) * scaleY, y1 = (1.f - (std::max)(minY, -1.f)) * scaleY;
	size_t level = 0;
	while (level + 1 < view.levels.size() && (x1 - x0 > 4.f || y1 - y0 > 4.f))
	{
		++level;
		x0 *= 0.5f; x1 *= 0.5f; y0 *= 0.5f; y1 *= 0.5f;
	}
	const auto [width, height] = view.levelSizes[level];
	const vector<Float>& depths = view.levels[level];
	const uint32 left = (std::min)(static_cast<uint32>(x0), width - 1), right = (std::min)(static_cast<uint32>(x1), width - 1);
	const uint32 top = (std::min)(static_cast<uint32>(y0), height - 1), bottom = (std::min)(static_cast<uint32>(y1), height - 1);
	Float farthest = 0.f;
	for (uint32 y = top; y <= bottom; ++y)
		for (uint32 x = left; x <= right; ++x)
			farthest = (std::max)(farthest, depths[static_cast<size_t>(y) * width + x]);

	const Bool occluded = minZ > farthest;
	if (occluded)
		++m_OccludedCount;
	return occluded;
}

Matrix Renderer::Get_OcclusionViewProj() const
{
	return m_Occlusion && m_OcclusionView && m_OcclusionView->usable ? m_OcclusionView->viewProj : Matrix{};
}

void Renderer::Render_Group(uint32 groupIndex)
{
	m_DrawingGroup = static_cast<RENDERGROUP>(groupIndex);
	// 렌더 타깃은 부르는 쪽이 묶는다: 화면은 G-buffer MRT, 그림자 패스는 캐스케이드 DSV.
	for (auto& object : m_RenderGroup[groupIndex])
	{
		uint32 objLayer = object->Get_LayerMask().Get_Layer();
		if (objLayer != 0 && !(m_LayerMask & objLayer))
			continue;
		if (!object->Is_Active() || object->Is_Destroy())
			continue;

		Render_Recursive(object);
	}
	m_DrawingGroup = RENDERGROUP::END;
}
void Renderer::Render_Recursive(const Shared<GameObject>& object) const
{
	/* 움직이는 메시(모델을 단 PartObject: 플레이어·적·무기)는 지금 포즈의 경계로 판정해 보이지 않으면 그리지 않는다.
	   그림자 패스는 이번 캐스케이드 박스(해 쪽 SHADOW_CASTER_MARGIN 포함), 화면 패스는 카메라 프러스텀과 가림 컬링이다.
	   그룹(NONBLEND·발광·균열)마다 같은 판정을 하고, 자식은 따로 판정한다. */
	Bool culled = false;
	if (m_FrustumCulling && GAMEOBJECTTYPE::PART == object->Get_GameObjectType())
	{
		BoundingBox bounds{};
		if (const Shared<Model> model = object->Get_Component<Model>(); model && model->Compute_PosedBounds(bounds))
		{
			bounds.Transform(bounds, static_pointer_cast<PartObject>(object)->Get_CombinedWorldMatrix());
			culled = !GAME_INSTANCE->Is_Visible(bounds);
		}
	}
	if (!culled)
		object->Render();

	for (auto& child : object->Get_Children())
	{
		if (!child->Is_Active() || child->Is_Destroy() || m_SubmittedChildren.contains(child.get()))
			continue;
		Render_Recursive(child);
	}
}

Unique<Renderer> Renderer::Create(const ComPtr<ID3D11Device> &device, const ComPtr<ID3D11DeviceContext> &context) {
	auto renderer = make_unique<Renderer>(device, context);

	if (FAILED(renderer->Initialize(nullptr))) {
		MSG_BOX("Failed To Create Renderer");
		return nullptr;
	}

	return renderer;
}


#ifdef _DEBUG
void Renderer::Render_Debug()
{
	if (!m_ShowRenderTargets)
		return;

	for (const RenderTargetDebugTile& tile : RenderTargetDebugTiles)
		GAME_INSTANCE->Ready_RenderTarget_Debug(tile.tag, tile.x, tile.y, tile.size, tile.size);
	if (FAILED(GAME_INSTANCE->Render_RenderTarget_Debug(m_Buffer, m_Shader, MRT_GameObject)))
		return;

	if (FAILED(GAME_INSTANCE->Render_RenderTarget_Debug(m_Buffer, m_Shader, MRT_LIGHT)))
		return;
	if (FAILED(GAME_INSTANCE->Render_RenderTarget_Debug(m_Buffer, m_Shader, MRT_SSAO)))
		return;
	if (FAILED(GAME_INSTANCE->Render_RenderTarget_Debug(m_Buffer, m_Shader, MRT_SSAO_BLUR)))
		return;
	if (FAILED(GAME_INSTANCE->Render_RenderTarget_Debug(m_Buffer, m_Shader, MRT_VOLUMETRIC)))
		return;
	if (FAILED(GAME_INSTANCE->Render_RenderTarget_Debug(m_Buffer, m_Shader, MRT_VOLUMETRIC_BLUR)))
		return;
	if (FAILED(GAME_INSTANCE->Render_RenderTarget_Debug(m_Buffer, m_Shader, MRT_EMISSIVE)))
		return;
	if (FAILED(GAME_INSTANCE->Render_RenderTarget_Debug(m_Buffer, m_Shader, MRT_BLOOM_H)))
		return;
	if (FAILED(GAME_INSTANCE->Render_RenderTarget_Debug(m_Buffer, m_Shader, MRT_BLOOM)))
		return;

	// 정적 그림자 맵은 RenderTargetManager 밖의 깊이 텍스처라 여기서 따로 그린다.
	if (!m_BakedReady)
		return;
	UINT count = 1;
	D3D11_VIEWPORT viewport{};
	m_Context->RSGetViewports(&count, &viewport);
	const RenderTargetDebugTile& tile = ShadowMapDebugTile;
	Matrix tileMatrix = Matrix::CreateScale(tile.size, tile.size, 1.f);
	tileMatrix._41 = tile.x - viewport.Width * 0.5f;
	tileMatrix._42 = -tile.y + viewport.Height * 0.5f;
	if (FAILED(m_Shader->Bind_Matrix(WorldMatrix, &tileMatrix)) || FAILED(m_Shader->Bind_SRV(BakedShadowMap, m_BakedSRV)))
		return;
	m_Shader->Begin(ETOI(DEFERRED::SHADOW_DEBUG));
	m_Buffer->Bind_Resources();
	m_Buffer->Render();
}
#endif
