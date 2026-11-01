#include "pch.h"
#include "Engine_ReflectionRegistration.h"
#include "Emb001Body.h"

ENGINE_REFLECTION_REGISTRATION(Emb001Body)
{
	ReflectedTypeBuilder<Emb001Body>(batch, "Emb001Body", "Em0000Body");
}
