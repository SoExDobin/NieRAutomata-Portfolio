#pragma once
#include <Model.h>

#include "PartObject.h"

NS_BEGIN(Engine)
class Model;
NS_END

NS_BEGIN(Client)

class Pl0000;

class CLIENT_DLL Pl0000Parts abstract :public PartObject
{
public:
	typedef struct tagPl0000PartsDesc : public PARTOBJECT_DESC
	{
		
	} PL0000PART_DESC;
public:
	explicit Pl0000Parts();
	explicit Pl0000Parts(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit Pl0000Parts(const Pl0000Parts& rhs);
	virtual ~Pl0000Parts() override = default;

public:
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;

public:
	Shared<Model> Get_ModelComponent() { return m_Model; }
	void Set_Visible(Bool visible) { m_Visible = visible; }
	Bool Is_Visible() const { return m_Visible; }
	virtual void Set_Animation(uint32 animIndex, Float blendDuration, Bool isLoop);
	uint32 Get_CurrentAnimationIndex() const { return m_Model->Get_AnimationIndex(); }
	uint32 Get_NextAnimationIndex() const { return m_Model->Get_NextAnimationIndex(); }
	Float Get_AnimationProgress() const { return m_Model->Get_AnimationProgress(); }
	Bool Is_AnimationFinished() const { return m_Model->Is_AnimationFinished(); }

protected:
	// Render-only gate: attack colliders and animation keep their normal active lifetime.
	Bool m_Visible{ true };
	Shared<Shader>	m_Shader{ nullptr };
	Shared<Model>	m_Model{ nullptr };

public:
	Shared<GameObject> Clone(void* arg) override PURE;
};

NS_END
