#pragma once
#include "Level.h"

NS_BEGIN(Client)

class Loader;
class LoadingFadeIn;

class CLIENT_DLL LevelGamePlay final : public Level
{
public:
	LevelGamePlay(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context, LEVEL level);
	~LevelGamePlay() override = default;

private:
	HRESULT Initialize_Prototype() override { return S_OK; }
	HRESULT Initialize(void* arg) override;
	void On_Destroy() override;

public:
	void Update_Level(Float timeDelta) override;
	HRESULT Render_Level() override;
    Bool Request_LevelChange(LEVEL nextLevel);

private:
	// Which gameplay stage this is (GAMEPLAY, AMUSEMENT_PARK, ...); it loads LevelData_<level>.json.
	LEVEL m_Level{ LEVEL::GAMEPLAY };
    LEVEL m_NextLevel{ LEVEL::LEVEL_END };
    Shared<LoadingFadeIn> m_Fade;
    Bool m_TransitionQueued{};

public:
	static Shared<LevelGamePlay> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context, LEVEL level);

};

NS_END

