#pragma once
#include "Level.h"

NS_BEGIN(Engine)
class TextUI;
NS_END

NS_BEGIN(Client)

class LoadingFadeIn;
class LoadingFadeOut;

class CLIENT_DLL LevelTitle final : public Level
{
public:
	LevelTitle(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	~LevelTitle() override = default;

private:
	HRESULT Initialize_Prototype() override { return S_OK; }
	HRESULT Initialize(void* arg) override;
	void On_Destroy() override;

public:
	void Update_Level(Float timeDelta) override;
	HRESULT Render_Level() override;

private:
	HRESULT Ready_TitleUI();

private:
	Bool m_IsIntroPlayed{ false };
	Shared<LoadingFadeIn>	m_FadeIn{ nullptr };
	Shared<TextUI>	m_TapPrompt{ nullptr };
	Float m_PromptTime{};
	Bool m_Covering{};
    Bool m_TransitionQueued{};

public:
	static Shared<LevelTitle> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);

};

NS_END
