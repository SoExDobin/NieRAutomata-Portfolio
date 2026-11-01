#include "pch.h"
#include "StateEm3004_Dead.h"

#include <Game.h>

#include "Em3004Body.h"
#include "EmMovement.h"
#include "ExplodeEffect.h"

StateEm3004_Dead::StateEm3004_Dead(const wstring& tag, const Shared<Em3004>& owner)
	: StateEm3004{ tag, owner } {}

Bool StateEm3004_Dead::StateEnterInvoke()
{
	EmMovement::EM_MOVEMENT_DATA stopData{};
	stopData.canRotation = false;
	m_Movement.lock()->Set_MovementData(stopData);

	ExplodeEffect::EXPLODE_EFFECT_DESC explodeDesc{};
	explodeDesc.position = m_Body.lock()->Get_Center();
	m_Body.lock()->Begin_Shatter();
	explodeDesc.size = ExplodeSize;
	GAME_INSTANCE->Instantiate<ExplodeEffect>(L"ExplodeEffect", m_Owner.lock()->Get_LevelIndex(), &explodeDesc);
	// Placeholder until the original death explosion sound is extracted (as StateEm0010_Dead).
	GAME_INSTANCE->PlaySoundFXOnce(L"Wp0220_Charge_Impact", SOUNDCHANNEL::CHANNEL_25, 0.4f);
	return true;
}

void StateEm3004_Dead::Update(Float timeDelta)
{
	// Removed once the pieces have dissolved.
	if (m_Body.lock()->Get_ShatterTime() >= Em3004Body::ShatterDuration)
		GAME_INSTANCE->Destroy(m_Owner.lock()->Get_ObjectGuid());
}

Shared<StateEm3004_Dead> StateEm3004_Dead::Create(const wstring& tag, const Shared<Em3004>& owner)
{
	auto instance = make_shared<StateEm3004_Dead>(tag, owner);

	if (FAILED(instance->Initialize()))
	{
		MSG_BOX("Failed to Created : StateEm3004_Dead");
		return nullptr;
	}

	return instance;
}
