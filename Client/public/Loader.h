#pragma once
#include "Level.h"
#include "Client_Define.h"

#include <future>


NS_BEGIN(Client)

class ClientApp;

class CLIENT_DLL Loader final : public Level
{
public:
	typedef struct tagLoaderDesc
	{
		Bool isLoadStatic{ false };
		LEVEL nextLevelID{};
		Shared<Level> ownerLevel{ nullptr };
	} LOADER_DESC;
public:
	Loader(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	~Loader() override;

public:
	Bool Is_Finished() const { return m_isFinished; }

public:
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg = nullptr) override;
	void On_Destroy() override;
	
public:
	void Update_Level(Float timeDelta) override;
	HRESULT Render_Level() override;

private:
	HRESULT Loading();
	HRESULT Loading_Global_Prototype();
	HRESULT Loading_For_TitleLevel();
	HRESULT Loading_For_GamePlayLevel();
	

private:
	Bool				m_LoadStatic = { false };
	std::future<HRESULT> m_LoadResult = {}; // Loading() running on a JobManager worker
	LEVEL				m_NextLevelID = {LEVEL::LEVEL_END};
	Weak<Level>			m_OwnerLevel = {};
	tChar				m_LoadingText[MAX_PATH] = {};

	std::atomic_bool m_isFinished{ false };

public:
	static Shared<Loader> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context, LEVEL nextLevelID, const Shared<Level>& ownerLevel, Bool IsLoadStatic = false);

};

NS_END
