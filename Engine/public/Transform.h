#pragma once
#include "Component.h"

NS_BEGIN(Engine)

class Shader;

class ENGINE_DLL Transform final : public Component, public enable_shared_from_this<Transform> 
{
public:
    explicit Transform();
    explicit Transform(const ComPtr<ID3D11Device> &device, const ComPtr<ID3D11DeviceContext> &context);
    explicit Transform(const Transform& prototype);
    ~Transform() override = default;

public: /* Local Getter */
    Vector3 Get_LocalScale() const;
    Vector3 Get_LocalPosition() const;
    Quaternion Get_LocalRotation() const;
    Vector3 Get_LocalEulerAngles() const;
    Matrix Get_LocalMatrix() const;

public: /* Local Setter */
    void Set_LocalScale(const Vector3 &scale);
    void Set_LocalScale(Float x, Float y, Float z);
    void Set_LocalScaleByValue(Vector3 scale);

    void Set_LocalRotation(const Quaternion& rotation);
    void Set_LocalRotation(const Vector3& eulerAngles);
    void Set_LocalRotation(Float pitch, Float yaw, Float roll);
    void Set_LocalEulerAngleByValue(Vector3 rotation);

    void Set_LocalPosition(const Vector3 &position);
    void Set_LocalPosition(Float x, Float y, Float z);
    void Set_LocalPositionByValue(Vector3 position);

public: /* World Getter */
    Vector3 Get_Scale() const;
    Vector3 Get_Rotation() const;
    Quaternion Get_Quaternion() const;
    Vector3 Get_Position() const;
    Matrix Get_WorldMatrix() const;
    Matrix* Get_WorldMatrixPtr();
    Vector3 Get_Right() const;
    Vector3 Get_Up() const;
    Vector3 Get_Look() const;

public: /* World Setter */
    void Set_Scale(Float scaleX, Float scaleY, Float scaleZ);
    void Set_Scale(Vector3 scaleVec);

    void Set_Rotation(Vector3 eulerAngles);
    void Set_Rotation(Float pitch, Float yaw, Float roll);
    void Set_Rotation(Quaternion rotation);

    void Set_Position(Float scaleX, Float scaleY, Float scaleZ);
    void Set_Position(Vector3 positionVec);

    void Set_WorldMatrix(const Matrix& matrix) {
        m_WorldMatrix = matrix;
        m_IsDirty = false; // 엔진의 자동 연산을 방지
    }

public: /* Util Method */
    void Move_Forward(Float delta, Float amount = 1.f);
    void Move_Backward(Float delta, Float amount = 1.f);
    void Move_Right(Float delta, Float amount = 1.f);
    void Move_Left(Float delta, Float amount = 1.f);
    void LookAt(Vector3 atVec, Vector3 upVector = Vector3::UnitY);
    void Turn(Vector3 eulerAmount, Float timeDelta, Float amount = 1.f);

    HRESULT Bind_ShaderResource(const Shared<Shader>& shader, const Char* constantName) const;

private:
	Shared<Transform> Get_Parent() const;

public:
    Bool Is_Dirty() const { return m_IsDirty; }
    void Set_Dirty() { m_IsDirty = true; }

public: /* override */
    COMPONENT_TYPE Get_ComponentType() const override { return COMPONENT_TYPE::TRANSFORM; }
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void *arg) override;
    void On_Destroy() override;
    void On_Disable() override;
    void On_Enable() override;

public:
	void Update_WorldMatrix();

private:
    Vector3 m_LocalScale = {Vector3::One};
    Quaternion m_LocalRotation = {Quaternion::Identity};
    Vector3 m_LocalPosition = {Vector3::Zero};
    Matrix m_WorldMatrix = {Matrix::Identity};
    Bool m_IsDirty = {true};

public:
    static Shared<Transform> CreatePrototype();
    static Shared<Transform> Create(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context);
    Shared<Component> Clone(void *arg) override;

};

NS_END