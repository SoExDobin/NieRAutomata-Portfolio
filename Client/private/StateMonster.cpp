#include "pch.h"
#include "StateMonster.h"

StateMonster::StateMonster(const wstring& tag, const Shared<Monster>& owner)
	: State(tag), m_Owner{owner} {}
StateMonster::~StateMonster()
{
	m_Owner.reset();
}

HRESULT StateMonster::Initialize()
{
	return S_OK;
}

Vector3 StateMonster::Calculate_Direction()
{
	return Vector3::Zero;
}
