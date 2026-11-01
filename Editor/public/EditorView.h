#pragma once
#include "EditorObject.h"

NS_BEGIN(Editor)

class EditorView final : public EditorObject
{
public:
	EditorView();
	~EditorView() override;

public:
	HRESULT Initialize() override;
	void Update(Bool isResize) override;
	void Render(Bool isResize) override;

private:
	void RenderView(Bool isResize);
	void Render_UIEditor();
	void MousePicking(ImVec2 viewport, ImVec2 imageStartPos);
	void Update_ImGuizmo(ImVec2 viewport, ImVec2 imageStartPos);
	void Draw_ForwardAxis(const Matrix& world, const Matrix& view, const Matrix& proj,
		ImVec2 viewport, ImVec2 imageStartPos);

private:
	Bool m_PlayMode = { false };
	Bool m_WasUsingGizmo = { false };
	Bool m_UIDragging = false;
	Bool m_UIResizing = false;
	Bool m_UIBeginGesture = false;

public:
	static Shared<EditorView> Create();

};

NS_END
