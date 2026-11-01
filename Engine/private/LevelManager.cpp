#include "LevelManager.h"
#include "Game.h"
#include "Level.h"


HRESULT LevelManager::Initialize(void *arg) {
  return EngineManager::Initialize(arg);
}

void LevelManager::On_Destroy() {
    if (m_CurrentLevel) m_CurrentLevel->On_Destroy();
    m_CurrentLevel.reset();
}

HRESULT LevelManager::Change_Level(uint32 levIndex,const Shared<Level>& level) {

    // A failed level factory must not clear the current world or publish an empty level.
    if (!level)
        return E_INVALIDARG;

    if (nullptr != m_CurrentLevel)
    {
        m_CurrentLevel->On_Destroy();
        Game::GetInstance()->Clear_Resource(m_CurrentLevelIndex);
    }

    m_CurrentLevel = level;
    m_CurrentLevelIndex = levIndex;
    return S_OK;
}

void LevelManager::Update(Float timeDelta) {
  if (m_CurrentLevel == nullptr)
    return;

  m_CurrentLevel->Update_Level(timeDelta);
}

HRESULT LevelManager::Render() {
  if (m_CurrentLevel == nullptr)
    return E_FAIL;

  return m_CurrentLevel->Render_Level();
}

Unique<LevelManager> LevelManager::Create() {
  return make_unique<LevelManager>();
}
