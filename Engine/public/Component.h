#pragma once
#include "Object.h"

NS_BEGIN(Engine)

class GameObject;

class ENGINE_DLL Component abstract : public Object, public enable_shared_from_this<Component> {
public:
	typedef struct tagComponentDesc : public OBJECT_DESC {} COMPONENT_DESC;
public:
    Component();
	explicit Component(const ComPtr<ID3D11Device> &pDevice, const ComPtr<ID3D11DeviceContext> &context);
	explicit Component(const Component& prototype);
	virtual ~Component() override = default;

public:
  Shared<GameObject> Get_Owner() const { return m_Owner.lock(); }

private:
  friend GameObject;
  void Set_Owner(const Shared<GameObject>& owner) { m_Owner = owner; }
	virtual HRESULT Post_Load() { return S_OK; }

public:
  virtual COMPONENT_TYPE Get_ComponentType() const PURE;

public:
  virtual HRESULT Initialize_Prototype() override;
  virtual HRESULT Initialize(void *arg) override;

protected:
  ComPtr<ID3D11Device> m_Device = {nullptr};
  ComPtr<ID3D11DeviceContext> m_Context = {nullptr};
  Weak<GameObject> m_Owner = {};

public:
  constexpr virtual Shared<Component> Clone(void *arg = nullptr) PURE;

};

NS_END
