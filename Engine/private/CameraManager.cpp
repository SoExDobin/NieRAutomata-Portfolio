#include "CameraManager.h"
#include "Game.h"
#include "Camera.h"
#include "SpdLogger.h"
#include "Transform.h"

CameraManager::CameraManager()
	: EngineManager{} {}

HRESULT CameraManager::Initialize_Prototype()
{
	return EngineManager::Initialize_Prototype();
}

HRESULT CameraManager::Initialize(void* arg)
{
	m_LevCount = *static_cast<uint32*>(arg);
	m_Cameras.resize(m_LevCount);

	return EngineManager::Initialize(arg);
}

HRESULT CameraManager::Add_Camera(uint32 levIndex, const Shared<Camera>& camera)
{
	std::erase_if(m_Cameras[levIndex], [](const Shared<Camera>& current) {
		return !current || current->Is_Destroy();
	});

	if (!camera || !camera->Get_ObjectGuid().Is_Valid() || camera->Get_RuntimeObjectId() == 0)
	{
		LOG_ERROR(L"Fail to add camera with invalid runtime identity");
		return E_FAIL;
	}
	if (camera->Get_Name().empty())
	{
		LOG_ERROR(L"Fail to Add Camera By Empty Name");
		return E_FAIL;
	}

	for (const auto& cameraPtr : m_Cameras[levIndex])
	{
		if (cameraPtr == camera || cameraPtr->Get_ObjectGuid() == camera->Get_ObjectGuid())
		{
			LOG_ERROR(L"Already Added Camera");
			return E_FAIL;
		}
	}

	m_Cameras[levIndex].push_back(camera);

	if (m_MainCamera.expired() || m_MainCamera.lock()->Is_Destroy())
		return Set_MainCamera(camera);

	return S_OK;
}

/* A level change destroys that level's cameras (Transform released), but a caller holding a strong
   reference (Editor's in-game camera) keeps the weak main camera lockable, so destroyed counts as none. */
Shared<Camera> CameraManager::Get_MainCamera() const
{
	if (m_MainCamera.expired() || m_MainCamera.lock()->Is_Destroy())
	{
		LOG_ERROR(L"There is no MainCamera");
		return nullptr;
	}

	return m_MainCamera.lock();
}

vector<Shared<Camera>> CameraManager::Get_Cameras(uint32 levIndex)
{
	return m_Cameras[levIndex];
}

HRESULT CameraManager::Set_MainCamera(const Shared<Camera>& camera)
{
	std::erase_if(m_Cameras[GAME_INSTANCE->Get_CurrentLevelIndex()], [](auto& cam) -> Bool { return cam->Is_Destroy(); });

	if (camera == nullptr)
	{
		LOG_WARN("nullptr GameCamera");
		return E_FAIL;
	}

	m_MainCamera = camera;

	GAME_INSTANCE->Update_CameraPipeline();

	return S_OK;
}

void CameraManager::Bind_MainCamera_Transform()
{
	std::erase_if(m_Cameras[GAME_INSTANCE->Get_CurrentLevelIndex()], [](auto& cam) -> Bool { return cam->Is_Destroy(); });

	if (auto mainCamera = Get_MainCamera())
	{
		const auto& worldMatrix = mainCamera->Get_Transform()->Get_WorldMatrix();

		Matrix viewMat = worldMatrix.Invert();
		Matrix projMat = XMMatrixPerspectiveFovLH(
			mainCamera->Get_FovY(),
			mainCamera->Get_Aspect(),
			mainCamera->Get_NearPlane(),
			mainCamera->Get_FarPlane()
		);

		GAME_INSTANCE->Set_Transform(D3DTS::VIEW, viewMat);
		GAME_INSTANCE->Set_Transform(D3DTS::PROJ, projMat);
	}
}

HRESULT CameraManager::Clear_Cameras(uint32 levIndex)
{
	m_Cameras[levIndex].clear();
	m_Cameras[levIndex].shrink_to_fit();

	/* The next level's camera was added while this level's camera was still main (LevelLoading's StaticCamera),
	   so Add_Camera left it out; once this level's objects are destroyed it takes over. */
	if (m_MainCamera.expired() || m_MainCamera.lock()->Is_Destroy())
	{
		m_MainCamera.reset();
		for (const auto& cameras : m_Cameras)
			for (const auto& camera : cameras)
				if (camera && !camera->Is_Destroy())
				{
					m_MainCamera = camera;
					return S_OK;
				}
	}

	return S_OK;
}
HRESULT CameraManager::Clear_AllCameras()
{
	for (auto& camerasInLevel : m_Cameras)
	{
		camerasInLevel.clear();
		camerasInLevel.shrink_to_fit();
	}
	m_MainCamera.reset();

	return S_OK;
}

Unique<CameraManager> CameraManager::Create(uint32 levCount)
{
	auto cameraManager = make_unique<CameraManager>();

	if (FAILED(cameraManager->Initialize(&levCount)))
	{
		LOG_ERROR(L"Failed to Create CameraManager");
		return nullptr;
	}

	return cameraManager;
}
