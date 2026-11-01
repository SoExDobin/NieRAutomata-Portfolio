#pragma once
#include "StaticMesh.h"

NS_BEGIN(Engine)

/* 같은 정적 지오메트리를 인스턴스(INSTANCE_DATA)마다 한 번씩 그린다. 정점·인덱스 버퍼는 원본 StaticMesh와 공유하고
   인스턴스 버퍼만 자기 것이다. Model::Set_Instances가 클론의 공유 메시를 이 사본으로 바꾸므로, 프로토타입과
   같은 모델을 쓰는 다른 클론은 영향을 받지 않는다. 그리는 셰이더는 VTXMESH_INSTANCE 레이아웃이어야 한다. */
class InstanceMesh final : public StaticMesh
{
public:
	explicit InstanceMesh();
	explicit InstanceMesh(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit InstanceMesh(const InstanceMesh& rhs);
	// 공유 StaticMesh의 버퍼·인덱스 수·재질 인덱스·로컬 경계를 그대로 쓰는 인스턴스 사본. 인스턴스 버퍼는 비어 있다.
	explicit InstanceMesh(const StaticMesh& source);
	~InstanceMesh() override = default;

public:
	// 이번에 그릴 인스턴스들. 개수가 용량을 넘으면 버퍼를 다시 만들고, 아니면 덮어쓴다.
	HRESULT Set_Instances(const vector<INSTANCE_DATA>& instances);
	uint32 Get_NumInstances() const { return m_NumInstances; }

public:
	HRESULT Bind_Resources() override;
	/* F7 Instancing(Renderer)이 켜져 있으면 DrawIndexedInstanced 한 번으로 그린다. 꺼져 있으면 인스턴싱이 없는
	   렌더러처럼 인스턴스마다 그 world를 GPU에 올리고(Map/Unmap) 바인딩한 뒤 한 번씩 그린다. 화면은 같다. */
	HRESULT Render() override;
	void On_Destroy() override;

private:
	ComPtr<ID3D11Buffer> m_VBInstance{ nullptr };
	uint32 m_InstanceStride{ sizeof(INSTANCE_DATA) };
	uint32 m_NumInstances{};
	uint32 m_InstanceCapacity{};
	// F7 끔 경로: 인스턴스 하나만 담는 동적 버퍼와, 그 버퍼에 매 draw 올릴 인스턴스 원본.
	ComPtr<ID3D11Buffer> m_VBSingle{ nullptr };
	vector<INSTANCE_DATA> m_Instances;

public:
	Shared<Component> Clone(void* arg = nullptr) override { return nullptr; };
};

NS_END
