#include "Pipeline.h"
#include "Shader.h"
#include "Game.h"

Pipeline::Pipeline()
{
}

HRESULT Pipeline::Bind_CameraPosition(const Shared<Shader>& shader, const Char* constantName)
{
	if (shader->Supports_CBuffer(ConstantBuffer::Camera))
		return Bind_CameraBuffer(shader);

	Float4 cameraPosition = Get_CamTransform();
	return shader->Bind_RawValue(constantName, &cameraPosition, sizeof(Float4));
}

HRESULT Pipeline::Bind_CameraBuffer(const Shared<Shader>& shader)
{
	if (nullptr == shader)
		return E_INVALIDARG;

	CameraCB cameraBuffer{};
	cameraBuffer.viewMatrix = m_TransformStateMatrices[ETOI(D3DTS::VIEW)];
	cameraBuffer.projMatrix = m_TransformStateMatrices[ETOI(D3DTS::PROJ)];
	cameraBuffer.viewInverseMatrix = m_TransformStateInverseMatrices[ETOI(D3DTS::VIEW)];
	cameraBuffer.projInverseMatrix = m_TransformStateInverseMatrices[ETOI(D3DTS::PROJ)];
	cameraBuffer.cameraPosition = Get_CamTransform();
	cameraBuffer.time.x = GAME_INSTANCE->Get_TotalTime();

	return shader->Bind_CBufferData(cameraBuffer);
}

HRESULT Pipeline::Bind_TransformMatrix(const Shared<Shader>& shader, const Char* constantName, D3DTS transformState)
{
	if (shader->Supports_CBuffer(ConstantBuffer::Camera))
		return Bind_CameraBuffer(shader);

	return shader->Bind_Matrix(constantName, &m_TransformStateMatrices[ETOI(transformState)]);
}

HRESULT Pipeline::Bind_TransformMatrix_Inverse(const Shared<Shader>& shader, const Char* constantName, D3DTS transformState)
{
	if (shader->Supports_CBuffer(ConstantBuffer::Camera))
		return Bind_CameraBuffer(shader);

	return shader->Bind_Matrix(constantName, &m_TransformStateInverseMatrices[ETOI(transformState)]);
}

void Pipeline::Update_Pipeline()
{
	for (uint32 i = 0; i < ETOI(D3DTS::END); ++i)
		m_TransformStateInverseMatrices[i] = m_TransformStateMatrices[i].Invert();

	// View마다 한 번 불리므로 시야 볼륨도 그 View의 카메라를 따라간다.
	// PROJ에서 카메라 공간 볼륨을 만들고 VIEW의 역행렬(= 카메라 월드 행렬)로 월드 공간에 올린다.
	const Matrix& proj = m_TransformStateMatrices[ETOI(D3DTS::PROJ)];
	m_Orthographic = proj._34 == 0.f && proj._44 == 1.f;
	if (m_Orthographic)
	{
		// BoundingFrustum::CreateFromMatrix는 원근 투영 전용이다. 직교 투영은 NDC 박스(x·y -1..1, z 0..1)를
		// 투영 역행렬로 되돌린 카메라 공간 박스가 곧 시야 볼륨이다.
		BoundingBox viewBounds{};
		BoundingBox::CreateFromPoints(viewBounds,
			Vector3::Transform(Vector3{ -1.f, -1.f, 0.f }, m_TransformStateInverseMatrices[ETOI(D3DTS::PROJ)]),
			Vector3::Transform(Vector3{ 1.f, 1.f, 1.f }, m_TransformStateInverseMatrices[ETOI(D3DTS::PROJ)]));
		BoundingOrientedBox::CreateFromBoundingBox(m_OrthoBounds, viewBounds);
		m_OrthoBounds.Transform(m_OrthoBounds, m_TransformStateInverseMatrices[ETOI(D3DTS::VIEW)]);
		return;
	}
	BoundingFrustum::CreateFromMatrix(m_Frustum, proj);
	m_Frustum.Transform(m_Frustum, m_TransformStateInverseMatrices[ETOI(D3DTS::VIEW)]);
}

Bool Pipeline::Is_Visible(const BoundingBox& worldBounds) const
{
	if (m_Orthographic)
		return m_OrthoBounds.Contains(worldBounds) != DISJOINT;
	return m_Frustum.Contains(worldBounds) != DISJOINT;
}

Unique<Pipeline> Pipeline::Create()
{
	auto pipeline = make_unique<Pipeline>();

	if (FAILED(pipeline->Initialize_Prototype()))
	{
		MSG_BOX("Failed To Create Pipeline");
		return nullptr;
	}

	return pipeline;
}
