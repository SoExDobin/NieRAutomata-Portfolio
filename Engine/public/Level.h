#pragma once
#include "Object.h"

NS_BEGIN(Engine)

class ENGINE_DLL Level abstract : public Object, public enable_shared_from_this<Level> {
public:
    typedef struct tagLevelDesc{} LEVEL_DESC;
public:
  explicit Level(const ComPtr<ID3D11Device> &device,
                 const ComPtr<ID3D11DeviceContext> &context);
  virtual ~Level() override = default;

public:
    Bool Load_Finished() const { return m_IsFinished.load(); }
    void Set_LoadFinishFlag(Bool isFinish) { m_IsFinished.store(isFinish); }

public:
  HRESULT Initialize_Prototype() override {
    return Object::Initialize_Prototype();
  }
  HRESULT Initialize(void *arg = nullptr) override {
    return Object::Initialize(arg);
  }
  void On_Destroy() override { Object::On_Destroy(); }
  void On_Disable() override { Object::On_Disable(); }
  void On_Enable() override { Object::On_Enable(); }
  void Set_Active(Bool isActive) override { Object::Set_Active(isActive); }

public:
	virtual void Update_Level(Float timeDelta);
	virtual HRESULT Render_Level();

protected:
	ComPtr<ID3D11Device> m_Device{nullptr};
	ComPtr<ID3D11DeviceContext> m_Context{nullptr};
    std::atomic_bool m_IsFinished{ true };
};

NS_END
