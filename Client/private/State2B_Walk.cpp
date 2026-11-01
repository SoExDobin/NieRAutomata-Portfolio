#include "pch.h"
#include "State2B_Walk.h"
#include "Pl0000Body.h"
#include <Game.h>
#include <SpdLogger.h>

#include "Model.h"
#include "Transform.h"
#include "StateMachine.h"
#include "Camera.h"

State2B_Walk::State2B_Walk(const wstring& tag, const Shared<Pl0000>& owner)
	: State2B{tag, owner}
{
}

HRESULT State2B_Walk::Initialize()
{
	return S_OK;
}

Bool State2B_Walk::StateEnterInvoke()
{
	if (m_Owner.expired())
		return false;

	//Shared<Model> model = m_Owner.lock()->Get_Model();
	//if (model)
	//{
	//	int32 animIndex = model->Get_AnimationIndexByName(L"pl0000|pl0000_0001"); // Assuming 0001 is walk/run
	//	if (animIndex != -1)
	//	{
	//		model->Set_AnimationIndex(animIndex);
	//		model->Set_AnimLoop(true);
	//	}
	//}

	return true;
}

void State2B_Walk::Update(Float timeDelta)
{
	//Shared<Transform> transform = m_Owner.lock()->Get_Transform();
	Shared<Camera> camera = GAME_INSTANCE->Get_MainCamera();
	
	if (nullptr == camera)
		return;

	Vector3 vLook = camera->Get_Transform()->Get_Look();
	Vector3 vRight = camera->Get_Transform()->Get_Right();
	
	vLook.y = 0.f;
	vRight.y = 0.f;
	vLook.Normalize();
	vRight.Normalize();
	
	Vector3 vDir = { 0.f, 0.f, 0.f };
	Bool isInput = false;

	if (GAME_INSTANCE->Get_DIKeyState(DIK_W) & 0x80) { vDir += vLook; isInput = true; }
	if (GAME_INSTANCE->Get_DIKeyState(DIK_S) & 0x80) { vDir -= vLook; isInput = true; }
	if (GAME_INSTANCE->Get_DIKeyState(DIK_A) & 0x80) { vDir -= vRight; isInput = true; }
	if (GAME_INSTANCE->Get_DIKeyState(DIK_D) & 0x80) { vDir += vRight; isInput = true; }

	if (isInput)
	{
		vDir.Normalize();
		Float speed = 0.f; // Default speed
		//transform->Set_Position(transform->Get_Position() + vDir * speed * timeDelta);
		//transform->LookAt(transform->Get_Position() + vDir);
	}
	else
	{
		//m_Owner.lock()->Get_StateMachine()->Change_StateByTag(L"State2B_Idle");
	}
}

void State2B_Walk::Late_Update(Float timeDelta)
{
}

void State2B_Walk::StateExitInvoke()
{
}

Shared<State2B_Walk> State2B_Walk::Create(const wstring& tag, const Shared<Pl0000>& owner)
{
	auto instance = make_shared<State2B_Walk>(tag, owner);

	if (FAILED(instance->Initialize()))
	{
		LOG_ERROR(L"Failed to Create State2B_Walk");
		return nullptr;
	}

	return instance;
}

