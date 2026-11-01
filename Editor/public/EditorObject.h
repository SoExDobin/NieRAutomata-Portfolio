#pragma once
#include "Editor_Define.h"

NS_BEGIN(Editor)

class EditorObject abstract
{
	NO_COPY(EditorObject)
public:
	EditorObject() = default;
	virtual ~EditorObject() = default;
	
public:
	Bool Is_Enabled() const { return m_Enable; }
	void Set_Enable(Bool isEnable) { m_Enable = isEnable; }
	Bool Is_Dirty() const { return m_Enable; }
	void Set_Dirty() { m_IsDirty = true; }

public:
	virtual HRESULT Initialize() { return S_OK; }
	virtual void Update(Bool isResize) {}
	virtual void Render(Bool isResize) {}
		
protected:
	Bool m_Enable = { true };
	Bool m_IsDirty = { true };

};

NS_END