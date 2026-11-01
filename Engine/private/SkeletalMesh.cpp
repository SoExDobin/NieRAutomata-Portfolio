#include "SkeletalMesh.h"

#include "Bone.h"
#include "SpdLogger.h"

SkeletalMesh::SkeletalMesh() : Mesh{} {}
SkeletalMesh::SkeletalMesh(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: Mesh{ device, context } {}
SkeletalMesh::SkeletalMesh(const SkeletalMesh& rhs)
	: Mesh{ rhs }, m_DominantBoneBounds{ rhs.m_DominantBoneBounds } {}

void SkeletalMesh::Fill_BoneMatrices(const vector<Shared<Bone>>& bones, Matrix* out) const
{
	for (uint32 i = 0; i < m_NumBones; ++i)
	{
		out[i] = m_OffsetMatrices[i] * (*bones[m_BoneIndices[i]]->Get_CombinedTransformationMatrixPtr());
	}
}

HRESULT SkeletalMesh::Ready_VertexBuffer(const MODEL_MESH& meshData, const Matrix& /*preTransformMatrix*/)
{
	m_BoneIndices = meshData.boneIndices;
	m_OffsetMatrices = meshData.offsetMatrices;
	m_NumBones = static_cast<uint32>(m_BoneIndices.size());

	m_VtxStride = sizeof(VTXANIMMESH);
	m_NumVertices = meshData.animVertices.size();

	D3D11_BUFFER_DESC vertexBufferDesc{};
	vertexBufferDesc.ByteWidth = m_VtxStride * m_NumVertices;
	vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertexBufferDesc.StructureByteStride = m_VtxStride;
	vertexBufferDesc.CPUAccessFlags = 0;
	vertexBufferDesc.MiscFlags = 0;

	VTXANIMMESH* vertices = new VTXANIMMESH[m_NumVertices];
	m_RawPosition.clear();
	m_RawPosition.reserve(m_NumVertices * 3);
	// Per palette bone: min/max of the vertices it dominates. The shader derives w = 1 - (x + y + z).
	vector<Vector3> boneMin(m_NumBones, Vector3(FLT_MAX)), boneMax(m_NumBones, Vector3(-FLT_MAX));
	// The uploaded palette has max(1, bones) matrices and its unused tail is not cleared, so an index
	// past it would read stale data. Such an index points at palette entry 0 instead.
	const uint32 paletteSize = std::max(1u, m_NumBones);
	size_t remappedVertices = 0;
	for (size_t i = 0; i < m_NumVertices; i++)
	{
		vertices[i] = meshData.animVertices[i];
		uint32* const blendIndex = &vertices[i].blendIndex.x;
		Bool remapped = false;
		for (uint32 k = 0; k < 4; ++k)
			if (blendIndex[k] >= paletteSize) { blendIndex[k] = 0; remapped = true; }
		remappedVertices += remapped;
		m_RawPosition.push_back(vertices[i].position.x);
		m_RawPosition.push_back(vertices[i].position.y);
		m_RawPosition.push_back(vertices[i].position.z);

		const Vector4& weight = vertices[i].blendWeight;
		const Float weights[4]{ weight.x, weight.y, weight.z, 1.f - (weight.x + weight.y + weight.z) };
		const uint32 indices[4]{ vertices[i].blendIndex.x, vertices[i].blendIndex.y, vertices[i].blendIndex.z, vertices[i].blendIndex.w };
		uint32 dominant = 0;
		for (uint32 k = 1; k < 4; ++k)
			if (weights[k] > weights[dominant]) dominant = k;
		if (indices[dominant] < m_NumBones)
		{
			const Vector3 position(vertices[i].position);
			boneMin[indices[dominant]] = Vector3::Min(boneMin[indices[dominant]], position);
			boneMax[indices[dominant]] = Vector3::Max(boneMax[indices[dominant]], position);
		}
	}
	if (remappedVertices > 0)
		LOG_WARN(L"Skeletal mesh: {} of {} vertices had bone indices past its {} bones; remapped to bone 0",
			remappedVertices, m_NumVertices, m_NumBones);
	m_DominantBoneBounds.clear();
	for (uint32 bone = 0; bone < m_NumBones; ++bone)
	{
		if (boneMin[bone].x > boneMax[bone].x)
			continue;
		BoundingBox box;
		BoundingBox::CreateFromPoints(box, boneMin[bone], boneMax[bone]);
		m_DominantBoneBounds.emplace_back(bone, box);
	}

	D3D11_SUBRESOURCE_DATA vertexInitialData{};
	vertexInitialData.pSysMem = vertices;

	if (FAILED(m_Device->CreateBuffer(&vertexBufferDesc, &vertexInitialData, &m_VB)))
	{
		LOG_ERROR(L"Failed to Create Mesh vertices");
		MSG_BOX("Failed to Create Mesh vertices");
		delete[] vertices;
		return E_FAIL;
	}

	delete[] vertices;
	return S_OK;
}
