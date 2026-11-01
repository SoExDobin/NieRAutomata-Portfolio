#include "LayerRegistry.h"
#include "TagRegistry.h"

#include "Game.h"

#include "Camera.h"
#include "GraphicDevice.h"
#include "LevelManager.h"
#include "ObjectManager.h"
#include "PrototypeManager.h"
#include "Renderer.h"
#include "TimeManager.h"

#include "Level.h"
#include "SpdLogger.h"
#include "LevelSerializer.h"
#include "PrefabManager.h"
#include "Timer.h"
#include "TypeCatalog.h"
#include "String_Helper.h"
#include <fstream>
#include <nlohmann/json.hpp>

std::shared_ptr<Game> Game::m_pInstance = nullptr;
std::once_flag Game::m_onceFlag;

std::shared_ptr<Game> Game::GetInstance() {
    std::call_once(m_onceFlag, []() {
        struct MakeSharedEnabler : public Game {};
        m_pInstance = std::make_shared<MakeSharedEnabler>();
    });
    return m_pInstance;
}

void Game::DestroyInstance() {
    const auto instance = m_pInstance;
    if (!instance) return;
    instance->Shutdown(); // Callbacks may still resolve GAME_INSTANCE here.
    m_pInstance.reset();
}

Game::Game() = default;

Game::~Game() = default;

void Game::Shutdown() {
    if (m_ShuttingDown) return;
    m_ShuttingDown = true;
    // Join level loaders before touching any resource or manager they use.
    if (m_LevelManager) m_LevelManager->On_Destroy();
    if (m_ObjectManager) m_ObjectManager->On_Destroy();
    // Loaders have finished, so no job is left that could reach the managers released below.
    if (m_JobManager) m_JobManager->On_Destroy();
    m_JobManager.reset();

	m_PrefabManager.reset();
	m_LevelSerializer.reset();

    m_TimeManager.reset();
    m_InputDevice.reset();

    if (m_CollisionManager) m_CollisionManager->On_Destroy();
    m_CollisionManager.reset();

    if (m_PhysicsManager) m_PhysicsManager->On_Destroy();
    m_PhysicsManager.reset();
#ifdef _DEBUG
    m_DebugRenderer.reset();
#endif

    if (m_ResourceManager) m_ResourceManager->On_Destroy();
    m_ResourceManager.reset();

    m_LevelManager.reset();

    m_ObjectManager.reset();

    if (m_Renderer) m_Renderer->On_Destroy();
    m_Renderer.reset();

    if (m_PrototypeManager) m_PrototypeManager->On_Destroy();
    m_PrototypeManager.reset();

	if (m_Registry)
		m_Registry->On_Destroy();
	m_Registry.reset();

    if (m_CameraManager) m_CameraManager->On_Destroy();
    m_CameraManager.reset();

    m_LayerRegistry.reset();
    m_TagRegistry.reset();

    if (m_LightManager) m_LightManager->On_Destroy();
    m_LightManager.reset();

    m_FontManager.reset();
    m_SoundManager.reset();
    m_EventManager.reset();
    m_RenderTargetManager.reset();
    m_NavigationBuilder.reset();
    m_Pipeline.reset();
    m_GraphicDevice.reset();

    LOG_SHUTDOWN(); /* SpdLogger */
}

HRESULT Game::Load_EngineDesc(const wstring& filePath, ENGINE_DESC& inOutDesc) const
{
    std::ifstream file(filePath);
    if (!file.is_open())
    {
        LOG_WARN(L"Failed To Find Path : {}", filePath);
        return E_FAIL;
    }

    const nlohmann::json json = nlohmann::json::parse(file, nullptr, false);
    if (json.is_discarded())
    {
        LOG_WARN(L"Failed To Parse Engine Desc : {}", filePath);
        return E_FAIL;
    }

    inOutDesc.startLevel = json.value("startLevel", inOutDesc.startLevel);
    inOutDesc.viewportWidth = json.value("viewportWidth", inOutDesc.viewportWidth);
    inOutDesc.viewportHeight = json.value("viewportHeight", inOutDesc.viewportHeight);
    if (json.contains("windowTitle"))
        inOutDesc.windowTitle = Helper::To_wString(json.value("windowTitle", string{}));
    return S_OK;
}

HRESULT Game::Load_LayerAndTagSettings(const wstring& projectSettingPath) const
{
    if (m_LayerRegistry)
        m_LayerRegistry->LoadFromFile(projectSettingPath + L"LayerSettings.json");
    if (m_TagRegistry)
        m_TagRegistry->LoadFromFile(projectSettingPath + L"TagSettings.json");
    return S_OK;
}

HRESULT Game::Initialize_Engine(const ENGINE_DESC &engineDesc) {
    /* SpdLogger - 먼저 초기화. Release는 디버그 공간인 Editor처럼 요청한 호스트만 켠다. */
#ifdef _DEBUG
    LOG_INIT();
#else
    if (engineDesc.useLog)
        LOG_INIT();
#endif

    m_LayerRegistry = make_shared<LayerRegistry>();
    m_TagRegistry = make_shared<TagRegistry>();
	m_Registry = make_unique<Registry>();
	if (FAILED(m_Registry->Initialize(nullptr)))
		return E_FAIL;

    if (nullptr == (m_GraphicDevice = GraphicDevice::Create(engineDesc)))
		return E_FAIL;
#ifdef _DEBUG
    if (nullptr == (m_DebugRenderer = DebugRenderer::Create(m_GraphicDevice->Get_Device(), m_GraphicDevice->Get_Context())))
		return E_FAIL;
#endif

    if (nullptr == (m_TimeManager = TimeManager::Create()))
		return E_FAIL;

    if (nullptr == (m_JobManager = JobManager::Create()))
		return E_FAIL;

    if (nullptr == (m_InputDevice = InputDevice::Create(engineDesc.hWnd, engineDesc.hInst)))
		return E_FAIL;

    if (nullptr == (m_Pipeline = Pipeline::Create()))
		return E_FAIL;

    if (nullptr == (m_LevelManager = LevelManager::Create()))
		return E_FAIL;

    if (nullptr == (m_PrototypeManager = PrototypeManager::Create(engineDesc.levelCount)))
		return E_FAIL;
	if (FAILED(Register_EngineComponentPrototypes()))
		return E_FAIL;

    if (nullptr == (m_ObjectManager = ObjectManager::Create(engineDesc.levelCount)))
		return E_FAIL;

    if (nullptr == (m_ResourceManager = ResourceManager::Create(m_GraphicDevice->Get_Device(), m_GraphicDevice->Get_Context(), engineDesc.levelCount)))
        return E_FAIL;

    if (nullptr == (m_CameraManager = CameraManager::Create(engineDesc.levelCount)))
		return E_FAIL;

    if (nullptr == (m_RenderTargetManager = RenderTargetManager::Create(m_GraphicDevice->Get_Device(), m_GraphicDevice->Get_Context())))
        return E_FAIL;

    if (nullptr == (m_Renderer = Renderer::Create(m_GraphicDevice->Get_Device(),m_GraphicDevice->Get_Context())))
		return E_FAIL;

    if (nullptr == (m_LightManager = LightManager::Create()))
		return E_FAIL;

    if (nullptr == (m_LevelSerializer = LevelSerializer::Create()))
        return E_FAIL;
	m_PrefabManager = make_unique<PrefabManager>();

    if (nullptr == (m_FontManager = FontManager::Create(m_GraphicDevice->Get_Device(), m_GraphicDevice->Get_Context())))
        return E_FAIL;

    if (nullptr == (m_SoundManager = SoundManager::Create()))
        return E_FAIL;

    if (nullptr == (m_EventManager = EventManager::Create(engineDesc.levelCount)))
        return E_FAIL;

    if (nullptr == (m_CollisionManager = CollisionManager::Create()))
        return E_FAIL;

    if (nullptr == (m_PhysicsManager = PhysicsManager::Create()))
        return E_FAIL;

    if (nullptr == (m_NavigationBuilder = NavigationBuilder::Create()))
        return E_FAIL;


    return S_OK;
}

Float Game::Begin_Frame(Bool accumulateFixedTime) {
    const Float delta = m_TimeManager->Update_Timers(accumulateFixedTime);
    m_InputDevice->Update();
    return delta;
}

void Game::Update_Engine() {
    Update_Engine(Begin_Frame(true), false);
}

void Game::Update_Engine(Float timeDelta, Bool singleFixedStep) {
    const Float delta = singleFixedStep ? Get_FixedDeltaTime() : timeDelta;
    ++m_RuntimeFrameCount;
    m_LastRuntimeFixedStepCount = 0;
    m_LastRuntimeDelta = delta;

    m_ObjectManager->PriorityUpdate(delta);

    m_ObjectManager->Update(delta);

    m_ObjectManager->LateUpdate(delta);

    m_CameraManager->Bind_MainCamera_Transform();
    m_Pipeline->Update_Pipeline();
    m_SoundManager->Update_Listener(m_Pipeline->Get_InvTransform(D3DTS::VIEW));


    if (singleFixedStep) {
        m_ObjectManager->FixedUpdate(Get_FixedDeltaTime());
        m_PhysicsManager->Step(Get_FixedDeltaTime());
        ++m_LastRuntimeFixedStepCount;
    }
    else while (m_TimeManager->Has_FixedUpdate()) {
        Float fixedDelta = m_TimeManager->Get_MainTimer()->GetFixedDeltaTime();
        m_ObjectManager->FixedUpdate(fixedDelta);
        // 오브젝트가 이번 스텝의 변위를 정한 뒤에 물리를 돌린다.
        m_PhysicsManager->Step(fixedDelta);
        ++m_LastRuntimeFixedStepCount;
        m_TimeManager->Get_MainTimer()->ConsumeFixedDeltaTime();
    }

    m_ObjectManager->Submit_RenderGroup();

    Flush_DestroyedGameObjects();

    m_CollisionManager->Update_Collision(*m_PhysicsManager);

    m_LevelManager->Update(delta);
    m_EventManager->Execute_Events();
}

HRESULT Game::Draw() const {
    const HRESULT result = Draw_NoClearing();
    m_Renderer->Clear_RenderGroup();
    return result;
}
HRESULT Game::Clear_RenderGroup() const {
    return m_Renderer->Clear_RenderGroup();
}
HRESULT Game::Draw_NoClearing() const {
    D3D11_VIEWPORT viewport{};
    UINT count = 1;
    m_GraphicDevice->Get_Context()->RSGetViewports(&count, &viewport);
    const uint32 screenIndex = m_GraphicDevice->Get_ActiveScreen();
    if (FAILED(m_RenderTargetManager->Prepare_View(screenIndex,
        static_cast<uint32>(viewport.Width), static_cast<uint32>(viewport.Height))))
        return E_FAIL;
    m_Renderer->Draw_NoClearing(screenIndex);
    return S_OK;
}

void Game::Update_CameraPipeline()
{
    m_CameraManager->Bind_MainCamera_Transform();
    m_Pipeline->Update_Pipeline();
}

void Game::Clear_AllResource() const {
    m_LevelManager->On_Destroy();
    m_PrototypeManager->Clear_Prototypes();
    m_ObjectManager->Clear_AllGameObjects();
    m_Renderer->Clear_RenderGroup();
    m_CameraManager->Clear_AllCameras();
    m_LightManager->Clear_Lights();
    m_ResourceManager->Clear_AllResources();
    m_LevelManager->Clear_LevelMembers();

    m_FontManager->Clear_Fonts();
    m_SoundManager->Clear_SoundSources();
    m_EventManager->Clear_AllEvents();
}

void Game::Clear_Resource(uint32 levIndex) const {

	if (FAILED(m_PrototypeManager->Clear_Prototypes(levIndex)))
		LOG_CRITICAL(L"Failed To Clear Level{} Prototypes", levIndex);
  
    if (FAILED(m_ObjectManager->Clear_GameObjects(levIndex)))
        LOG_CRITICAL(L"Failed To Clear Level{} GameObjects", levIndex);

	if (FAILED(m_ResourceManager->Clear_Resource(levIndex)))
		LOG_CRITICAL(L"Failed To Clear Resources");

	if (FAILED(m_Renderer->Clear_RenderGroup()))
      LOG_CRITICAL(L"Failed To Clear RenderGroup");

	if (FAILED(m_CameraManager->Clear_Cameras(levIndex)))
      LOG_CRITICAL(L"Failed To Clear Cameras");

	/* Lights are not cleared per level: each belongs to a scene object and leaves with it.
	   Change_Level clears the previous level only after the next level's scene has loaded,
	   so a global clear here would drop the new level's lights. */
    if (FAILED(m_EventManager->Clear_Events(levIndex)))
        LOG_CRITICAL(L"Failed To Clear Events");

}

void Game::Update_Input() const { m_InputDevice->Update(); }

HRESULT Game::Clear_BackBufferView(const Shared<Float4> &clearColor) const {
  if (FAILED(m_GraphicDevice->Clear_BackBufferView(clearColor)))
    return E_FAIL;
  if (FAILED(m_GraphicDevice->Clear_DepthStencilView()))
    return E_FAIL;

  return S_OK;
}

HRESULT Game::Present() const { return m_GraphicDevice->Present(); }

HRESULT Game::OnResize(uint32 width, uint32 height, uint32 offscreenIndex)
{
	if (nullptr == m_GraphicDevice)
	{
		return S_OK;
	}

	if (offscreenIndex != UINT_MAX && width > 0 && height > 0)
	{
		if (!m_RenderTargetManager) return E_FAIL;
		GraphicDevice::OffscreenRenderTarget replacement{};
		const HRESULT prepared = m_GraphicDevice->Prepare_OffscreenResize(width, height, offscreenIndex, replacement);
		if (FAILED(prepared)) return prepared;
		// MRT creation is atomic on failure; offscreen commit below cannot fail.
		const HRESULT resized = m_RenderTargetManager->Resize_View(offscreenIndex, width, height);
		if (FAILED(resized)) return resized;
		if (prepared == S_OK)
			m_GraphicDevice->Commit_OffscreenResize(offscreenIndex, std::move(replacement));
		return S_OK;
	}
	const HRESULT result = m_GraphicDevice->OnResize(width, height, offscreenIndex);
	if (SUCCEEDED(result) && offscreenIndex == UINT_MAX && width > 0 && height > 0 &&
		m_CameraManager && m_CameraManager->Get_MainCamera())
		m_CameraManager->Get_MainCamera()->Bind_Aspect(static_cast<Float>(width) / height);
	return result;
}

HRESULT Game::Begin_RenderOffScreen(uint32 screenIndex) const {
  return m_GraphicDevice->Begin_RenderOffScreen(screenIndex);
}
HRESULT Game::End_RenderOffScreen() const {
  return m_GraphicDevice->End_RenderOffScreen();
}
ComPtr<ID3D11ShaderResourceView>
Game::Get_OffScreenSRV(uint32 screenIndex) const {
  return m_GraphicDevice->Get_OffscreenSRV(screenIndex);
}

void Game::Set_DepthStencilState(ID3D11DepthStencilState* state, UINT ref) const
{
    m_GraphicDevice->Set_DepthStencilState(state, ref);
}

Byte Game::Get_DIKeyState(uByte byKeyID) const {
  return m_InputDevice->Get_DIKeyState(byKeyID);
}

Byte Game::Get_DIMouseState(DIMB mouseInput) const {
  return m_InputDevice->Get_DIMouseState(mouseInput);
}

Long Game::Get_DIMouseMove(DIMM mouseState) const {
  return m_InputDevice->Get_DIMouseMove(mouseState);
}

HRESULT Game::Add_Timer(const wstring &timerTag) const {
  if (FAILED(m_TimeManager->Add_Timer(timerTag))) {
    MSG_BOX("Failed To Add Timer");
    return E_FAIL;
  }

  return S_OK;
}

void Game::Set_TimeScale(Float timeScale) const {
  return m_TimeManager->Get_MainTimer()->SetTimeScale(timeScale);
}

Float Game::Get_FPS() const { return m_TimeManager->Get_MainTimer()->GetFPS(); }
Float Game::Get_TotalTime() const { return m_TimeManager->Get_MainTimer()->GetTotalTime(); }
Float Game::Compute_TimeDelta() const {
  return m_TimeManager->Get_MainTimer()->GetDeltaTime();
}
Float Game::Compute_UnscaledTimeDelta() const {
  return m_TimeManager->Get_MainTimer()->GetUnscaledDeltaTime();
}
#ifdef _DEBUG
void Game::Fail_NextViewResize_Debug(Bool offscreen) const
{
    m_GraphicDevice->m_FailNextOffscreenCreation = offscreen;
    m_RenderTargetManager->m_FailNextViewCreation = !offscreen;
}
#endif
Float Game::Get_FixedDeltaTime() const {
  return m_TimeManager->Get_MainTimer()->GetFixedDeltaTime();
}
Float Game::Compute_TimeDelta(const wstring &timerTag) const {
  return m_TimeManager->Get_Timer(timerTag)->GetDeltaTime();
}

Shared<Level> Game::Get_CurrentLevel()
{
    return m_LevelManager->Get_CurrentLevel();
}

void Game::Update_Level() const
{
    m_LevelManager->Update(0.f);
}

HRESULT Game::Change_Level(uint32 levIndex, const Shared<Level>& newLevel) {
  const HRESULT result = m_LevelManager->Change_Level(levIndex, newLevel);
  if (FAILED(result))
    LOG_ERROR(L"Failed to change to level {} (HRESULT {}); current level preserved",
        levIndex, static_cast<unsigned long>(result));
  else
    LOG_INFO(L"[Level] Entered {}", levIndex);
  return result;
}

const unordered_map<wstring, Shared<GameObject>>& Game::Get_Prototypes(uint32 levIndex) const
{
    return m_PrototypeManager->Get_GameObjects()[levIndex];
}

HRESULT Game::Add_GameObject(const Shared<GameObject> &gameObject, uint32 levIndex) const {
  if (FAILED(m_ObjectManager->Add_GameObject(levIndex, gameObject))) {
    LOG_ERROR(L"Failed To Add GameObject");
    return E_FAIL;
  }

  return S_OK;
}

void Game::Clearing_ObjectManager(uint32 levIndex) const
{
    m_ObjectManager->Cleanup_GameObjects(levIndex);
}

Shared<GameObject> Game::Find_ObjectByObjectTag(uint32 levIndex, const wstring& tag) const
{
    return m_ObjectManager->Find_ObjectByObjectTag(levIndex, tag);
}

Shared<GameObject> Game::Find(ObjectGuid objectGuid) const
{
    return m_ObjectManager->Find_ByObjectGuid(objectGuid);
}

Shared<GameObject> Game::Find(RuntimeObjectId runtimeObjectId) const
{
    return m_ObjectManager->Find_ByRuntimeObjectId(runtimeObjectId);
}

Bool Game::Contains(uint32 levIndex, ObjectGuid objectGuid) const
{
    return m_ObjectManager->Contains(levIndex, objectGuid);
}

HRESULT Game::Find_Level(ObjectGuid objectGuid, uint32& outLevelIndex) const
{
	return m_ObjectManager->Find_Level(objectGuid, outLevelIndex);
}

HRESULT Game::Destroy(ObjectGuid objectGuid) const
{
    return m_ObjectManager->Destroy(objectGuid);
}

void Game::Flush_DestroyedGameObjects() const
{
    m_ObjectManager->Cleanup_GameObjects(0);
    const uint32 currentLevelIndex = Get_CurrentLevelIndex();
    if (currentLevelIndex != 0)
        m_ObjectManager->Cleanup_GameObjects(currentLevelIndex);
}

HRESULT Game::Clear_GameObjects(uint32 levIndex) const
{
	return m_ObjectManager->Clear_GameObjects(levIndex);
}

vector<Shared<GameObject>> Game::FindAll_Internal(RuntimeTypeId runtimeTypeId) const
{
    return m_ObjectManager->Find_AllByRuntimeTypeId(runtimeTypeId);
}

void Game::Submit_RenderGroup() const { m_ObjectManager->Submit_RenderGroup(); }

const unordered_map<RuntimeObjectId, Shared<GameObject>>& Game::Get_GameObjects(uint32 levIndex) const {
    return m_ObjectManager->Get_GameObjects(levIndex);
}

HRESULT Game::Clear_AllGameObjects() const
{
	if (FAILED(m_ObjectManager->Clear_AllGameObjects()))
	{
		LOG_ERROR(L"[Game] Failed To Clear All GameObjects");
		return E_FAIL;
	}
	if (FAILED(m_Renderer->Clear_RenderGroup()))
	{
		LOG_ERROR(L"[Game] Failed To Clear RenderGroup");
		return E_FAIL;
	}
	m_CameraManager->Clear_AllCameras();
	return S_OK;
}

HRESULT Game::Add_Camera(uint32 levIndex, const Shared<Camera> &camera) const
{
	return m_CameraManager->Add_Camera(levIndex, camera);
}
vector<Shared<Camera>> Game::Get_Cameras(uint32 levIndex) const
{
    return m_CameraManager->Get_Cameras(levIndex);
}
HRESULT Game::Set_MainCamera(const Shared<Camera> &camera) const
{
	return m_CameraManager->Set_MainCamera(camera);
}
Shared<Camera> Game::Get_MainCamera() const { return m_CameraManager->Get_MainCamera(); }

std::future<HRESULT> Game::Submit_Job(std::function<HRESULT()> job) const
{
    return m_JobManager->Submit(std::move(job));
}

HRESULT Game::Parallel_For(uint32 count, const std::function<HRESULT(uint32)>& job) const
{
    return m_JobManager->Parallel_For(count, job);
}

HRESULT Game::Load_Shader(uint32 levIndex, const tChar* shaderFilePath, const D3D11_INPUT_ELEMENT_DESC* elements, uint32 numElements, const wstring& descriptionTag) const
{
    return m_ResourceManager->Load_Shader(levIndex, shaderFilePath, elements, numElements, descriptionTag);
}

HRESULT Game::Load_Shader(uint32 levIndex, const tChar* shaderFilePath) const
{
    return m_ResourceManager->Load_Shader(levIndex, shaderFilePath);
}

Shared<Shader> Game::Get_Shader(uint32 levIndex, const tChar* shaderFilePath) const
{
    return m_ResourceManager->Get_Shader(levIndex, shaderFilePath);
}

vector<wstring> Game::Get_ShaderTags(uint32 levIndex) const
{
    return m_ResourceManager->Get_ShaderTags(levIndex);
}

HRESULT Game::Load_Texture(uint32 levIndex, const tChar* textureFilePath, uint32 numSRVs, const wstring& descriptionTag, Bool allowMissing) const
{
    return m_ResourceManager->Load_Texture(levIndex, textureFilePath, numSRVs, descriptionTag, allowMissing);
}

const Texture::TEXTURE_DESC* Game::Get_TextureDesc(uint32 levIndex, const wstring& descriptionTag) const
{
    return m_ResourceManager->Get_TextureDescByTag(levIndex, descriptionTag);
}

vector<wstring> Game::Get_TextureTags(uint32 levIndex) const
{
    return m_ResourceManager->Get_TextureTags(levIndex);
}

const ComPtr<ID3D11ShaderResourceView>& Game::Get_Texture(uint32 levIndex, const tChar* textureFilePath) const
{
    return m_ResourceManager->Get_Texture(levIndex, textureFilePath);
}

HRESULT Game::Load_Model(uint32 levIndex, const tChar* modelFilePath, const wstring& descriptionTag,
	const Matrix& preTransformMatrix, const tChar* materialSettingsPath, const wstring& replacedTag) const
{
    return m_ResourceManager->Load_Model(levIndex, modelFilePath, descriptionTag,
		preTransformMatrix, materialSettingsPath, replacedTag);
}

HRESULT Game::Load_ModelAnimations(uint32 levIndex, const wstring& modelTag,
	const vector<wstring>& animationFilePaths) const
{
	return m_ResourceManager->Load_ModelAnimations(levIndex, modelTag, animationFilePaths);
}

Shared<Model> Game::Get_Model(uint32 levIndex, const tChar* modelFilePath) const
{
    return m_ResourceManager->Get_Model(levIndex, modelFilePath);
}

int32 Game::Get_ContainLevelByModelTag(const wstring& modelTag) const
{
    return m_ResourceManager->Get_ContainLevelByModelTag(modelTag);
}

vector<Shared<Model>> Game::Get_Models(uint32 levIndex) const
{
    return m_ResourceManager->Get_Models(levIndex);
}

void Game::Add_RenderGroup(RENDERGROUP group, const Shared<GameObject> &gameObject) const {
	m_Renderer->Add_RenderGroup(group, gameObject);
}

Bool Game::Toggle_FrustumCulling() const { return m_Renderer->Toggle_FrustumCulling(); }

Bool Game::Get_FrustumCulling() const { return m_Renderer->Get_FrustumCulling(); }

Bool Game::Toggle_WorldLod() const { return m_Renderer->Toggle_WorldLod(); }

Bool Game::Get_WorldLod() const { return m_Renderer->Get_WorldLod(); }

Bool Game::Toggle_Shadow() const { return m_Renderer->Toggle_Shadow(); }

Bool Game::Get_Shadow() const { return m_Renderer->Get_Shadow(); }

Bool Game::Toggle_SSAO() const { return m_Renderer->Toggle_SSAO(); }

Bool Game::Get_SSAO() const { return m_Renderer->Get_SSAO(); }

Vector3 Game::Get_SSAOParams() const { return m_Renderer->Get_SSAOParams(); }

void Game::Set_SSAOParams(const Vector3& params) const { m_Renderer->Set_SSAOParams(params); }

void Game::Set_ColorGrade(const Vector3& grade, const Color& tint) const { m_Renderer->Set_ColorGrade(grade, tint); }

void Game::Set_Fog(const Color& color, Float distance, Float opacity, const Color& sunColor) const
{
    m_Renderer->Set_Fog(color, distance, opacity, sunColor);
}

void Game::Set_Bloom(Float intensity) const { m_Renderer->Set_Bloom(intensity); }

Bool Game::Toggle_Fog() const { return m_Renderer->Toggle_Fog(); }

Bool Game::Get_Fog() const { return m_Renderer->Get_Fog(); }

Bool Game::Toggle_Bloom() const { return m_Renderer->Toggle_Bloom(); }

Bool Game::Get_Bloom() const { return m_Renderer->Get_Bloom(); }

Bool Game::Toggle_ColorGrade() const { return m_Renderer->Toggle_ColorGrade(); }

Bool Game::Get_ColorGrade() const { return m_Renderer->Get_ColorGrade(); }

void Game::Start_ScreenGlitch(Float strength, Float duration, Bool bands) const
{
    m_Renderer->Start_ScreenGlitch(strength, duration, bands);
}

Bool Game::Toggle_Volumetric() const { return m_Renderer->Toggle_Volumetric(); }

Bool Game::Get_Volumetric() const { return m_Renderer->Get_Volumetric(); }

Vector3 Game::Get_VolumetricParams() const { return m_Renderer->Get_VolumetricParams(); }

void Game::Set_VolumetricParams(const Vector3& params) const { m_Renderer->Set_VolumetricParams(params); }

Bool Game::Toggle_CascadeDebug() const { return m_Renderer->Toggle_CascadeDebug(); }

Bool Game::Get_CascadeDebug() const { return m_Renderer->Get_CascadeDebug(); }

Bool Game::Toggle_Instancing() const { return m_Renderer->Toggle_Instancing(); }

Bool Game::Get_Instancing() const { return m_Renderer->Get_Instancing(); }

Bool Game::Toggle_Occlusion() const { return m_Renderer->Toggle_Occlusion(); }

Bool Game::Get_Occlusion() const { return m_Renderer->Get_Occlusion(); }

Bool Game::Is_Occluded(const BoundingBox& worldBounds) const { return m_Renderer->Is_Occluded(worldBounds); }

Matrix Game::Get_OcclusionViewProj() const { return m_Renderer->Get_OcclusionViewProj(); }

Bool Game::Is_ShadowPass() const { return m_Renderer->Is_ShadowPass(); }

RENDERGROUP Game::Get_DrawingGroup() const { return m_Renderer->Get_DrawingGroup(); }

HRESULT Game::Bind_ForwardLighting(const Shared<Shader>& shader) const { return m_Renderer->Bind_ForwardLighting(shader); }

uint32 Game::Get_ShadowCascade() const { return m_Renderer->Get_ShadowCascade(); }

Float Game::Get_ShadowTexelSize() const { return m_Renderer->Get_ShadowTexelSize(); }

HRESULT Game::Bind_CameraPosition(const Shared<Shader> &shader,
                                  const Char *constantName) const {
	return m_Pipeline->Bind_CameraPosition(shader, constantName);
}

HRESULT Game::Bind_TransformMatrix(const Shared<Shader> &shader,
                                   const Char *constantName,
                                   D3DTS transformState) {
	return m_Pipeline->Bind_TransformMatrix(shader, constantName, transformState);
}

HRESULT Game::Bind_TransformMatrix_Inverse(const Shared<Shader> &shader,
                                           const Char *constantName,
                                           D3DTS transformState) {
	return m_Pipeline->Bind_TransformMatrix_Inverse(shader, constantName, transformState);
}

Matrix Game::Get_Transform(D3DTS transformState) const {
	return m_Pipeline->Get_Transform(transformState);
}

Matrix Game::Get_InvTransform(D3DTS transformState) const {
    return m_Pipeline->Get_InvTransform(transformState);
}

Vector4 Game::Get_CamTransform() const {
	return m_Pipeline->Get_CamTransform();
}

void Game::Set_Transform(D3DTS transformState, Matrix transformStateMatrix) {
	m_Pipeline->Set_Transform(transformState, transformStateMatrix);
}

void Game::Update_Pipeline() const { m_Pipeline->Update_Pipeline(); }

// 시야 볼륨 안이어도 지난 깊이에 완전히 가렸으면 보이지 않는 것으로 본다(화면 패스만, Renderer가 판정).
Bool Game::Is_Visible(const BoundingBox& worldBounds) const
{
    return m_Pipeline->Is_Visible(worldBounds) && !m_Renderer->Is_Occluded(worldBounds);
}

const LIGHT_DESC *Game::Get_LightDesc(uint32 index) const {
  return m_LightManager->Get_LightDesc(index);
}

const LIGHT_DESC *Game::Find_ActiveLight(LIGHT type) const {
  return m_LightManager->Find_ActiveLight(type);
}
HRESULT Game::Add_Light(const Shared<Light>& light) const {
  return m_LightManager->Add_Light(light);
}
HRESULT Game::Remove_Light(const Shared<Light>& light) const {
  return m_LightManager->Remove_Light(light);
}
HRESULT Game::Render_Lights(const Shared<class Shader>& shader, const Shared<class VIBuffer_Rect>& buffer) const
{
    return m_LightManager->Render_Lights(shader, buffer);
}

HRESULT Game::Refresh_ReflectionRegistry() const
{
    return m_Registry ? m_Registry->Refresh() : E_FAIL;
}

HRESULT Game::Register_ReflectionDescriptors(const ReflectionDescriptorBatch& descriptors) const
{
	return m_Registry ? m_Registry->Register_Descriptors(descriptors) : E_FAIL;
}

HRESULT Game::Register_ReflectedPrototypes() const
{
    if (!m_Registry || !m_PrototypeManager)
        return E_FAIL;
	constexpr uint32 staticPrototypeLevel = 0;

    for (const Registry::Entry& entry : m_Registry->Get_ObjectTypes()) {
		const Bool isGameObject = entry.typeInfo.objectKind == REFLECTED_OBJECT_KIND::GAMEOBJECT;
		const Bool isComponent = entry.typeInfo.objectKind == REFLECTED_OBJECT_KIND::COMPONENT;
        if (!isGameObject && !isComponent)
            continue;

		Shared<Object> prototype;
		const HRESULT externalCreate = m_Registry->Create_Prototype(entry.registeredName, prototype);
		if (FAILED(externalCreate)) {
			LOG_ERROR(L"Failed to invoke external Create for {}",
				Helper::To_wString(entry.registeredName));
			return E_FAIL;
		}
		if (externalCreate == S_FALSE)
			continue;

		const HRESULT addResult = prototype
			? Add_TypePrototype_Internal(staticPrototypeLevel, prototype)
			: E_FAIL;
        if (FAILED(addResult)) {
            LOG_ERROR(L"Failed to auto-register reflected prototype {}",
                      Helper::To_wString(entry.registeredName));
            return E_FAIL;
        }
    }

    return S_OK;
}

HRESULT Game::Register_EngineComponentPrototypes() const
{
	constexpr uint32 staticPrototypeLevel = 0;
	for (const Registry::Entry& entry : m_Registry->Get_ObjectTypes()) {
		if (entry.typeInfo.objectKind != REFLECTED_OBJECT_KIND::COMPONENT || entry.externalFactory)
			continue;

		Shared<Object> prototype;
		const HRESULT createResult = m_Registry->Create_EngineComponentPrototype(entry.registeredName, prototype);
		if (FAILED(createResult)) {
			LOG_ERROR(L"[Reflection] : Failed to Create Prototype {}", Helper::To_wString(entry.registeredName));
			return E_FAIL;
		}
		if (createResult == S_FALSE) {
			LOG_WARN(L"[Reflection] : {} doesn't have \"CreatePrototype\"", Helper::To_wString(entry.registeredName));
			continue;
		}
		if (FAILED(Add_TypePrototype_Internal(staticPrototypeLevel, prototype)))
			continue;
		LOG_INFO(L"Auto-Registered Prototype: {}", Helper::To_wString(entry.registeredName));
	}
	return S_OK;
}

string Game::Find_RegisteredName(RuntimeTypeId runtimeTypeId) const
{
    return m_Registry ? m_Registry->Find_RegisteredName(runtimeTypeId) : string{};
}

HRESULT Game::Find_ReflectedIdentity(const Object& object, RuntimeTypeId& outRuntimeTypeId,
	string& outRegisteredName, Shared<const vector<RuntimeTypeId>>& outTypeAncestors) const
{
	outRuntimeTypeId = {};
	outRegisteredName.clear();
	outTypeAncestors.reset();
	return m_Registry
		? m_Registry->Find_Identity(object, outRuntimeTypeId, outRegisteredName, outTypeAncestors)
		: E_FAIL;
}

RuntimeTypeId Game::Find_RuntimeTypeId(std::string_view registeredName) const
{
    return m_Registry ? m_Registry->Find_RuntimeTypeId(registeredName) : RuntimeTypeId{};
}

HRESULT Game::Find_ReflectedType(std::string_view registeredName,
    ReflectedTypeInfo& outInfo) const
{
    return m_Registry ? m_Registry->Find_TypeInfo(registeredName, outInfo) : E_FAIL;
}

HRESULT Game::Find_ReflectedType(RuntimeTypeId runtimeTypeId,
    ReflectedTypeInfo& outInfo) const
{
    return m_Registry ? m_Registry->Find_TypeInfo(runtimeTypeId, outInfo) : E_FAIL;
}

vector<ReflectedTypeInfo> Game::Get_ReflectedTypes() const
{
    return m_Registry ? m_Registry->Get_Types() : vector<ReflectedTypeInfo>{};
}

vector<ReflectedPropertyInfo> Game::Get_ReflectedProperties(
    std::string_view registeredName) const
{
    return m_Registry
        ? m_Registry->Get_Properties(registeredName)
        : vector<ReflectedPropertyInfo>{};
}

vector<ReflectedPropertyInfo> Game::Get_ReflectedProperties(const Object& target) const
{
    return m_Registry
        ? m_Registry->Get_Properties(target.Get_RuntimeTypeId())
        : vector<ReflectedPropertyInfo>{};
}

HRESULT Game::Find_ReflectedEnum(std::string_view registeredName,
    ReflectedEnumInfo& outInfo) const
{
    return m_Registry ? m_Registry->Find_Enum(registeredName, outInfo) : E_FAIL;
}

vector<ReflectedEnumInfo> Game::Get_ReflectedEnums(std::string_view category) const
{
    return m_Registry ? m_Registry->Get_Enums(category) : vector<ReflectedEnumInfo>{};
}

HRESULT Game::Read_ReflectedProperty(Object& target, std::string_view propertyName,
    ReflectionValue& outValue) const
{
    return m_Registry
        ? m_Registry->Read_Property(target, propertyName, outValue)
        : E_FAIL;
}

HRESULT Game::Write_ReflectedProperty(Object& target, std::string_view propertyName,
    const ReflectionValue& value) const
{
    if (!m_Registry)
        return E_FAIL;

    // A reference must name an object of the property's ReferenceBase in the referrer's level.
    // A GUID that resolves to nothing stays as a missing reference (deleted target, Undo restores it).
    if (const ObjectGuid* objectGuid = value.Try_Get<ObjectGuid>(); objectGuid && objectGuid->Is_Valid()) {
        if (const Shared<GameObject> referenced = m_ObjectManager->Find_ByObjectGuid(*objectGuid)) {
            const vector<ReflectedPropertyInfo> properties = m_Registry->Get_Properties(target.Get_RuntimeTypeId());
            const auto propertyIt = ranges::find_if(properties, [&](const ReflectedPropertyInfo& property) {
                return property.registeredName == propertyName;
            });
            const string expectedBase = propertyIt == properties.end() ? string{} : propertyIt->expectedBaseRegisteredName;
            if (!m_Registry->Is_A(referenced->Get_RuntimeTypeId(), expectedBase)) {
                LOG_WARN(L"{} cannot reference {}: it is not a {}", Helper::To_wString(string{ propertyName }),
                    referenced->Get_Name(), Helper::To_wString(expectedBase));
                return E_NOINTERFACE;
            }

            Shared<GameObject> owner;
            const GameObject* referrer = dynamic_cast<GameObject*>(&target);
            if (auto* component = dynamic_cast<Component*>(&target)) {
                owner = component->Get_Owner();
                referrer = owner.get();
            }
            uint32 referrerLevel{}, referencedLevel{};
            if (referrer && SUCCEEDED(m_ObjectManager->Find_Level(referrer->Get_ObjectGuid(), referrerLevel)) &&
                SUCCEEDED(m_ObjectManager->Find_Level(*objectGuid, referencedLevel)) &&
                referrerLevel != referencedLevel) {
                LOG_WARN(L"{} cannot reference {}: it is in level {}, not level {}",
                    Helper::To_wString(string{ propertyName }), referenced->Get_Name(), referencedLevel, referrerLevel);
                return E_INVALIDARG;
            }
        }
    }
    return m_Registry->Write_Property(target, propertyName, value);
}

HRESULT Game::Add_TypePrototype_Internal(uint32 levIndex, const Shared<Object>& object) const
{
	return m_PrototypeManager->Add_TypePrototype(levIndex, object);
}

HRESULT Game::Add_ResourceComponentPrototype_Internal(uint32 levIndex,
	const Shared<Component>& component, const wstring& resourceTag) const
{
	return m_PrototypeManager->Add_ResourceComponentPrototype(
		levIndex, component, resourceTag);
}
HRESULT Game::SerializeLevel(uint32 levIndex, const wstring &path) const
{
    return m_LevelSerializer->SerializeLevel(levIndex, path);
}

HRESULT Game::DeSerializeLevel(const wstring &path) const
{
    return m_LevelSerializer->DeSerializeLevel(path);
}

HRESULT Game::Register_Prefab(PrefabGuid prefabGuid, const wstring& path) const
{
	return m_PrefabManager->Register_Prefab(prefabGuid, path);
}

HRESULT Game::Register_PrefabDocument(const wstring& path, PrefabGuid& outPrefabGuid) const
{
	return m_PrefabManager->Register_PrefabDocument(path, outPrefabGuid);
}

HRESULT Game::Load_PrefabRepository(const wstring& directoryPath) const
{
	return m_PrefabManager->Load_PrefabRepository(directoryPath);
}

HRESULT Game::Unregister_Prefab(PrefabGuid prefabGuid) const
{
	return m_PrefabManager->Unregister_Prefab(prefabGuid);
}

wstring Game::Find_PrefabPath(PrefabGuid prefabGuid) const
{
	return m_PrefabManager->Find_PrefabPath(prefabGuid);
}

vector<pair<PrefabGuid, wstring>> Game::Get_PrefabDocuments() const
{
	return m_PrefabManager->Get_PrefabDocuments();
}

HRESULT Game::SerializePrefabDocument(PrefabGuid prefabGuid,
	const Shared<GameObject>& selectedRoot, uint32 levIndex) const
{
	const uint32 targetLevel = levIndex == UINT_MAX
		? Get_CurrentLevelIndex()
		: levIndex;
	return m_PrefabManager->SerializePrefabDocument(prefabGuid, targetLevel, selectedRoot);
}

HRESULT Game::DeSerializePrefabDocument(PrefabGuid prefabGuid,
	Shared<GameObject>& outRoot, uint32 levIndex) const
{
	const uint32 targetLevel = levIndex == UINT_MAX
		? Get_CurrentLevelIndex()
		: levIndex;
	return m_PrefabManager->DeSerializePrefabDocument(prefabGuid, targetLevel, outRoot);
}

HRESULT Game::SerializeSubtreeSnapshot(PrefabGuid snapshotGuid,
	const Shared<GameObject>& selectedRoot, string& outSnapshot, uint32 levIndex) const
{
	const uint32 targetLevel = levIndex == UINT_MAX
		? Get_CurrentLevelIndex()
		: levIndex;
	return m_LevelSerializer->SerializeSubtreeSnapshot(
		snapshotGuid, targetLevel, selectedRoot, outSnapshot);
}

HRESULT Game::DeSerializeSubtreeSnapshot(PrefabGuid snapshotGuid,
	const string& snapshot, Bool preserveObjectGuids,
	Shared<GameObject>& outRoot, uint32 levIndex) const
{
	const uint32 targetLevel = levIndex == UINT_MAX
		? Get_CurrentLevelIndex()
		: levIndex;
	return m_LevelSerializer->DeSerializeSubtreeSnapshot(
		snapshotGuid, targetLevel, snapshot, preserveObjectGuids, outRoot);
}

ObjectGuid Game::Consume_RestoredObjectGuid() const
{
	return m_PrefabManager->Consume_RestoredObjectGuid();
}

uint32 Game::Consume_InitializingLevel() const
{
	return m_ObjectManager->Consume_InitializingLevel();
}

HRESULT Game::Add_Font(const wstring& fontTag, const tChar* fontFilePath)
{
    return m_FontManager->Add_Font(fontTag, fontFilePath);
}

void Game::Draw_Font(const wstring& fontTag, const tChar* text, const Vector2& position, const Color& color,
    Float scale, Float spacing)
{
    m_FontManager->Draw(fontTag, text, position, color, scale, spacing);
}

Vector2 Game::Measure_Font(const wstring& fontTag, const tChar* text, Float spacing) const
{
    return m_FontManager->Measure(fontTag, text, spacing);
}

HRESULT Game::LoadSoundFile(const wstring& path) const
{
    return m_SoundManager->LoadSoundFile(path);
}

HRESULT Game::Load_Sound(const wstring& soundTag, const vector<wstring>& soundFilePaths, SOUND_ORDER order) const
{
    return m_SoundManager->Load_Sound(soundTag, soundFilePaths, order);
}

Bool Game::Is_SoundLoaded(const wstring& soundKey) const
{
    return m_SoundManager->Is_SoundLoaded(soundKey);
}

Bool Game::Is_SoundPlaying(SOUNDCHANNEL channel) const
{
    return m_SoundManager->Is_ChannelPlaying(channel);
}

Float Game::Get_SoundLevel(SOUNDCHANNEL channel) const
{
    return m_SoundManager->Get_ChannelLevel(channel);
}

void Game::PlaySoundFX(const wstring& soundKey, SOUNDCHANNEL id, Float volume, const Vector3* position) const
{
    m_SoundManager->PlaySoundFx(soundKey, id, volume, position);
}

void Game::PlaySoundFXOnce(const wstring& soundKey, SOUNDCHANNEL id, Float volume, const Vector3* position) const
{
    m_SoundManager->PlaySoundFxOnce(soundKey, id, volume, position);
}

void Game::PlaySoundLoopSection(const wstring& soundKey, SOUNDCHANNEL id, Float volume, uint32 loopStartMs, uint32 loopEndMs, Bool playIntro, Float fadeInSeconds) const
{
    m_SoundManager->PlaySoundLoopSection(soundKey, id, volume, loopStartMs, loopEndMs, playIntro, fadeInSeconds);
}

void Game::StopSound(SOUNDCHANNEL targetChannel) const
{
    if (targetChannel == SOUNDCHANNEL::MAX_CHANNELS)
        m_SoundManager->StopAll();
    else
        m_SoundManager->StopChannel(targetChannel);
}

void Game::FadeSound(SOUNDCHANNEL targetChannel, Float gain, Float seconds, Bool stop) const
{
    m_SoundManager->Fade_Channel(targetChannel, gain, seconds, stop);
}

void Game::Reset_SoundSequence(const wstring& soundKey) const
{
    m_SoundManager->Reset_Sequence(soundKey);
}

uint32 Game::PlaySound3D(const wstring& soundKey, Float volume, const Vector3& position, Float nearDistance, Float farDistance, Bool loop) const
{
    return m_SoundManager->Play_3D(soundKey, volume, position, nearDistance, farDistance, loop);
}

void Game::StopLoopSound3D(uint32 handle) const
{
    m_SoundManager->Stop_Loop3D(handle);
}

void Game::MoveLoopSound3D(uint32 handle, const Vector3& position) const
{
    m_SoundManager->Move_Loop3D(handle, position);
}

HRESULT Game::Add_Instance_Event(uint32 levIndex, const wstring& eventTag, const std::function<void()>& callback) const
{
    return m_EventManager->Add_EventOnce(levIndex, eventTag, callback);
}

HRESULT Game::Add_Permanent_Event(uint32 levIndex, const wstring& eventTag, const std::function<void()>& callback) const
{
    return m_EventManager->Add_EventPermanent(levIndex, eventTag, callback);
}

HRESULT Game::Remove_Event(uint32 levIndex, const wstring& eventTag) const
{
    return m_EventManager->Remove_Event(levIndex, eventTag);
}

void Game::Add_Collider(const Shared<class Collider>& collider) const
{
    m_CollisionManager->Add_Collider(collider);
}

void Game::Remove_Collider(const Shared<class Collider>& collider) const
{
    m_CollisionManager->Remove_Collider(collider, m_PhysicsManager.get());
}

Bool Game::Is_PhysicsReady() const
{
    return m_PhysicsManager && m_PhysicsManager->Is_Ready();
}

uint32 Game::Add_StaticCollision(const wstring& modelTag, const Matrix& worldMatrix) const
{
    return m_PhysicsManager ? m_PhysicsManager->Add_StaticActor(modelTag, worldMatrix) : 0;
}

void Game::Remove_StaticCollision(uint32 handle) const
{
    if (m_PhysicsManager)
        m_PhysicsManager->Remove_StaticActor(handle);
}

Bool Game::Raycast_Physics(const Vector3& origin, const Vector3& direction,
    Float distance, Vector3& outHit, Vector3* outNormal) const
{
    return m_PhysicsManager && m_PhysicsManager->Raycast(origin, direction, distance, outHit, outNormal);
}

Bool Game::Has_StaticCollision() const
{
    return m_PhysicsManager && m_PhysicsManager->Has_StaticCollision();
}

Bool Game::Get_StaticCollisionBounds(BoundingBox& outBounds) const
{
    return m_PhysicsManager && m_PhysicsManager->Get_StaticBounds(outBounds);
}

uint32 Game::Create_CharacterController(const CHARACTER_CONTROLLER_DESC& desc, const Vector3& footPosition) const
{
    return m_PhysicsManager ? m_PhysicsManager->Create_Controller(desc, footPosition) : 0;
}

uint32 Game::Move_CharacterController(uint32 handle, const Vector3& displacement, Float timeDelta) const
{
    return m_PhysicsManager ? m_PhysicsManager->Move_Controller(handle, displacement, timeDelta) : 0;
}

void Game::Set_ControllerFootPosition(uint32 handle, const Vector3& footPosition) const
{
    if (m_PhysicsManager)
        m_PhysicsManager->Set_ControllerFootPosition(handle, footPosition);
}

Vector3 Game::Get_ControllerFootPosition(uint32 handle) const
{
    return m_PhysicsManager ? m_PhysicsManager->Get_ControllerFootPosition(handle) : Vector3::Zero;
}

void Game::Release_CharacterController(uint32 handle) const
{
    if (m_PhysicsManager)
        m_PhysicsManager->Release_Controller(handle);
}

uint32 Game::Create_SoftBody(const SOFTBODY_DESC& desc, const vector<Matrix>& jointMatrices) const
{
    return m_PhysicsManager ? m_PhysicsManager->Create_SoftBody(desc, jointMatrices) : 0;
}

void Game::Simulate_SoftBody(uint32 handle, const vector<Matrix>& jointMatrices, Float timeDelta, Bool reset,
    const SOFTBODY_TUNING& tuning, const vector<WIND_VOLUME>& winds) const
{
    if (m_PhysicsManager)
        m_PhysicsManager->Simulate_SoftBody(handle, jointMatrices, timeDelta, reset, tuning, winds);
}

void Game::Get_SoftBodyPositions(uint32 handle, vector<Vector3>& outPositions) const
{
    if (m_PhysicsManager)
        m_PhysicsManager->Get_SoftBodyPositions(handle, outPositions);
    else
        outPositions.clear();
}

void Game::Release_SoftBody(uint32 handle) const
{
    if (m_PhysicsManager)
        m_PhysicsManager->Release_SoftBody(handle);
}

uint32 Game::Add_SoftBodyCollider(uint32 softBody, Float radius, Float halfHeight) const
{
    return m_PhysicsManager ? m_PhysicsManager->Add_SoftBodyCollider(softBody, radius, halfHeight) : 0;
}

void Game::Move_SoftBodyCollider(uint32 handle, const Vector3& from, const Vector3& to) const
{
    if (m_PhysicsManager)
        m_PhysicsManager->Move_SoftBodyCollider(handle, from, to);
}

void Game::Remove_SoftBodyCollider(uint32 handle) const
{
    if (m_PhysicsManager)
        m_PhysicsManager->Remove_SoftBodyCollider(handle);
}

NavigationBuilder::NAV_BUILD_RESULT Game::Build_Navigation(const Float* vertices, int32 numVertices,
	const int32* triangles, int32 numTriangles, const NavigationBuilder::NAV_BUILD_PARAMS_DESC& params)
{
    return m_NavigationBuilder->Build(vertices, numVertices, triangles, numTriangles, params);
}

vector<NavCellBinary> Game::Bake_Navigation(const Shared<Model>& model, const rcConfig& config) const
{
    return m_NavigationBuilder->Bake_Navigation(model, config);
}

HRESULT Game::Export_Navigation(const string& fileName, const Shared<Model>& model, const rcConfig& config) const
{
    return m_NavigationBuilder->Export_Binary(fileName, model, config);
}

vector<NavCell> Game::Import_Navigation(const string& filePath) const
{
    return m_NavigationBuilder->Import_Binary(filePath);
}


HRESULT Game::Add_RenderTarget(const wstring& renderTargetTag, uint32 sizeX, uint32 sizeY, DXGI_FORMAT pixelFormat, const Color& color) const
{
    return m_RenderTargetManager->Add_RenderTarget(renderTargetTag, sizeX, sizeY, pixelFormat, color);
}

HRESULT Game::Add_MultiRenderTarget(const wstring& multiRenderTargetTag, const wstring& renderTargetTag) const
{
    return m_RenderTargetManager->Add_MultiRenderTarget(multiRenderTargetTag, renderTargetTag);
}

HRESULT Game::Begin_MultiRenderTarget(const wstring& multiRenderTargetTag) const
{
    return m_RenderTargetManager->Begin_MultiRenderTarget(multiRenderTargetTag);
}

HRESULT Game::End_MultiRenderTarget() const
{
    return m_RenderTargetManager->End_MultiRenderTarget();
}

HRESULT Game::Bind_RenderTarget_ShaderResource(const Shared<Shader>& shader, const Char* constantName, const wstring& renderTargetTag) const
{
    return m_RenderTargetManager->Bind_ShaderResource(shader, constantName, renderTargetTag);
}

HRESULT Game::Copy_BoundRenderTarget(const wstring& renderTargetTag) const
{
    return m_RenderTargetManager->Copy_BoundTarget(renderTargetTag);
}


Shared<GameObject> Game::Instantiate_GameObjectPrototype(
	const Shared<GameObject>& prototype, uint32 levIndex, void* arg,
	ObjectGuid objectGuid) const
{
    if (!prototype)
        return nullptr;

    const uint32 targetLevel = levIndex == UINT_MAX
        ? m_LevelManager->Get_CurrentLevelIndex()
        : levIndex;

    if (objectGuid.Is_Valid()) {
		if (m_ObjectManager->Find_ByObjectGuid(objectGuid) ||
			FAILED(m_PrefabManager->Prepare_RestoredObjectGuid(objectGuid))) {
			return nullptr;
		}
    }

    m_ObjectManager->Prepare_InitializingLevel(targetLevel);
    Shared<GameObject> cloned = prototype->Clone(arg);
    m_ObjectManager->Consume_InitializingLevel();
	if (objectGuid.Is_Valid()) {
		const ObjectGuid unconsumedObjectGuid = m_PrefabManager->Consume_RestoredObjectGuid();
		if (unconsumedObjectGuid.Is_Valid()) {
			LOG_ERROR(L"GameObject Clone did not call GameObject::Initialize before returning");
			if (cloned)
				cloned->On_Destroy();
			return nullptr;
		}
	}
    if (cloned)
    {
        if (FAILED(m_ObjectManager->Add_GameObject(targetLevel, cloned))) {
            cloned->On_Destroy();
            return nullptr;
        }

        if (prototype->Get_GameObjectType() == GAMEOBJECTTYPE::CAMERA &&
            FAILED(Add_Camera(targetLevel, static_pointer_cast<Camera>(cloned)))) {
            m_ObjectManager->Remove_GameObject(targetLevel, cloned);
            return nullptr;
        }
    }

    return cloned;
}

Shared<Component> Game::Instantiate_ComponentPrototype(
	const Shared<Component>& prototype, void* arg) const
{
	return prototype ? prototype->Clone(arg) : nullptr;
}

Shared<GameObject> Game::Instantiate_GameObjectByTag(
	const wstring& prototypeTag, uint32 levIndex, void* arg) const
{
	if (prototypeTag.empty())
	{
        LOG_ERROR("There is no Tag to prototype");
		return nullptr;
	}

    const uint32 targetLevel = levIndex == UINT_MAX
        ? m_LevelManager->Get_CurrentLevelIndex()
        : levIndex;
    Shared<GameObject> prototype = m_PrototypeManager->Find_GameObject(targetLevel, prototypeTag);
    if (!prototype && targetLevel != 0)
        prototype = m_PrototypeManager->Find_GameObject(0, prototypeTag);

    if (!prototype) {
        LOG_WARN(L"[Game] Failed to find prototype by tag: {}. (Searched Level 0 and {})", prototypeTag, 
            targetLevel);
        return nullptr;
    }

    return Instantiate_GameObjectPrototype(prototype, targetLevel, arg);
}

Shared<Component> Game::Instantiate_ComponentByTag(
	const wstring& prototypeTag, uint32 levIndex, void* arg) const
{
	if (prototypeTag.empty()) {
        LOG_ERROR("There is no Tag to prototype");
		return nullptr;
	}

	const uint32 targetLevel = levIndex == UINT_MAX
		? m_LevelManager->Get_CurrentLevelIndex()
		: levIndex;
	Shared<Component> prototype = m_PrototypeManager->Find_Component(targetLevel, prototypeTag);
	if (!prototype && targetLevel != 0)
		prototype = m_PrototypeManager->Find_Component(0, prototypeTag);
	if (!prototype) {
		LOG_WARN(L"[Game] Failed to find component prototype by tag: {}. (Searched Level 0 and {})",
			prototypeTag, targetLevel);
		return nullptr;
	}

	return Instantiate_ComponentPrototype(prototype, arg);
}

Shared<GameObject> Game::Instantiate_GameObjectByRuntimeTypeId(
	RuntimeTypeId runtimeTypeId, uint32 levIndex, void* arg,
	ObjectGuid objectGuid) const
{
    const uint32 targetLevel = levIndex == UINT_MAX
        ? m_LevelManager->Get_CurrentLevelIndex()
        : levIndex;

    Shared<GameObject> prototype = m_PrototypeManager->Find_DefaultGameObject(targetLevel, runtimeTypeId);
    if (!prototype && targetLevel != 0)
        prototype = m_PrototypeManager->Find_DefaultGameObject(0, runtimeTypeId);

    return Instantiate_GameObjectPrototype(prototype, targetLevel, arg, objectGuid);
}

Shared<Component> Game::Instantiate_ComponentByRuntimeTypeId(
	RuntimeTypeId runtimeTypeId, uint32 levIndex, void* arg) const
{
	const uint32 targetLevel = levIndex == UINT_MAX
		? m_LevelManager->Get_CurrentLevelIndex()
		: levIndex;

	Shared<Component> prototype = m_PrototypeManager->Find_DefaultComponent(targetLevel, runtimeTypeId);
	if (!prototype && targetLevel != 0)
		prototype = m_PrototypeManager->Find_DefaultComponent(0, runtimeTypeId);

	return Instantiate_ComponentPrototype(prototype, arg);
}

Shared<GameObject> Game::Instantiate_GameObject(std::string_view registeredName,
                                                uint32 levIndex,
                                                void* arg,
                                                ObjectGuid objectGuid) const
{
    const RuntimeTypeId runtimeTypeId = Find_RuntimeTypeId(registeredName);
    if (runtimeTypeId == 0) {
        LOG_ERROR(L"Unknown GameObject RTTR RegisteredName {}", Helper::To_wString(registeredName));
        return nullptr;
    }

    return Instantiate_GameObjectByRuntimeTypeId(runtimeTypeId, levIndex, arg, objectGuid);
}

Bool Game::Can_InstantiateGameObject(std::string_view registeredName, uint32 levIndex) const
{
	const RuntimeTypeId runtimeTypeId = Find_RuntimeTypeId(registeredName);
	const uint32 targetLevel = levIndex == UINT_MAX
		? m_LevelManager->Get_CurrentLevelIndex()
		: levIndex;
	if (runtimeTypeId == 0)
		return false;

	if (m_PrototypeManager->Find_DefaultGameObject(targetLevel, runtimeTypeId))
		return true;

	return targetLevel != 0 &&
		m_PrototypeManager->Find_DefaultGameObject(0, runtimeTypeId) != nullptr;
}

Bool Game::Can_InstantiateComponent(std::string_view registeredName, uint32 levIndex) const
{
	const RuntimeTypeId runtimeTypeId = Find_RuntimeTypeId(registeredName);
	const uint32 targetLevel = levIndex == UINT_MAX
		? m_LevelManager->Get_CurrentLevelIndex()
		: levIndex;
	if (runtimeTypeId == 0)
		return false;

	if (m_PrototypeManager->Find_DefaultComponent(targetLevel, runtimeTypeId))
		return true;

	return targetLevel != 0 &&
		m_PrototypeManager->Find_DefaultComponent(0, runtimeTypeId) != nullptr;
}

#ifdef _DEBUG
void Game::Render_CollisionDebug() const
{
    // 정적 충돌 면을 먼저 칠하고 게임플레이 콜라이더 선을 그 위에 그린다. 둘 다 F2 하나로 켠다.
    if (m_CollisionManager->Get_DebugMode() && m_PhysicsManager)
        m_PhysicsManager->Render_StaticDebug();
    m_CollisionManager->Render_Debug();
}

Bool Game::Toggle_RenderDebug() const
{
    return m_CollisionManager->Toggle_DebugMode();
}

Bool Game::Get_RenderDebug() const
{
    return m_CollisionManager->Get_DebugMode();
}

Bool Game::Toggle_RenderTargetDebug() const
{
    return m_Renderer->Toggle_RenderTargetDebug();
}

Bool Game::Get_RenderTargetDebug() const
{
    return m_Renderer->Get_RenderTargetDebug();
}
#endif

void Game::Add_CulledMesh() const
{
    m_Renderer->Add_CulledMesh();
}

uint32 Game::Get_FrameCulledCount() const
{
    return m_Renderer->Get_FrameCulledCount();
}

uint32 Game::Get_FrameOccludedCount() const
{
    return m_Renderer->Get_FrameOccludedCount();
}

void Game::Add_LodTile() const
{
    m_Renderer->Add_LodTile();
}

uint32 Game::Get_FrameLodTileCount() const
{
    return m_Renderer->Get_FrameLodTileCount();
}

void Game::Add_DrawStats(uint32 triangleCount) const
{
    m_Renderer->Add_DrawStats(triangleCount);
}

void Game::Add_InstancedProps(uint32 instanceCount) const
{
    m_Renderer->Add_InstancedProps(instanceCount);
}

uint32 Game::Get_FrameDrawCount() const
{
    return m_Renderer->Get_FrameDrawCount();
}

uint64_t Game::Get_FrameTriangleCount() const
{
    return m_Renderer->Get_FrameTriangleCount();
}

uint32 Game::Get_FrameInstanceCount() const
{
    return m_Renderer->Get_FrameInstanceCount();
}

#ifdef _DEBUG
uint32 Game::Get_FrameCascadeDraws(uint32 cascade) const
{
    return m_Renderer->Get_FrameCascadeDraws(cascade);
}

Float Game::Get_FrameCascadeMs(uint32 cascade) const
{
    return m_Renderer->Get_FrameCascadeMs(cascade);
}

uint32 Game::Get_FrameShadowBakes() const
{
    return m_Renderer->Get_FrameShadowBakes();
}

HRESULT Game::Ready_RenderTarget_Debug(const wstring& renderTargetTag, Float x, Float y, Float sizeX, Float sizeY) const
{
    return m_RenderTargetManager->Ready_RenderTarget_Debug(renderTargetTag, x, y, sizeX, sizeY);
}

HRESULT Game::Render_RenderTarget_Debug(const Shared<class VIBuffer_Rect>& buffer, const Shared<class Shader>& shader, const wstring& multiRenderTargetTag) const
{
    return m_RenderTargetManager->Render_RenderTarget_Debug(buffer, shader, multiRenderTargetTag);
}

#endif
