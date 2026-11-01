#pragma once
#include "Object.h"

NS_BEGIN(Engine)

class EngineManager abstract : public Object
{
	NO_COPY(EngineManager)
public:
	EngineManager() = default;
	virtual ~EngineManager() override = default;

public:

};

NS_END
