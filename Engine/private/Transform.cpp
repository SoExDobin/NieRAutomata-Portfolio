#include "Transform.h"
#include "GameObject.h"
#include "Shader.h"
#include "Game.h"

Transform::Transform() : Component{} {}
Transform::Transform(const ComPtr<ID3D11Device> &device,
                     const ComPtr<ID3D11DeviceContext> &context)
    : Component(device, context) {
  m_LocalScale = {Vector3::One};
  m_LocalRotation = {Quaternion::Identity};
  m_LocalPosition = {Vector3::Zero};
  m_WorldMatrix = {Matrix::Identity};
}
Transform::Transform(const Transform &prototype)
    : Component(prototype), m_LocalScale(prototype.m_LocalScale),
      m_LocalRotation(prototype.m_LocalRotation),
      m_LocalPosition(prototype.m_LocalPosition),
      m_WorldMatrix(prototype.m_WorldMatrix), m_IsDirty(true) {}

Vector3 Transform::Get_LocalScale() const { return m_LocalScale; }
Vector3 Transform::Get_LocalPosition() const { return m_LocalPosition; }
Quaternion Transform::Get_LocalRotation() const { return m_LocalRotation; }
Vector3 Transform::Get_LocalEulerAngles() const {
    Vector3 rad = m_LocalRotation.ToEuler();
    return Vector3{
        XMConvertToDegrees(rad.x),
        XMConvertToDegrees(rad.y),
        XMConvertToDegrees(rad.z)
    };
}
Matrix Transform::Get_LocalMatrix() const {
    Matrix scale = Matrix::CreateScale(m_LocalScale);
    Matrix rotation = Matrix::CreateFromQuaternion(m_LocalRotation);
    Matrix position = Matrix::CreateTranslation(m_LocalPosition);

    return scale * rotation * position;
}

void Transform::Set_LocalScale(const Vector3& scale) {
    m_LocalScale = scale;
    m_IsDirty = true;
}

void Transform::Set_LocalScale(Float x, Float y, Float z) {
  Set_LocalScale(Vector3{x, y, z});
}

void Transform::Set_LocalScaleByValue(Vector3 scale) {
    m_LocalScale = scale;
    m_IsDirty = true;
    Update_WorldMatrix();
}

void Transform::Set_LocalEulerAngleByValue(Vector3 rotation) {
    Set_LocalRotation(rotation);
    Update_WorldMatrix();
}

void Transform::Set_LocalPositionByValue(Vector3 position) {
    m_LocalPosition = position;
    m_IsDirty = true;
    Update_WorldMatrix();
}

void Transform::Set_LocalRotation(const Quaternion &rotation) {
  m_LocalRotation = rotation;
  m_IsDirty = true;
}
void Transform::Set_LocalRotation(const Vector3 &eulerAngles) {
  m_LocalRotation = Quaternion::CreateFromYawPitchRoll(
      XMConvertToRadians(eulerAngles.y), XMConvertToRadians(eulerAngles.x),
      XMConvertToRadians(eulerAngles.z));
  m_IsDirty = true;
}

void Transform::Set_LocalRotation(Float pitch, Float yaw, Float roll) {
	Set_LocalRotation(Vector3{pitch, yaw, roll});
}

void Transform::Set_LocalPosition(const Vector3 &position) {
	m_LocalPosition = position;
	m_IsDirty = true;
}
void Transform::Set_LocalPosition(Float x, Float y, Float z) {
	Set_LocalPosition(Vector3{x, y, z});
}

Vector3 Transform::Get_Scale() const {
  return Vector3{XMVectorGetX(XMVector3Length(m_WorldMatrix.Right())),
                 XMVectorGetX(XMVector3Length(m_WorldMatrix.Up())),
                 XMVectorGetX(XMVector3Length(m_WorldMatrix.Backward()))};
}

Vector3 Transform::Get_Rotation() const {
  return Get_Quaternion().ToEuler();
}

Quaternion Transform::Get_Quaternion() const {
  Matrix normalized = m_WorldMatrix;

  normalized.Right(XMVector3Normalize(normalized.Right()));
  normalized.Up(XMVector3Normalize(normalized.Up()));
  normalized.Backward(XMVector3Normalize(normalized.Backward()));

  return Quaternion::CreateFromRotationMatrix(normalized);
}

Vector3 Transform::Get_Position() const { return m_WorldMatrix.Translation(); }
Matrix Transform::Get_WorldMatrix() const { return m_WorldMatrix; }
Matrix* Transform::Get_WorldMatrixPtr()
{
    return &m_WorldMatrix;
}

Vector3 Transform::Get_Right() const
{
    return m_WorldMatrix.Right();
}

Vector3 Transform::Get_Up() const
{
    return m_WorldMatrix.Up();
}

Vector3 Transform::Get_Look() const
{
    return m_WorldMatrix.Backward();
}

void Transform::Set_Scale(Float scaleX, Float scaleY, Float scaleZ) {
  Set_Scale(Vector3{scaleX, scaleY, scaleZ});
}

void Transform::Set_Scale(Vector3 scaleVec) {
  if (!m_Owner.lock()->Has_Parent())
    Set_LocalScale(scaleVec);
  else {
    auto parent = Get_Parent();
    Vector3 parentScale = parent->Get_Scale();
    m_LocalScale =
        Vector3{scaleVec.x / parentScale.x, scaleVec.y / parentScale.y,
                scaleVec.z / parentScale.z};
    m_IsDirty = true;
  }
}

void Transform::Set_Rotation(Vector3 eulerAngles) {
  if (!m_Owner.lock()->Has_Parent())
    Set_LocalRotation(eulerAngles);
  else {
    auto parent = Get_Parent();
    Quaternion worldQuat = Quaternion::CreateFromYawPitchRoll(
        XMConvertToRadians(eulerAngles.y), XMConvertToRadians(eulerAngles.x),
        XMConvertToRadians(eulerAngles.z));
    Quaternion parentQuat = parent->Get_Quaternion();
    parentQuat.Inverse(parentQuat);
    m_LocalRotation = worldQuat * parentQuat;
    m_IsDirty = true;
  }
}

void Transform::Set_Rotation(Float pitch, Float yaw, Float roll) {
  Set_Rotation(Vector3{pitch, yaw, roll});
}
void Transform::Set_Rotation(Quaternion rotation) {
  if (!m_Owner.lock()->Has_Parent())
    Set_LocalRotation(rotation);
  else {
    auto parent = Get_Parent();
    Quaternion parentQuat = parent->Get_Quaternion();
    parentQuat.Inverse(parentQuat);
    m_LocalRotation = rotation * parentQuat;
    m_IsDirty = true;
  }
}

void Transform::Set_Position(Float x, Float y, Float z) {
  Set_Position(Vector3{x, y, z});
}
void Transform::Set_Position(Vector3 positionVec) {
  if (!m_Owner.lock()->Has_Parent())
    Set_LocalPosition(positionVec);
  else {
    auto parent = Get_Parent();
    Matrix parentWorldInv = parent->Get_WorldMatrix().Invert();
    m_LocalPosition = Vector3::Transform(positionVec, parentWorldInv);
    m_IsDirty = true;
  }
}

void Transform::Move_Forward(Float delta, Float amount) {
  Vector3 look = XMVector3Normalize(m_WorldMatrix.Backward());
  m_LocalPosition += look * amount * delta;
  m_IsDirty = true;
}

void Transform::Move_Backward(Float delta, Float amount) {
  Vector3 look = XMVector3Normalize(m_WorldMatrix.Backward());
  m_LocalPosition -= look * amount * delta;
  m_IsDirty = true;
}

void Transform::Move_Right(Float delta, Float amount) {
  Vector3 right = XMVector3Normalize(m_WorldMatrix.Right());
  m_LocalPosition += right * amount * delta;
  m_IsDirty = true;
}

void Transform::Move_Left(Float delta, Float amount) {
  Vector3 right = XMVector3Normalize(m_WorldMatrix.Right());
  m_LocalPosition -= right * amount * delta;
  m_IsDirty = true;
}

void Transform::LookAt(Vector3 atVec, Vector3 upVector) {
  Update_WorldMatrix();
  Vector3 position = m_WorldMatrix.Translation();
  Vector3 lookDir = atVec - position;

  if (lookDir.LengthSquared() < 0.0001f)
    return;

  lookDir.Normalize();
  if (upVector != Vector3::UnitY)
    upVector.Normalize();

  if (abs(lookDir.Dot(upVector)) > 0.999f)
    upVector = (abs(lookDir.y) > 0.999f) ? Vector3::UnitZ : Vector3::UnitY;

  Vector3 rightDir = XMVector3Cross(upVector, lookDir);
  rightDir.Normalize();
  Vector3 upDir = XMVector3Cross(lookDir, rightDir);

  Matrix rotationMat = Matrix::Identity;
  rotationMat.Right(rightDir);
  rotationMat.Up(upDir);
  rotationMat.Backward(lookDir);

  m_LocalRotation = Quaternion::CreateFromRotationMatrix(rotationMat);

  if (!m_Owner.expired() && m_Owner.lock()->Has_Parent()) {
    auto parent = Get_Parent();
    Quaternion parentQuat = parent->Get_Quaternion();
    parentQuat.Inverse(parentQuat);
    m_LocalRotation = m_LocalRotation * parentQuat;
  }

  m_IsDirty = true;
}

void Transform::Turn(Vector3 eulerAmount, Float timeDelta, Float amount) {
    Float deltaPitch = XMConvertToRadians(eulerAmount.x) * timeDelta * amount;
    Float deltaYaw = XMConvertToRadians(eulerAmount.y) * timeDelta * amount;
    Float deltaRoll = XMConvertToRadians(eulerAmount.z) * timeDelta * amount;

    Quaternion yawQuat = Quaternion::CreateFromAxisAngle(Vector3::UnitY, deltaYaw);

    Vector3 rightVec = m_WorldMatrix.Right();
    rightVec.Normalize();
    Quaternion pitchQuat = Quaternion::CreateFromAxisAngle(rightVec, deltaPitch);

    Vector3 lookVec = m_WorldMatrix.Backward();
    lookVec.Normalize();
    Quaternion rollQuat = Quaternion::CreateFromAxisAngle(lookVec, deltaRoll);

    Quaternion rotate = m_LocalRotation * yawQuat * pitchQuat * rollQuat;

    if (auto parent = m_Owner.lock()->Get_Parent())
    {
        Quaternion parentQuat = parent->Get_Transform()->Get_Quaternion();
        Quaternion parentQuatInv;
        parentQuat.Inverse(parentQuatInv);

        // 내 회전 * 부모 역행렬 = 순수 Local 회전
        m_LocalRotation = rotate * parentQuatInv;
    }
    else
    {
        m_LocalRotation = rotate;
    }
    
    m_LocalRotation.Normalize();
    m_IsDirty = true;
}

HRESULT Transform::Bind_ShaderResource(const Shared<Shader> &shader,
                                       const Char *constantName) const {
  return shader->Bind_Matrix(constantName, &m_WorldMatrix);
}

Shared<Transform> Transform::Get_Parent() const {
  if (auto owner = m_Owner.lock()) {
    if (owner->Has_Parent()) {
      auto parentObj = owner->Get_Parent();
      if (parentObj)
        return parentObj->Get_Transform();
    }
  }
  return nullptr;
}

HRESULT Transform::Initialize_Prototype() {
    m_LocalPosition = Vector3::Zero;
    m_LocalRotation = Quaternion{0.f, 0.f, 0.f, 1.f};
    m_LocalScale = Vector3::One;
    m_WorldMatrix = Matrix::Identity;
	return Component::Initialize_Prototype();
}

HRESULT Transform::Initialize(void *arg) {
  Update_WorldMatrix();
  return Component::Initialize(arg);
}

void Transform::On_Destroy() {
  m_Owner.reset();

  Component::On_Destroy();
}

void Transform::On_Disable() { Component::On_Disable(); }

void Transform::On_Enable() {
  m_IsDirty = true;
  Update_WorldMatrix();

  Component::On_Enable();
}

void Transform::Update_WorldMatrix() {
  if (m_IsDestroy == true)
    return;
  if (m_IsActive == false)
    return;
  const Shared<GameObject> owner = m_Owner.lock();
  if (m_IsDirty == false && (!owner || !owner->Has_Parent()))
    return;

  Matrix localMatrix = Get_LocalMatrix();

  if (!owner || !owner->Has_Parent())
    m_WorldMatrix = localMatrix;
  else {
    auto parent = Get_Parent();
    if (parent)
      m_WorldMatrix = localMatrix * parent->Get_WorldMatrix();
    else
      m_WorldMatrix = localMatrix;
  }

  m_IsDirty = false; // dirty 플래그 리셋
}

Shared<Transform> Transform::CreatePrototype()
{
    auto transform = make_shared<Transform>(
        GAME_INSTANCE->Get_Device(), GAME_INSTANCE->Get_Context());

    if (FAILED(transform->Initialize_Prototype())) {
        MSG_BOX("Failed To Create Transform Prototype");
        return nullptr;
    }

    return transform;
}

Shared<Transform> Transform::Create(ComPtr<ID3D11Device> device,
                                    ComPtr<ID3D11DeviceContext> context) {
  auto transform = make_shared<Transform>(device, context);

  if (FAILED(transform->Initialize_Prototype())) {
    MSG_BOX("Failed To Create Transform Prototype");
    return nullptr;
  }

  return transform;
}

Shared<Component> Transform::Clone(void *arg) {
  auto clone = make_shared<Transform>(*this);

  if (FAILED(clone->Initialize(arg))) {
    MSG_BOX("Failed To Clone Transform");
    return nullptr;
  }

  return clone;
}
