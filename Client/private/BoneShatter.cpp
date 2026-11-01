#include "pch.h"
#include "BoneShatter.h"

#include <Model.h>
#include <Shader.h>

void BoneShatter::Begin(const Shared<Model>& model, const Vector3& center)
{
	if (Is_Running() || !model) return;

	// Each part pivots on the centre of the vertices its bone dominates.
	constexpr size_t maxParts = 256;
	const uint32 numMeshes = static_cast<uint32>(model->Get_NumMeshes());
	m_PartCenters.assign(numMeshes, {});
	vector<Vector3> centers;
	for (uint32 i = 0; i < numMeshes; ++i)
	{
		if (!model->Get_MeshPartCenters(i, centers) || centers.size() > maxParts)
			continue;
		for (const Vector3& part : centers)
			m_PartCenters[i].emplace_back(part.x, part.y, part.z, 1.f);
	}
	m_Center = center;
	m_Time = 0.f;
}

HRESULT BoneShatter::Bind(const Shared<Shader>& shader) const
{
	if (FAILED(shader->Bind_RawValue("g_ShatterTime", &m_Time, sizeof(Float))) ||
		FAILED(shader->Bind_RawValue("g_ShatterDuration", &Duration, sizeof(Float))) ||
		FAILED(shader->Bind_RawValue("g_ShatterCenter", &m_Center, sizeof(Vector3))))
		return E_FAIL;
	return S_OK;
}

HRESULT BoneShatter::Begin_Mesh(const Shared<Shader>& shader, uint32 meshIndex) const
{
	if (meshIndex >= m_PartCenters.size() || m_PartCenters[meshIndex].empty())
		return S_FALSE;
	const vector<Vector4>& centers = m_PartCenters[meshIndex];
	if (FAILED(shader->Bind_RawValue("g_PartCenters", centers.data(), static_cast<uint32>(centers.size() * sizeof(Vector4)))) ||
		FAILED(shader->Bind_RawValue("g_ShatterSeed", &meshIndex, sizeof(uint32))) ||
		FAILED(shader->Begin("Shatter")))
		return E_FAIL;
	return S_OK;
}
