#pragma once
#include "Component.h"

NS_BEGIN(Engine)

class ENGINE_DLL ScriptComponent abstract : public Component {
public:
    explicit ScriptComponent();
	explicit ScriptComponent(const ComPtr<ID3D11Device> &device, const ComPtr<ID3D11DeviceContext> &context);
	explicit ScriptComponent(const ScriptComponent &rhs);
	virtual ~ScriptComponent() override = default;

public:
  COMPONENT_TYPE Get_ComponentType() const override {
    return COMPONENT_TYPE::SCRIPT;
  }
  virtual HRESULT Initialize_Prototype() override {
    return Component::Initialize_Prototype();
  }
  virtual HRESULT Initialize(void *arg = nullptr) override {
    return Component::Initialize(arg);
  }

  virtual void On_Destroy() override { return Component::On_Destroy(); }
  virtual void On_Disable() override { return Component::On_Disable(); }
  virtual void On_Enable() override { return Component::On_Enable(); }
  void Set_Active(Bool isActive) final { Component::Set_Active(isActive); };

public:
  virtual Shared<Component> Clone(void *arg = nullptr) override PURE;
};

NS_END