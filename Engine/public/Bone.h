#pragma once
#include "Component.h"

NS_BEGIN(Engine)

class Bone final : public Component
{
public:
	explicit Bone();
	explicit Bone(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit Bone(const Bone& rhs);
	~Bone() override = default;

public:
	void On_Destroy() override { Component::On_Destroy(); }
	void On_Disable() override { Component::On_Disable(); }
	void On_Enable() override { Component::On_Enable(); }
	void Set_Active(Bool isActive) override { Component::Set_Active(isActive); }
	
public:
	COMPONENT_TYPE Get_ComponentType() const override { return COMPONENT_TYPE::BONE; };
	HRESULT Initialize_Prototype(const MODEL_BONE& modelBone);
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;

public:
	Bool Is_SameBone(const Char* boneName) const { return !strcmp(boneName, m_BoneName); }
	const Matrix* Get_CombinedTransformationMatrixPtr() const { return &m_CombinedTransformationMatrix; }
	const Matrix& Get_TransformationMatrix() const { return m_TransformationMatrix; }
	int32 Get_ParentBoneIndex() const { return m_ParentBoneIndex; }
	void Update_TransformationMatrix(const Matrix& transformationMatrix);
	void Update_CombinedTransformationMatrix(const vector<Shared<Bone>>& modelBones, const Matrix& preTransformMatrix);
	// 애니메이션 값은 두고 이번 포즈의 결합 행렬만 바꾼다(Model::Override_BoneMatrices).
	void Set_CombinedTransformationMatrix(const Matrix& combinedMatrix) { m_CombinedTransformationMatrix = combinedMatrix; }

private:
	Char			m_BoneName[MAX_PATH]{};
	int32			m_ParentBoneIndex{};
	Matrix			m_TransformationMatrix{};
	Matrix			m_CombinedTransformationMatrix{};

public:
	static Shared<Bone> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context, const MODEL_BONE& boneData);
	Shared<Component> Clone(void* arg = nullptr) override;
};

NS_END
