#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "Emb001.h"

ENGINE_REFLECTION_REGISTRATION(Emb001)
{
	ReflectedTypeBuilder<Emb001>(batch, "Emb001", "Em0000")
		.Property("MaxHp", &Emb001::Get_MaxHp, &Emb001::Set_MaxHp)
		.Property("WakeDamage", &Emb001::Get_WakeDamage, &Emb001::Set_WakeDamage)
		.Property("AttackSpeed", &Emb001::Get_AttackSpeed, &Emb001::Set_AttackSpeed);
}
