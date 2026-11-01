#pragma once
#include "Mesh.h"

NS_BEGIN(Engine)

// 본이 없는 메시. 모델의 pre-transform이 정점에 구워져 들어간다.
class StaticMesh : public Mesh
{
public:
	explicit StaticMesh();
	explicit StaticMesh(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit StaticMesh(const StaticMesh& rhs);
	~StaticMesh() override = default;

protected:
	HRESULT Ready_VertexBuffer(const MODEL_MESH& meshData, const Matrix& preTransformMatrix) override;

public:
	Shared<Component> Clone(void* arg = nullptr) override { return nullptr; };
};

NS_END
