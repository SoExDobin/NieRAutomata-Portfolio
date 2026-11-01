#pragma once
#include <Object.h>

NS_BEGIN(Engine)
	class Navigation;
NS_END

NS_BEGIN(Client)

class NavigationManager 
{
	DECLARE_SINGLETON(NavigationManager) 
public:
	explicit NavigationManager() = default;
	~NavigationManager();

public:
	vector<Shared<Navigation>> Get_LinkedNodeNavigations(const wstring& navTag);
	void Mapping_NavigationTable();

public:
	map<wstring, vector<Shared<Navigation>>> m_NavMapping;

};

NS_END