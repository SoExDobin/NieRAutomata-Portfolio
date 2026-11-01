#pragma once
#include "EngineManager.h"

NS_BEGIN(Engine)

class Shader;

class Pipeline final : public EngineManager
{
public:
	Pipeline();
	~Pipeline() override = default;
	
public:
	Matrix Get_Transform(D3DTS transformState) const
	{
		return m_TransformStateMatrices[ETOI(transformState)];
	}
	Matrix Get_InvTransform(D3DTS transformState) const
	{
		return m_TransformStateInverseMatrices[ETOI(transformState)];
	}
	void Set_Transform(D3DTS transformState, Matrix transformStateMatrix)
	{
		m_TransformStateMatrices[ETOI(transformState)] = transformStateMatrix;
	}
	Vector4 Get_CamTransform() const
	{
		Vector4 v4 = Vector4(m_TransformStateInverseMatrices[ETOI(D3DTS::VIEW)].Translation());
		v4.w = 1.f;
		return v4;
	}

public:
	HRESULT Bind_CameraPosition(const Shared<Shader>& shader, const Char* constantName);
	HRESULT Bind_CameraBuffer(const Shared<Shader>& shader);
	HRESULT Bind_TransformMatrix(const Shared<Shader>& shader, const Char* constantName, D3DTS transformState);
	HRESULT Bind_TransformMatrix_Inverse(const Shared<Shader>& shader, const Char* constantName, D3DTS transformState);
	void Update_Pipeline();

public:
	// 현재 View의 월드 공간 시야 볼륨과 겹치는지 본다. Update_Pipeline이 View마다 다시 만든다.
	// 원근 투영은 프러스텀, 직교 투영(그림자 캐스케이드)은 회전 박스로 판정한다.
	Bool Is_Visible(const BoundingBox& worldBounds) const;

private:
	Matrix m_TransformStateMatrices[ETOI(D3DTS::END)] = {};
	Matrix m_TransformStateInverseMatrices[ETOI(D3DTS::END)] = {};
	BoundingFrustum m_Frustum{};
	BoundingOrientedBox m_OrthoBounds{};
	Bool m_Orthographic{ false };

public:
	static Unique<Pipeline> Create();
};

NS_END
