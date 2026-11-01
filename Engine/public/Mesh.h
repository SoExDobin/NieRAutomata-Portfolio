#pragma once
#include "VIBuffer.h"

NS_BEGIN(Engine)

class Shader;
class Bone;

// 모든 모델 메시의 공통 기반. 정점 형식은 하위 타입(StaticMesh/SkeletalMesh/InstanceMesh)이 정한다.
class Mesh : public VIBuffer
{
public:
	explicit Mesh();
	explicit Mesh(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit Mesh(const Mesh& rhs);
	~Mesh() override = default;

public:
	COMPONENT_TYPE Get_ComponentType() const override { return COMPONENT_TYPE::MESH; }
	HRESULT Initialize_Prototype(const MODEL_MESH& meshData, const Matrix& preTransformMatrix);
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg = nullptr) override;
	void On_Destroy() override;

public:
	const vector<Float>& Get_RawPositions() const { return m_RawPosition; }
	const vector<int32>& Get_RawIndices() const { return m_RawIndices; }

public:
	uint32 Get_MaterialIndex() const { return m_MaterialIndex; }
	const string& Get_MeshName() const { return m_MeshName; }
	// 정점 기준 로컬 AABB. 정적 메시는 pre-transform이 반영된 값이고, 스켈레탈은 bind space다.
	const BoundingBox& Get_LocalBounds() const { return m_LocalBounds; }
	Bool Has_LocalBounds() const { return m_HasLocalBounds; }

protected:
	// 하위 타입이 자기 정점 형식으로 정점 버퍼를 만들고 m_RawPosition을 채운다.
	virtual HRESULT Ready_VertexBuffer(const MODEL_MESH& meshData, const Matrix& preTransformMatrix);

private:
	HRESULT Ready_IndexBuffer(const MODEL_MESH& meshData);
	void Compute_LocalBounds();

protected:
	string				m_MeshName{};
	uint32				m_MaterialIndex{};

protected:
	vector<Float> m_RawPosition;
	vector<int32> m_RawIndices;
	BoundingBox m_LocalBounds{};
	Bool m_HasLocalBounds{ false };

public:
	static Shared<Mesh> CreatePrototype();
	// isAnim으로 StaticMesh/SkeletalMesh 중 알맞은 하위 타입을 만든다.
	static Shared<Mesh> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context, Bool isAnim, const MODEL_MESH& meshData, const Matrix& preTransformMatrix);
	Shared<Component> Clone(void* arg = nullptr) override { return nullptr; };
};

NS_END
