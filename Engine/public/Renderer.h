#pragma once
#include "EngineManager.h"

NS_BEGIN(Engine)

class GameObject;
class Shader;
class VIBuffer_Rect;

class Renderer final : public EngineManager {
	NO_COPY(Renderer)
public:
	explicit Renderer(const ComPtr<ID3D11Device> &device, const ComPtr<ID3D11DeviceContext> &context);
	~Renderer() override = default;

public:
  uint32 Get_LayerBits() const { return m_LayerMask; }

public:
	// F4 프러스텀 컬링·F5 월드 타일 LOD 스위치. 기본은 켜짐이고 전후 FPS 비교용으로 끈다.
	// 최적화 스위치는 F키, 렌더 타깃·셰이딩 스위치(그림자·SSAO·볼류메트릭·안개·블룸·보정)는 Ctrl+F키다.
	Bool Toggle_FrustumCulling() { m_FrustumCulling = !m_FrustumCulling; return m_FrustumCulling; }
	Bool Get_FrustumCulling() const { return m_FrustumCulling; }
	Bool Toggle_WorldLod() { m_WorldLod = !m_WorldLod; return m_WorldLod; }
	Bool Get_WorldLod() const { return m_WorldLod; }
	// Ctrl+F2 방향광 캐스케이드 그림자. 기본은 켜짐이고, 끄면 그림자 패스를 건너뛴다. Ctrl+F3 SSAO.
	Bool Toggle_Shadow() { m_Shadow = !m_Shadow; return m_Shadow; }
	Bool Get_Shadow() const { return m_Shadow; }
	Bool Toggle_SSAO() { m_SSAO = !m_SSAO; return m_SSAO; }
	Bool Get_SSAO() const { return m_SSAO; }
	// SSAO 반경·강도·바이어스(x, y, z). 기본값은 SSAO_* 상수이고, Editor Debug 메뉴가 실행 중에 바꾼다.
	const Vector3& Get_SSAOParams() const { return m_SSAOParams; }
	void Set_SSAOParams(const Vector3& params) { m_SSAOParams = params; }
	/* 화면 보정(노출, 대비, 채도)과 색조. 레벨의 DirectionalLight가 매 프레임 넣고, 프레임이 끝나면(Clear_RenderGroup)
	   중립으로 돌아가 조명이 없는 레벨(타이틀·로딩)에는 남지 않는다. 중립이면 패스를 건너뛴다. */
	void Set_ColorGrade(const Vector3& grade, const Color& tint) { m_ColorGrade = grade; m_ColorTint = tint; }
	// 레벨 안개(색, 거리 m, 최대 불투명도, 해 쪽에 더하는 색). 보정과 같이 DirectionalLight가 매 프레임 넣고 프레임 끝에 꺼진다.
	// 블룸 세기(밝은 부분이 번지는 빛). 0이면 끈다. 보정과 같이 매 프레임 넣고 프레임 끝에 꺼진다.
	void Set_Bloom(Float intensity) { m_Bloom = intensity; }
	/* Ctrl+F5 안개, Ctrl+F6 블룸, Ctrl+F7 보정(노출·대비·채도·틴트) 켬/끔 비교. 끄면 레벨이 넣은 값을 그 View에서 중립으로 본다.
	   블룸은 레벨 블룸만 끄고 발광(RT_EMISSIVE)의 번짐은 그대로 둔다. */
	Bool Toggle_Fog() { m_FogOn = !m_FogOn; return m_FogOn; }
	Bool Get_Fog() const { return m_FogOn; }
	Bool Toggle_Bloom() { m_BloomOn = !m_BloomOn; return m_BloomOn; }
	Bool Get_Bloom() const { return m_BloomOn; }
	Bool Toggle_ColorGrade() { m_ColorGradeOn = !m_ColorGradeOn; return m_ColorGradeOn; }
	Bool Get_ColorGrade() const { return m_ColorGradeOn; }
	/* 화면 글리치: 화면 보정이 빨강·파랑을 가로 반대쪽으로 갈라 읽고(색수차), bands면 가로 띠 몇 줄도 밀어 놓는다
	   (블룸·발광도 같이). strength(1이 최대)에서 duration초 동안 0으로 줄어든다. 남은 세기가 더 센 글리치는 덮지 않는다. */
	void Start_ScreenGlitch(Float strength, Float duration, Bool bands = true);
	void Set_Fog(const Color& color, Float distance, Float opacity, const Color& sunColor)
	{
		m_FogColor = color; m_FogDistance = distance; m_FogOpacity = opacity; m_FogSunColor = sunColor;
	}
	// Ctrl+F4 볼류메트릭 라이트. 세기·밀도·최대 거리(x, y, z) 기본값은 VOLUMETRIC_* 상수이고, Editor Debug 메뉴가 바꾼다.
	Bool Toggle_Volumetric() { m_Volumetric = !m_Volumetric; return m_Volumetric; }
	Bool Get_Volumetric() const { return m_Volumetric; }
	const Vector3& Get_VolumetricParams() const { return m_VolumetricParams; }
	void Set_VolumetricParams(const Vector3& params) { m_VolumetricParams = params; }
	// Ctrl+F8 "Cascade Colors": 조명 결과를 캐스케이드마다 다른 색으로 물들인다. Release에서도 동작한다.
	Bool Toggle_CascadeDebug() { m_CascadeDebug = !m_CascadeDebug; return m_CascadeDebug; }
	Bool Get_CascadeDebug() const { return m_CascadeDebug; }
	/* F7 인스턴싱. 기본은 켜짐이고 Release에서도 동작한다. 끄면 InstanceMesh가 인스턴스마다 world를 올리고
	   draw를 한 번씩 불러, 인스턴싱이 없는 렌더러와 같은 화면·비용으로 FPS를 비교할 수 있다. */
	Bool Toggle_Instancing() { m_Instancing = !m_Instancing; return m_Instancing; }
	Bool Get_Instancing() const { return m_Instancing; }
	/* F6 가림 컬링. 기본은 켜짐이고 Release에서도 동작한다. 화면 패스(렌더 그룹 안, 그림자 패스 밖)에서 이 View가
	   1~2프레임 전에 그린 깊이에 완전히 가린 월드 상자를 true로 본다. 깊이가 아직 없거나 판정할 수 없으면 false다. */
	Bool Toggle_Occlusion() { m_Occlusion = !m_Occlusion; return m_Occlusion; }
	Bool Get_Occlusion() const { return m_Occlusion; }
	Bool Is_Occluded(const BoundingBox& worldBounds) const;
	// 지금 가림 판정에 쓰는 깊이를 그린 VIEW×PROJ(없으면 단위행렬). 판정 결과를 캐시하는 쪽이 키로 쓴다.
	Matrix Get_OcclusionViewProj() const;
	// 캐스케이드 그림자 맵을 그리는 동안 true. Model은 이때 ShadowCompatible 패스로 그리고, 오브젝트는 디버그 그리기를 건너뛴다.
	Bool Is_ShadowPass() const { return m_ShadowPass; }
	/* 지금 그리는 렌더 그룹(그룹 밖이면 END). 불투명·데칼·반투명 메시를 함께 가진 오브젝트(월드 타일)가
	   그룹마다 불려 그 그룹의 메시만 그린다. 그림자 패스는 STATIC·NONBLEND 그룹을 돈다. */
	RENDERGROUP Get_DrawingGroup() const { return m_DrawingGroup; }
	/* BLEND 그룹의 포워드 조명: G-buffer를 거치지 않는 오브젝트의 셰이더에 활성 방향광, 이번 View의 캐스케이드
	   그림자, 불투명 장면 깊이(RT_DEPTH)를 넣는다. PS_Directional과 같은 값이다. */
	HRESULT Bind_ForwardLighting(const Shared<Shader>& shader) const;
	// 그림자 패스 중에만 뜻이 있다: 지금 그리는 캐스케이드(정적 굽기는 0)와 그 맵 텍셀 하나의 월드 크기.
	uint32 Get_ShadowCascade() const { return m_ShadowCascade; }
	Float Get_ShadowTexelSize() const { return m_ShadowTexelSize; }

public:
	void Add_RenderGroup(RENDERGROUP renderGroup, const Shared<GameObject> &gameObject);
	void Draw();
	// screenIndex는 그리는 View(오프스크린 화면 번호). View마다 가림 깊이를 따로 둔다.
	void Draw_NoClearing(uint32 screenIndex = 0);
	HRESULT Clear_RenderGroup();

public:
	HRESULT Initialize(void *arg) override;
	void On_Destroy() override;
	void On_Disable() override;
	void On_Enable() override;
	void Set_Active(Bool isActive) override;

private:
	void Render_Deferred() const;
	void Render_SSAO() const;
	void Render_Lights() const;
	void Render_Grade() const;
	// 방향광 산란을 RT_VOLUMETRIC에 그린다. 그림자(ShadowCB)와 이 View의 카메라가 필요하다.
	void Render_Volumetric() const;
	void Render_Group(uint32 groupIndex);
	// 이번 View의 캐스케이드 그림자(ShadowBuffer·그림자 맵)를 셰이더에 넣는다. 디퍼드 조명과 포워드 조명이 같이 쓴다.
	HRESULT Bind_Shadow(const Shared<Shader>& shader) const;
	void Render_Recursive(const Shared<GameObject>& object) const;

private:
	ComPtr<ID3D11Device> m_Device = {nullptr};
	ComPtr<ID3D11DeviceContext> m_Context = {nullptr};
	array<vector<Shared<GameObject>>, ETOI(RENDERGROUP::END)> m_RenderGroup;
	// Children submitted this frame: drawn in their own groups, not along with their parent.
	unordered_set<const GameObject*> m_SubmittedChildren;
	uint32 m_LayerMask = {ETOI(LAYER::ALL_LAYER)};
	Bool m_FrustumCulling{ true };
	Bool m_WorldLod{ true };
	Bool m_Shadow{ true };
	Bool m_Instancing{ true };
	Bool m_Occlusion{ true };
	Bool m_SSAO{ true };
	Vector3 m_SSAOParams{ SSAO_RADIUS, SSAO_INTENSITY, SSAO_BIAS };
	Vector3 m_ColorGrade{ 1.f, 1.f, 1.f };
	Color m_ColorTint{ 1.f, 1.f, 1.f, 1.f };
	Color m_FogColor{};
	Float m_FogDistance{ 0.f };
	Float m_FogOpacity{ 0.f };
	Color m_FogSunColor{};
	Float m_Bloom{ 0.f };
	Bool m_FogOn{ true };
	Bool m_BloomOn{ true };
	Bool m_ColorGradeOn{ true };
	Bool m_Volumetric{ true };
	Vector3 m_VolumetricParams{ VOLUMETRIC_INTENSITY, VOLUMETRIC_DENSITY, VOLUMETRIC_DISTANCE };
	// 이번 View에 볼류메트릭을 그렸는가(켜짐 + 활성 방향광). Render_Grade가 합성 여부를 정한다.
	mutable Bool m_VolumetricReady{ false };
	// 이번 View에 발광(RENDERGROUP::EMISSIVE)을 RT_EMISSIVE에 그렸는가. Render_Grade가 더하고 블룸에 넣는다.
	Bool m_GlowReady{ false };
	// 글리치를 시작한 시각(Get_TotalTime)·세기·길이·띠 밀림과 이번 View의 세기. 0이면 Render_Grade가 글리치 없이 그린다.
	Float m_GlitchStart{};
	Float m_GlitchStrength{};
	Float m_GlitchDuration{};
	Bool m_GlitchBands{ true };
	Float m_Glitch{};
	Float Get_GlitchStrength(Float elapsed) const;
	Bool m_CascadeDebug{ false };
	Bool m_ShadowPass{ false };
	RENDERGROUP m_DrawingGroup{ RENDERGROUP::END };
	uint32 m_ShadowCascade{};
	Float m_ShadowTexelSize{};

private: /* Shadows: a static map baked once per level plus dynamic cascades redrawn every view. */
	/* View마다 PRIORITY 앞에서 그린다. 정적 그림자가 없거나 방향광·정적 그룹이 바뀌었으면 먼저 굽고, 움직이는
	   캐스터(NONBLEND)만 카메라 가까이의 캐스케이드에 ShadowCompatible로 그린다. 끝나면 카메라 행렬·렌더 타깃·뷰포트를 되돌린다. */
	void Render_Shadows();
	// STATIC 그룹을 레벨 정적 충돌 경계를 덮는 빛 방향 직교 투영 한 장(m_BakedTexture)에 굽는다.
	void Bake_StaticShadow(const Vector3& lightDirection, const Vector3& up, const Matrix& toTexture);
	// STATIC 그룹의 목록·활성 상태·월드 행렬 요약. 달라지면 정적 그림자를 다시 굽는다.
	size_t Hash_StaticGroup() const;

	// 동적 캐스케이드: R32_TYPELESS 배열, 슬라이스마다 D32 DSV, R32F 배열 SRV 하나.
	ComPtr<ID3D11Texture2D> m_ShadowTexture{ nullptr };
	array<ComPtr<ID3D11DepthStencilView>, SHADOW_CASCADE_COUNT> m_ShadowDSVs{};
	ComPtr<ID3D11ShaderResourceView> m_ShadowSRV{ nullptr };
	D3D11_VIEWPORT m_ShadowViewport{};
	// 이번 View에서 그린 캐스케이드: 월드 → 그림자 맵 UV·깊이 행렬과 각 캐스케이드가 끝나는 View 깊이. Render_Lights가 쓴다.
	array<Matrix, SHADOW_CASCADE_COUNT> m_ShadowMatrices{};
	Vector4 m_ShadowSplits{};
	Bool m_ShadowReady{ false };
	/* 먼 캐스케이드는 2프레임에 한 번 그린다. 마지막으로 그린 View·빛 방향과, 이번에 지난 깊이를 이어 썼는지
	   (이어 쓴 다음 프레임은 반드시 그린다). 그림자를 끄거나 정적 그림자를 다시 구우면 View를 비워 바로 그린다. */
	const void* m_FarCascadeView{ nullptr };
	Vector3 m_FarCascadeLight{};
	Bool m_FarCascadeReused{ false };

	// 정적 그림자: R16_TYPELESS, D16 DSV, R16 SRV. View와 상관없이 레벨에 한 장이다.
	ComPtr<ID3D11Texture2D> m_BakedTexture{ nullptr };
	ComPtr<ID3D11DepthStencilView> m_BakedDSV{ nullptr };
	ComPtr<ID3D11ShaderResourceView> m_BakedSRV{ nullptr };
	Matrix m_BakedMatrix{};			// 월드 → 정적 그림자 맵 UV(xy)·깊이(z)
	Float m_BakedTexelWorld{};		// 텍셀 하나의 월드 크기(긴 변 기준)
	Vector3 m_BakedLightDirection{};
	size_t m_BakedSignature{};
	Bool m_BakedReady{ false };

private: /* Occlusion: 화면 패스 깊이를 OCCLUSION_DEPTH_BLOCK 칸마다 가장 먼 값으로 줄여 CPU에서 읽는다. */
	struct OCCLUSION_VIEW
	{
		uint32 width{}, height{};							// 줄인 깊이 크기(View 크기 / 칸, 올림)
		uint32 viewWidth{}, viewHeight{};
		ComPtr<ID3D11Texture2D> target{ nullptr };
		ComPtr<ID3D11RenderTargetView> targetRTV{ nullptr };
		array<ComPtr<ID3D11Texture2D>, OCCLUSION_STAGING_COUNT> staging{};
		array<Matrix, OCCLUSION_STAGING_COUNT> stagingViewProj{};
		array<uint64_t, OCCLUSION_STAGING_COUNT> stagingSerial{};	// 0이면 비었다
		array<Vector3, OCCLUSION_STAGING_COUNT> stagingEye{};
		uint64_t nextSerial{ 1 }, readSerial{};
		// 읽어 온 깊이의 최댓값 mip 체인(0 = 줄인 깊이)과 그것을 그린 VIEW×PROJ·카메라 위치.
		vector<vector<Float>> levels;
		vector<std::pair<uint32, uint32>> levelSizes;
		Matrix viewProj{};
		Vector3 eye{};
		Bool ready{ false };
		Bool usable{ false };	// ready이고 지금 카메라가 그 깊이를 그린 자리 가까이 있다
	};
	// View를 그리기 전에 GPU가 끝낸 가장 새 깊이를 읽어 mip을 만든다.
	void Read_OcclusionDepth(OCCLUSION_VIEW& view);
	// G-buffer 직후 RT_DEPTH를 줄여 스테이징 한 장에 복사한다.
	void Capture_OcclusionDepth(OCCLUSION_VIEW& view, const D3D11_VIEWPORT& viewport);
	map<uint32, OCCLUSION_VIEW> m_OcclusionViews;
	OCCLUSION_VIEW* m_OcclusionView{ nullptr };		// 지금 그리는 View의 것

private: /* For DefShader */
	Matrix m_WorldMatrix{};
	Shared<Shader> m_Shader{ nullptr };
	// 화면 후처리(볼류메트릭 라이트, 화면 보정): POSTPROCESS 패스, PostProcessCB.
	Shared<Shader> m_PostShader{ nullptr };
	Shared<VIBuffer_Rect> m_Buffer{ nullptr };

public:
	static Unique<Renderer> Create(const ComPtr<ID3D11Device> &device, const ComPtr<ID3D11DeviceContext> &context);


public:
	// 프레임 draw 통계: VIBuffer가 draw마다 더하고, Clear_RenderGroup에서 직전 프레임 값으로 넘긴다.
	// Release Launcher의 F1 표시도 읽으므로 Debug 전용이 아니다.
	void Add_DrawStats(uint32 triangleCount) { ++m_DrawCount; m_TriangleCount += triangleCount; }
	uint32 Get_FrameDrawCount() const { return m_LastDrawCount; }
	uint64_t Get_FrameTriangleCount() const { return m_LastTriangleCount; }
	// 맵 소품(InstanceMesh)이 한 draw로 묶어 그린 인스턴스 수. F11을 끄면 0이 되고 그만큼 draw가 는다(파티클 인스턴스는 세지 않는다).
	void Add_InstancedProps(uint32 instanceCount) { m_InstanceCount += instanceCount; }
	uint32 Get_FrameInstanceCount() const { return m_LastInstanceCount; }
	// 컬링으로 건너뛴 메시 수와 LOD 메시로 그린 타일 수. draw 통계와 같은 주기로 넘어간다.
	void Add_CulledMesh() { ++m_CulledCount; }
	uint32 Get_FrameCulledCount() const { return m_LastCulledCount; }
	// 가림 컬링이 가렸다고 본 상자 수(타일·메시·소품 묶음·소품 인스턴스 판정을 모두 센다).
	uint32 Get_FrameOccludedCount() const { return m_LastOccludedCount; }
	void Add_LodTile() { ++m_LodTileCount; }
	uint32 Get_FrameLodTileCount() const { return m_LastLodTileCount; }

private:
	uint32 m_DrawCount{}, m_LastDrawCount{};
	uint64_t m_TriangleCount{}, m_LastTriangleCount{};
	uint32 m_InstanceCount{}, m_LastInstanceCount{};
	uint32 m_CulledCount{}, m_LastCulledCount{};
	mutable uint32 m_OccludedCount{};
	uint32 m_LastOccludedCount{};
	uint32 m_LodTileCount{}, m_LastLodTileCount{};

#ifdef _DEBUG
public:
	Bool Toggle_RenderTargetDebug() { m_ShowRenderTargets = !m_ShowRenderTargets; return m_ShowRenderTargets; }
	Bool Get_RenderTargetDebug() const { return m_ShowRenderTargets; }

	// 동적 캐스케이드별 draw 수와 CPU 시간(ms), 정적 그림자를 구운 횟수. 모든 View 합이고 draw 통계와 같은 주기로 넘어간다.
	uint32 Get_FrameCascadeDraws(uint32 cascade) const { return cascade < SHADOW_CASCADE_COUNT ? m_LastCascadeDraws[cascade] : 0; }
	Float Get_FrameCascadeMs(uint32 cascade) const { return cascade < SHADOW_CASCADE_COUNT ? m_LastCascadeMs[cascade] : 0.f; }
	uint32 Get_FrameShadowBakes() const { return m_LastShadowBakes; }

private:
	void Render_Debug();

	// Ctrl+F1 MRT 미리보기는 필요할 때만 켠다.
	Bool m_ShowRenderTargets{ false };

	array<uint32, SHADOW_CASCADE_COUNT> m_CascadeDraws{}, m_LastCascadeDraws{};
	array<Float, SHADOW_CASCADE_COUNT> m_CascadeMs{}, m_LastCascadeMs{};
	uint32 m_ShadowBakes{}, m_LastShadowBakes{};
#endif
};

NS_END