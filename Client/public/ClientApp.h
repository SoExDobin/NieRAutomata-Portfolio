#pragma once
#include "Client_Define.h"
#include "Engine_Define.h"

NS_BEGIN(Engine)
class Game;
NS_END

NS_BEGIN(Client)

class UIManager;
class CameraEventSystem;

class CLIENT_DLL ClientApp final 
{
public:
	ClientApp();
	~ClientApp();

public:
	static ClientApp* GetInstance() { return s_Instance; }
	UIManager* Get_UIManager() const { return m_UIManager.get(); }
	uint32 Get_LevelCount() const { return ETOI(LEVEL::LEVEL_END); }

public:
	HRESULT Initialize(const ENGINE_DESC& desc);
	Bool Play_CameraEvent(CAMERA_EVENT event);
	Bool Is_CameraEventPlaying() const;
	void Update_CameraEvents(LEVEL level, Float timeDelta, Bool entryReady);
	void Cancel_CameraEvent();
	void End_CameraEvents(LEVEL level);

private:
	HRESULT Ready_StartLevel(LEVEL startLevel);
	HRESULT Ready_InitialObject(); 

public:
	static Unique<ClientApp> Create(const ENGINE_DESC& desc);

private:
	static ClientApp* s_Instance; // Non-owning access to the Launcher/Editor-owned application.
	Unique<UIManager> m_UIManager;
	Unique<CameraEventSystem> m_CameraEventSystem;
};

NS_END