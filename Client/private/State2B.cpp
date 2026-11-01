#include "pch.h"
#include "State2B.h"

#include <Game.h>

#include "Pl0000.h"
#include "Pl0000Input.h"
#include "Pl0000Movement.h"
#include "Pl0000StateMachine.h"
#include "Pl0000Body.h"
#include "WP0070Body.h"
#include "WP0220Body.h"
#include "WP3000Body.h"

namespace Client
{
	State2B::State2B(const wstring& tag, const Shared<Pl0000>& owner)
		: State{ tag }, m_Owner{ owner }
	{
	}

	State2B::~State2B()
	{
		m_Owner.reset();
		m_Body.reset();
		m_States.reset();
		m_Input.reset();
		m_LightWeapon.reset();
		m_HeavyWeapon.reset();
		m_Pod.reset();
	}

	HRESULT State2B::Initialize()
	{
		GAME_INSTANCE->Add_Instance_Event(m_Owner.lock()->Get_LevelIndex(), L"State_Need_Camera", [&]()
			{
				auto camera = GAME_INSTANCE->Get_MainCamera();

				if (camera != nullptr)
				{
					m_MainCamera = camera;
				}
			}
		);
		

		m_Body = static_pointer_cast<Pl0000Body>(m_Owner.lock()->Find_Child(L"Pl0000Body"));
		m_LightWeapon = static_pointer_cast<WP0070Body>(m_Owner.lock()->Find_Child(L"WP0070Body"));
		m_HeavyWeapon = static_pointer_cast<WP0220Body>(m_Owner.lock()->Find_Child(L"WP0220Body"));
		m_Pod = static_pointer_cast<WP3000Body>(m_Owner.lock()->Find_Child(L"WP3000Body"));

		m_States = m_Owner.lock()->Get_Component<Pl0000StateMachine>();
		m_Input = m_Owner.lock()->Get_Component<Pl0000Input>();
		m_Movement = m_Owner.lock()->Get_Component<Pl0000Movement>();

		return S_OK;
	}

	Bool State2B::HasTarget() const
	{
		if (m_Owner.expired()) return false;
		return !m_Owner.lock()->Get_LockOnTarget().expired();
	}

	Vector3 State2B::Calculate_Direction()
	{
		if (m_MainCamera.expired()) return Vector3::Zero;

		Vector3 inputDir = Vector3::Zero;
		auto input = m_Input.lock();

		if (!input->Is_NoneOrUp(UBYTE(DIKEYBOARD_W))) inputDir.z += 1.f;
		if (!input->Is_NoneOrUp(UBYTE(DIKEYBOARD_S))) inputDir.z -= 1.f;
		if (!input->Is_NoneOrUp(UBYTE(DIKEYBOARD_D))) inputDir.x += 1.f;
		if (!input->Is_NoneOrUp(UBYTE(DIKEYBOARD_A))) inputDir.x -= 1.f;

		if (inputDir.LengthSquared() > 0.f)
		{
			inputDir.Normalize();
			Vector3 camLook = m_MainCamera.lock()->Get_Transform()->Get_Look();
			Vector3 camRight = m_MainCamera.lock()->Get_Transform()->Get_Right();
			camLook.y = 0.f; camLook.Normalize();
			camRight.y = 0.f; camRight.Normalize();
			Vector3 targetDir = (camLook * inputDir.z) + (camRight * inputDir.x);
			targetDir.Normalize();
			return targetDir;
		}

		return Vector3::Zero;
	}

	void State2B::Face_ComboDirection()
	{
		// Turn toward the lock-on target, otherwise toward the WASD input; with neither, keep the current facing.
		auto pl0000 = m_Owner.lock();
		const Vector3 pos = pl0000->Get_Transform()->Get_Position();
		Vector3 direction = Calculate_Direction();
		auto target = pl0000->Get_LockOnTarget();
		if (!target.expired() && !target.lock()->Is_Destroy() && target.lock()->Is_Active())
			direction = target.lock()->Get_Transform()->Get_Position() - pos;
		direction.y = 0.f;
		if (direction.LengthSquared() > 0.0001f)
			pl0000->Get_Transform()->LookAt(pos + direction);
	}
}
