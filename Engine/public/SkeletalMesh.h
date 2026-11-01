#pragma once
#include "Mesh.h"

NS_BEGIN(Engine)

class Bone;

// 본 스키닝 메시. 정점은 bind space로 남고 pre-transform은 본을 통해 적용된다.
class SkeletalMesh final : public Mesh
{
public:
	explicit SkeletalMesh();
	explicit SkeletalMesh(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit SkeletalMesh(const SkeletalMesh& rhs);
	~SkeletalMesh() override = default;

public:
	// 모델 인스턴스의 현재 포즈로 스키닝 행렬(오프셋 × 결합)을 out에 채운다. out은 본 수만큼의 공간이 있어야 한다.
	// 메시는 같은 모델의 인스턴스끼리 공유하므로 결과는 호출한 Model이 들고 있는다.
	void Fill_BoneMatrices(const vector<Shared<Bone>>& bones, Matrix* out) const;
	uint32 Get_NumMeshBones() const { return m_NumBones; }
	/* Bind-space AABB of the vertices each palette bone dominates (largest weight), for posed bounds. */
	const vector<pair<uint32, BoundingBox>>& Get_DominantBoneBounds() const { return m_DominantBoneBounds; }

protected:
	HRESULT Ready_VertexBuffer(const MODEL_MESH& meshData, const Matrix& preTransformMatrix) override;

private:
	uint32				m_NumBones{};
	vector<uint32>		m_BoneIndices;
	vector<Matrix>		m_OffsetMatrices;
	vector<pair<uint32, BoundingBox>> m_DominantBoneBounds;

public:
	Shared<Component> Clone(void* arg = nullptr) override { return nullptr; };
};

NS_END
