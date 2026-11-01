#pragma once
#include "Engine_Define.h"

#ifdef _DEBUG
NS_BEGIN(Engine)

/* Debug 빌드 전용 디버그 그리기. 선 배치와 반투명 면에 쓰는 이펙트·입력 레이아웃·상태를 하나만 두고,
   무엇을 그릴지는 데이터 주인(Collider, NavCell, PhysicsManager, Editor 도구)이 정한다. Game이 소유한다. */
class ENGINE_DLL DebugRenderer final
{
	NO_COPY(DebugRenderer)
public:
	explicit DebugRenderer() = default;
	~DebugRenderer() = default;

public:
	/// 선: 돌려준 배치에 그리고 End_Lines로 내보낸다. 깊이·블렌드·래스터 상태는 지금 묶인 것을 그대로 쓴다.
	const Shared<PrimitiveBatch<VertexPositionColor>>& Begin_Lines(const Matrix& world, const Matrix& view, const Matrix& proj);
	void End_Lines();

	/// 반투명 면: 장면 깊이로 가려지되 깊이는 쓰지 않고, 같은 자리 메시보다 카메라 쪽으로 당겨 양면을 칠한다.
	/// 정점(VertexPositionColor, 월드 좌표)은 부르는 쪽이 가진다. End_Faces가 Begin_Faces 전 상태를 되돌린다.
	void Begin_Faces(const Matrix& view, const Matrix& proj);
	void Draw_Faces(ID3D11Buffer* vertexBuffer, uint32 startVertex, uint32 vertexCount);
	void End_Faces();

private:
	HRESULT Initialize(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	void Apply_Effect(const Matrix& world, const Matrix& view, const Matrix& proj);

private:
	ComPtr<ID3D11DeviceContext> m_Context{ nullptr };
	Shared<BasicEffect> m_Effect{ nullptr };
	ComPtr<ID3D11InputLayout> m_InputLayout{ nullptr };
	Shared<PrimitiveBatch<VertexPositionColor>> m_Batch{ nullptr };

	ComPtr<ID3D11BlendState> m_FaceBlendState{ nullptr };
	ComPtr<ID3D11DepthStencilState> m_FaceDepthState{ nullptr };
	ComPtr<ID3D11RasterizerState> m_FaceRasterizerState{ nullptr };

	// Begin_Faces 전에 묶여 있던 상태. End_Faces가 되돌린다.
	ComPtr<ID3D11BlendState> m_SavedBlendState{ nullptr };
	Float m_SavedBlendFactor[4]{};
	UINT m_SavedSampleMask{};
	ComPtr<ID3D11DepthStencilState> m_SavedDepthState{ nullptr };
	UINT m_SavedStencilRef{};
	ComPtr<ID3D11RasterizerState> m_SavedRasterizerState{ nullptr };

public:
	static Unique<DebugRenderer> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
};

NS_END
#endif
