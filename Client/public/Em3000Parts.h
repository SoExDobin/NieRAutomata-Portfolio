#pragma once
#include "PartObject.h"
#include "BoneShatter.h"
#include "Shader.h"

NS_BEGIN(Engine)
class Model;
class SoftBody;
NS_END

NS_BEGIN(Client)

class Em3000;

class CLIENT_DLL Em3000Parts abstract : public PartObject
{
public:
	typedef struct tagEm3000PartsDesc : public PARTOBJECT_DESC
	{
		Shader::SHADER_DESC shaderDesc{};
		wstring modelResourceTag{};
		// Set to hang the part on one of the boss's bones. The part's root bone sits on that bone at rest.
		Weak<Model> parentModel{};
		string parentBoneName{};
		string rootBoneName{};
	} EM3000PART_DESC;
public:
	explicit Em3000Parts();
	explicit Em3000Parts(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context); 
	explicit Em3000Parts(const Em3000Parts& rhs);
	virtual ~Em3000Parts() override = default;

public:
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;

public:
	Shared<Model> Get_ModelComponent() { return m_Model; }
	// The phase 1 stagger: the cloth burns away where it hangs (Shader_VtxAnimMesh Burn/BurnEdge), then the part is gone.
	void Begin_Burn() { if (m_BurnTime < 0.f) m_BurnTime = 0.f; }
	// Death: the swing stops and the part breaks into its bones' pieces with the body (BoneShatter).
	void Begin_Shatter(const Vector3& center);

public:
	void Update(Float timeDelta) override;
	void Late_Update(Float timeDelta) override;
	
	HRESULT Render() override;
	void Submit_RenderGroup() override;

protected:
	virtual HRESULT Bind_ShaderResources();

private:
	HRESULT Ready_Components(Shader::SHADER_DESC shaderDesc, const wstring& modelResourceTag);
	// Puts the bind-pose part onto the parent bone as it is posed now (m_CombinedWorldMatrix).
	void Update_PartWorldMatrix();
	// g_BurnProgress of the stagger burn; < 0 while it does not run.
	Float Get_BurnProgress() const;

protected:
	Shared<Shader>	m_Shader{ nullptr };
	Shared<Model>	m_Model{ nullptr };
	// Chains and strings swing on <model tag>_cloth.json; a part without one stays in its bind pose.
	Shared<SoftBody>	m_SoftBody{ nullptr };

private:
	Weak<Model>		m_ParentModel{};
	int32			m_ParentBoneIndex{ -1 };
	int32			m_RootBoneIndex{ -1 };
	Float			m_BurnTime{ -1.f };		// < 0 while whole
	BoneShatter		m_Shatter{};

	static constexpr Float BurnDuration = 2.f;

public:
	Shared<GameObject> Clone(void* arg) override PURE;
};

NS_END
