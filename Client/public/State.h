#pragma once

NS_BEGIN(Engine)
	class GameObject;
NS_END

NS_BEGIN(Client)
class CLIENT_DLL State abstract
{
public:
	explicit State(const wstring& tag);
	virtual ~State() = default;

public:
	const wstring& Get_StateTag() const { return m_StateTag; }
	Bool IsSameTag(const wstring& tag) const { return tag == m_StateTag; }

public:
	Bool Is_Active() const { return m_IsActive; }

public:
	virtual Bool StateEnterInvoke() { return true; }
	virtual void Update(Float timeDelta) {}
	virtual void Late_Update(Float timeDelta) {}
	virtual void StateExitInvoke() {}

protected:
	wstring m_StateTag{};  
	Bool m_IsActive;
};

NS_END