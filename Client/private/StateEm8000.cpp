#include "pch.h"
#include "StateEm8000.h"

#include "Em8000Body.h"
#include "EmMovement.h"
#include "Model.h"

StateEm8000::StateEm8000(const wstring& tag, const Shared<Em8000>& owner)
	: State{ tag }, m_Owner{ owner } {}

StateEm8000::~StateEm8000()
{
	m_Owner.reset();
	m_Body.reset();
	m_Movement.reset();
}

HRESULT StateEm8000::Initialize()
{
	if (m_Owner.expired()) return E_FAIL;
	m_Body = m_Owner.lock()->Get_Body();
	m_Movement = m_Owner.lock()->Get_Component<EmMovement>();
	return m_Body.expired() || m_Movement.expired() ? E_FAIL : S_OK;
}

Bool StateEm8000::Has_Target() const
{
	return !m_Owner.expired() && m_Owner.lock()->Has_Target();
}

Float StateEm8000::Get_DistanceToTarget() const
{
	return m_Owner.expired() ? FLT_MAX : m_Owner.lock()->Get_DistanceToTarget();
}

Vector3 StateEm8000::Get_DirectionToTarget() const
{
	return m_Owner.expired() ? Vector3::Zero : m_Owner.lock()->Get_DirectionToTarget();
}

Float StateEm8000::Get_AngleToTarget() const
{
	if (m_Owner.expired() || !Has_Target()) return 180.f;
	Vector3 look = m_Owner.lock()->Get_Transform()->Get_Look();
	look.y = 0.f;
	look.Normalize();
	const Float cosine = std::clamp(look.Dot(Get_DirectionToTarget()), -1.f, 1.f);
	return XMConvertToDegrees(acosf(cosine));
}

void StateEm8000::Change_State(STATE state) const
{
	if (const Shared<Em8000> owner = m_Owner.lock())
		owner->Change_State(state);
}

void StateEm8000::Play(ANIM anim, Bool loop, Float blend) const
{
	m_Body.lock()->Set_Animation(ETOI(anim), blend, loop);
}

Bool StateEm8000::Is_Playing(ANIM anim) const
{
	return m_Body.lock()->Get_CurrentAnimationIndex() == ETOI(anim);
}

Bool StateEm8000::Is_Finished(ANIM anim) const
{
	return Is_Playing(anim) && m_Body.lock()->Is_AnimationFinished();
}

void StateEm8000::Hold_Still() const
{
	EmMovement::EM_MOVEMENT_DATA data{};
	data.canRotation = false;
	m_Movement.lock()->Set_MovementData(data);
}

void StateEm8000::Hold_FacingTarget() const
{
	EmMovement::EM_MOVEMENT_DATA data{};
	data.lookDirection = Get_DirectionToTarget();
	m_Movement.lock()->Set_MovementData(data);
}

void StateEm8000::Turn_ToBody() const
{
	const Shared<Em8000Body> body = m_Body.lock();
	const Shared<Model> model = body->Get_ModelComponent();
	const Matrix world = body->Get_CombinedWorldMatrix();
	Vector3 facing = (model->Get_BoneMatrix(model->Get_BoneIndexByName("bone16")) * world).Translation() -
		(model->Get_BoneMatrix(model->Get_BoneIndexByName("bone3")) * world).Translation();
	facing.y = 0.f;
	if (facing.LengthSquared() < 1e-6f) return;
	facing.Normalize();
	const Shared<Transform> transform = m_Owner.lock()->Get_Transform();
	transform->Set_Rotation(Quaternion::CreateFromYawPitchRoll(atan2f(facing.x, facing.z), 0.f, 0.f));
	transform->Update_WorldMatrix();
}

void StateEm8000::Turn_ToRoot() const
{
	const Shared<Em8000Body> body = m_Body.lock();
	const Shared<Model> model = body->Get_ModelComponent();
	const int32 root = model->Get_BoneIndexByName("em8000");
	if (root < 0) return;
	// The root rotation is a yaw on top of the bind pose's tilt about X, so the root's X axis turns by just the yaw.
	const Matrix world = body->Get_CombinedWorldMatrix();
	const Vector3 now = Vector3::TransformNormal(Vector3::UnitX, model->Get_BoneMatrix(root) * world);
	const Vector3 bind = Vector3::TransformNormal(Vector3::UnitX, model->Get_BindBoneMatrix(root) * world);
	const Float turn = atan2f(now.x, now.z) - atan2f(bind.x, bind.z);
	const Shared<Transform> transform = m_Owner.lock()->Get_Transform();
	const Vector3 look = transform->Get_Look();
	transform->Set_Rotation(Quaternion::CreateFromYawPitchRoll(atan2f(look.x, look.z) + turn, 0.f, 0.f));
}

void StateEm8000::Follow_RootMotion(Bool steerToTarget) const
{
	EmMovement::EM_MOVEMENT_DATA data{};
	data.isAttack = true;
	data.canRotation = steerToTarget;
	data.direction = Get_DirectionToTarget();
	m_Movement.lock()->Set_MovementData(data);
}
