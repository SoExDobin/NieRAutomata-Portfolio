#pragma once
#include "EditorObject.h"

NS_BEGIN(Engine)
class Transform;
NS_END

NS_BEGIN(Editor)

class InspectorTransform : public EditorObject
{
public:
	InspectorTransform() = default;
	~InspectorTransform() override = default;

public:
	HRESULT Initialize() override;
	void Render(Bool isResize) override {} // Not used directly in polling anymore
	void RenderComponent(const std::shared_ptr<Engine::Transform>& transform);

public:
	static std::shared_ptr<InspectorTransform> Create();
};

NS_END
