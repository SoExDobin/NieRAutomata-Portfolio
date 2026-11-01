#pragma once
#include <GameObject.h>

NS_BEGIN(Engine)
class Model;
class Shader;
class Texture;
NS_END

NS_BEGIN(Client)

class Pl0000Parts;

class CLIENT_DLL WeaponHalo final : public GameObject
{
public:
	struct WEAPON_HALO_DESC : GAMEOBJECT_DESC
	{
		// The sheathed sword; the halo rides its bone0 frame where the blade runs along Z.
		Weak<Pl0000Parts> anchor{};
		Weak<Pl0000Parts> handAnchor{};
		Matrix localMatrix{ Matrix::Identity };
		Float bladeStart{};
		Float bladeEnd{};
	};

	explicit WeaponHalo() = default;
	explicit WeaponHalo(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit WeaponHalo(const WeaponHalo& rhs);
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;
	void Update(Float timeDelta) override;
	void Late_Update(Float timeDelta) override;
	HRESULT Render() override;
	void Submit_RenderGroup() override;
	void Begin_Transition(Bool toSheath);
	static Shared<WeaponHalo> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	Shared<GameObject> Clone(void* arg) override;

private:
	Weak<Pl0000Parts> m_Anchor{};
	Weak<Pl0000Parts> m_HandAnchor{};
	uint32 m_BoneIndex{};
	uint32 m_HandBoneIndex{};
	Shared<Model> m_Model{};
	Shared<Shader> m_Shader{};
	Shared<Texture> m_Texture{};
	ComPtr<ID3D11Buffer> m_GlowQuad{};	// the faint camera-facing glow (core.eff/093 record 0)
	Matrix m_LocalMatrix{ Matrix::Identity };
	Matrix m_WorldMatrix{ Matrix::Identity };
	Matrix m_TransitionMatrix{ Matrix::Identity };
	Float m_BladeStart{};
	Float m_BladeEnd{};
	Float m_TransitionTime{ 1.f };
	Bool m_ToSheath{};
	Bool m_Revealed{ true };
	// One turn and speed per mesh of Halo_core093.model, about the ring axis.
	Float m_Spin[3]{};
	Float m_SpinSpeed[3]{};
};

NS_END
