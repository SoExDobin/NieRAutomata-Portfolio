#pragma once
#include <GameObject.h>

NS_BEGIN(Engine)
class OBBCollider;
class Shader;
class Texture;
NS_END

NS_BEGIN(Client)

class LaserEffect;

// Pod program R010: one beam from the pod. It pierces every monster on its path (one hit each)
// and stops at world collision, where LaserEffect plays the impact.
class CLIENT_DLL PodLaser final : public GameObject
{
public:
	struct POD_LASER_DESC : GAMEOBJECT_DESC
	{
		Weak<LaserEffect> impact{};
	};

	explicit PodLaser() = default;
	explicit PodLaser(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit PodLaser(const PodLaser& rhs);
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;
	void Update(Float timeDelta) override;
	void Late_Update(Float timeDelta) override;
	HRESULT Render() override;
	void Submit_RenderGroup() override;
	void OnCollisionStay(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider) override;

	// State2B_PodAttack calls this once per shot, when the pod has opened.
	void Fire(const Vector3& origin, const Vector3& direction);
	Bool Is_Beaming() const { return m_Age >= 0.f; }

	static Shared<PodLaser> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	Shared<GameObject> Clone(void* arg) override;

private:
	// The burst effect and its explosion sound at one impact point.
	void Play_Impact(const Vector3& position);

	Weak<LaserEffect> m_Impact{};
	Shared<Shader> m_Shader{};
	Shared<Texture> m_Texture{};
	Shared<OBBCollider> m_Collider{};
	ComPtr<ID3D11Buffer> m_VertexBuffer{};
	unordered_set<RuntimeObjectId> m_HitMonsters{};
	Vector3 m_Start{};
	Vector3 m_End{};
	Float m_Age{ -1.f };	// below zero while no beam is shown
};

NS_END
