#pragma once
#include "Component.h"

NS_BEGIN(Engine)

/// 월드 지오메트리를 PhysX 정적 액터로 올리고, 자기 수명 동안만 유지한다.
/// 게임플레이 판정용 Collider(AABB/OBB/Sphere)와는 역할이 다르다.
class ENGINE_DLL WorldCollider final : public Component
{
public:
	typedef struct tagWorldColliderDesc : public COMPONENT_DESC
	{
		wstring sourceModelTag{};   // 충돌로 쓸 모델 태그 (예: CityOfRuinEntry_COL)
	} WORLD_COLLIDER_DESC;

public:
	explicit WorldCollider();
	explicit WorldCollider(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit WorldCollider(const WorldCollider& rhs);
	~WorldCollider() override = default;

public:
	COMPONENT_TYPE Get_ComponentType() const override { return COMPONENT_TYPE::WORLD_COLLIDER; }
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;
	void    On_Destroy() override;

	const wstring& Get_SourceModelTag() const { return m_SourceModelTag; }

private:
	/// 소유자 Transform이 확정된 뒤에 액터를 만든다. Initialize 시점에는 아직 기본 위치다.
	HRESULT Post_Load() override;

private:
	wstring m_SourceModelTag{};
	uint32  m_ActorHandle{ 0 };

public:
	static Shared<WorldCollider> CreatePrototype();
	static Shared<WorldCollider> Create(const ComPtr<ID3D11Device>& device,
		const ComPtr<ID3D11DeviceContext>& context);
	Shared<Component> Clone(void* arg = nullptr) override;
};

NS_END
