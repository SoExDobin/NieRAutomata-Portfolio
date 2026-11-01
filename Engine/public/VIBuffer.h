#pragma once
#include "Component.h"

NS_BEGIN(Engine)

class ENGINE_DLL VIBuffer abstract : public Component 
{
public:
	typedef struct tagVIBuffer : public Component
	{
		uint32 m_NumVtxBuffers{};
		uint32 m_NumVertices{};
		uint32 m_VtxStride{};

		uint32 m_NumIndices{};
		uint32 m_IndexStride{};
	} VIBUFFER;

public:
    VIBuffer();
  VIBuffer(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context);
  VIBuffer(const VIBuffer& rhs);
  virtual ~VIBuffer() override = default;

public:
  virtual HRESULT Initialize_Prototype() override;
  virtual HRESULT Initialize(void *arg = nullptr) override;
  virtual void On_Destroy() override { Component::On_Destroy(); }
  virtual void On_Disable() override { Component::On_Disable(); }
  virtual void On_Enable() override { Component::On_Enable(); }
  virtual void Set_Active(Bool isActive) override {
    Component::Set_Active(isActive);
  }
  virtual COMPONENT_TYPE Get_ComponentType() const override PURE;
  uint32 Get_NumVertices() const { return m_NumVertices; }
  uint32 Get_NumIndices() const { return m_NumIndices; }

public:
  virtual HRESULT Bind_Resources();
  virtual HRESULT Render();

protected:
  // Debug 빌드에서 이번 draw를 Renderer 프레임 통계에 더한다.
  void Count_Draw(uint32 indexCount, uint32 instanceCount = 1) const;

protected:
  ComPtr<ID3D11Buffer> m_VB = {nullptr};
  ComPtr<ID3D11Buffer> m_IB = {nullptr};

protected:
  uint32 m_NumVtxBuffers = {};
  uint32 m_NumVertices = {};
  uint32 m_VtxStride = {};

  uint32 m_NumIndices = {};
  uint32 m_IndexStride = {};
  D3D11_PRIMITIVE_TOPOLOGY m_PrimitiveType = {};

public:
  virtual Shared<Component> Clone(void *arg) override PURE;
};

NS_END