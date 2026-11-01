#pragma once
#include "GameObject.h"

NS_BEGIN(Engine)

class ENGINE_DLL PartObject abstract : public GameObject
{
public:
	typedef struct tagPartObjectDesc : public GameObject::GAMEOBJECT_DESC {} PARTOBJECT_DESC;

public:
	explicit PartObject();
	explicit PartObject(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit PartObject(const PartObject& rhs);
	virtual ~PartObject() override = default;
	
public:
	Shared<GameObject> Get_Owner() const { return Get_Parent(); }
	const Matrix& Get_CombinedWorldMatrix() const { return m_CombinedWorldMatrix; }

public:
	GAMEOBJECTTYPE Get_GameObjectType() final { return GAMEOBJECTTYPE::PART; }
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;
	void On_Destroy() override { GameObject::On_Destroy(); }
	void On_Enable() override { GameObject::On_Enable(); }
	void On_Disable() override { GameObject::On_Disable(); }

public:
	void Priority_Update(Float timeDelta) override {}
	void Update(Float timeDelta) override {}
	void Late_Update(Float timeDelta) override {}
	void Fixed_Update(Float fixedDelta) override {}
	HRESULT Render() override { return S_OK; }
	virtual void Submit_RenderGroup() override {}

public:
	void Update_CombineWorldMatrix(const Matrix& childMatrix) {
		m_CombinedWorldMatrix = childMatrix;
	}

protected:
	Matrix			m_CombinedWorldMatrix{};

public:
	virtual Shared<GameObject> Clone(void* arg) PURE;
};

NS_END
