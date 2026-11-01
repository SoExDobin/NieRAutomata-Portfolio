#include "pch.h"
#include "Client_Function.h"

#include "Game.h"
#include "Engine_ReflectionRegistration.h"

// Every Client type registers itself in reflection/<Type>_Reflection.cpp (ENGINE_REFLECTION_REGISTRATION);
// this module entry hands the collected batch to the Engine Registry once.
HRESULT Client::Register_Client_Reflection()
{
	static Bool registered = false;
	if (registered)
		return S_OK;

	ReflectionDescriptorBatch batch;
	Collect_ModuleReflection(batch);

	const HRESULT result = GAME_INSTANCE->Register_ReflectionDescriptors(batch);
	if (SUCCEEDED(result))
		registered = true;
	return result;
}