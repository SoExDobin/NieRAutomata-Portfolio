#pragma once
#include "EngineManager.h"
#include "Level.h"

NS_BEGIN(Engine)

class LevelManager final : public EngineManager {
	NO_COPY(LevelManager)
public:
	LevelManager() = default;
	~LevelManager() override = default;

public:
	void Clear_LevelMembers() { m_CurrentLevel = nullptr; m_CurrentLevelIndex= {}; }
	Bool Is_LoadFinished() const
	{
		if (m_CurrentLevel)
			return m_CurrentLevel->Load_Finished();
		else
			return false;
	}
	uint32 Get_CurrentLevelIndex() const { return m_CurrentLevelIndex; }
	Shared<Level> Get_CurrentLevel() const { return m_CurrentLevel; }

public:
	HRESULT Initialize(void *arg) override;
	void On_Destroy() override;

public:
	HRESULT Change_Level(uint32 levIndex, const Shared<Level> &level);
	void Update(Float timeDelta);
	HRESULT Render();

private:
	Shared<Level> m_CurrentLevel = {nullptr};
	uint32 m_CurrentLevelIndex{};

public:
	static Unique<LevelManager> Create();
};

NS_END