#include "pch.h"
#include "NavigationManager.h"

#include <Game.h>
#include <Navigation.h>

#include "GameObject.h"

IMPLEMENT_SINGLETON(NavigationManager)

NavigationManager::~NavigationManager()
{
	for (auto mapping : m_NavMapping)
		mapping.second.clear();

	m_NavMapping.clear();
}

vector<Shared<Navigation>> NavigationManager::Get_LinkedNodeNavigations(const wstring& navTag)
{
	auto iter = m_NavMapping.find(navTag);
	if (iter != m_NavMapping.end())
		return iter->second;

	vector<Shared<Navigation>> empty;
	return empty;
}

void NavigationManager::Mapping_NavigationTable()
{
	// TODO : 이거 누수 코드임 DestroyInstance하셈
	//Navigation::NAVIGATION_DESC desc{  };
	//m_NavMapping[L"CityOfRuinEntry"].push_back(
	//	static_pointer_cast<Navigation>(GAME_INSTANCE->Instantiate<Navigation>(L"CityOfRuinBridge", ETOI(LEVEL::STATIC), &desc))
	//);
	//m_NavMapping[L"CityOfRuinBridge"].push_back(
	//	static_pointer_cast<Navigation>(GAME_INSTANCE->Instantiate<Navigation>(L"CityOfRuinEntry", ETOI(LEVEL::STATIC), &desc))
	//);
}
