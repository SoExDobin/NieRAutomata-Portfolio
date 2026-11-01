#pragma once
#include <GameObject.h>

NS_BEGIN(Engine)
class Model;
class OBBCollider;
class Shader;
class Texture;
NS_END

NS_BEGIN(Client)

class Pl0000Parts;

// What one sword swing leaves: the slash crescent (mesh ef0901) laid in the plane the blade swept stays there as the
// trail (wpf000.eff/111, refraction only), while a thick glass crescent wave flies off along the player's
// facing, lying across its flight as the swing did, dragging a short tunnel of torn slash crescents (coreeff 007).
// The wave's collider is on the PlayerWeapon layer, so it cancels enemy bullets as the blade does, and it pierces
// monsters (one hit each).
class CLIENT_DLL SwordWave final : public GameObject
{
public:
	explicit SwordWave() = default;
	explicit SwordWave(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit SwordWave(const SwordWave& rhs);
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;
	void Late_Update(Float timeDelta) override;
	HRESULT Render() override;
	void Submit_RenderGroup() override;
	void OnCollisionStay(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider) override;

	// A swing's attack window opened: follow the blade tip (bone0 offset) until the swing plane is known, then fly.
	// roll (degrees) is the swing's own wave roll about the facing from the original data (its z rotation); without
	// it the wave lies in the measured swing plane.
	void Swing(const Shared<Pl0000Parts>& weapon, const Vector3& tipOffset, ATK_TYPE attackType, const optional<Float>& roll);
	Bool Is_Idle() const { return m_Phase == PHASE::IDLE; }

	static Shared<SwordWave> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	Shared<GameObject> Clone(void* arg) override;

private:
	enum class PHASE { IDLE, TRACKING, FLYING };
	void Track(Float timeDelta);
	void Launch(const Vector3& pivot, const Vector3& tip);
	Matrix WaveMatrix(Float age) const;	// the flying wave's world matrix at this age of its flight
	HRESULT Draw(uint32 mesh, const char* pass, const Matrix& world, const Vector4& color);

	Shared<Model> m_Model{};
	Shared<Shader> m_Shader{};
	Shared<Texture> m_Texture{};
	Shared<OBBCollider> m_Collider{};
	Weak<Pl0000Parts> m_Weapon{};
	unordered_set<RuntimeObjectId> m_HitMonsters{};
	PHASE m_Phase{ PHASE::IDLE };
	ATK_TYPE m_AttackType{ ATK_TYPE::LIGHT };
	optional<Float> m_Roll{};
	Vector3 m_TipOffset{};
	Vector3 m_LastTip{};
	Vector3 m_Spin{};		// sum of r x dTip about the pivot: its direction is the swing plane's normal
	Float m_Swept{};		// radians the tip has turned about the pivot so far
	Float m_Age{};
	int32 m_BoneIndex{ -1 };
	Matrix m_Orientation{ Matrix::Identity };	// mesh space -> the wave's plane
	Vector3 m_Start{};
	Vector3 m_Velocity{};
	Matrix m_WorldMatrix{ Matrix::Identity };	// the flying wave
	Matrix m_TrailMatrix{ Matrix::Identity };	// the trail, where the swing was
	Float m_Opacity{};
	Float m_TrailOpacity{};
};

NS_END
