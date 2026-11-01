#pragma once
#include "Component.h"

NS_BEGIN(Engine)

/* 오브젝트가 가진 소리 나는 자리. 소리는 소유 오브젝트 위치에서 나고, 카메라가 NearDistance 안이면 Volume 그대로,
   멀어질수록 줄어 FarDistance에서 들리지 않는다.
   - SoundTag: 비어 있지 않으면 씬을 불러온 뒤(Post_Load) 그 자리에서 계속 반복한다(환경음).
   - Play: 그 순간 오브젝트 위치에서 한 번 낸다(발소리·공격음). */
class ENGINE_DLL SoundEmitter final : public Component
{
public:
	typedef struct tagSoundEmitterDesc : public COMPONENT_DESC
	{
		wstring soundTag{};
		Float volume = 0.5f;
		Float nearDistance = 5.f;
		Float farDistance = 40.f;
	} SOUND_EMITTER_DESC;

public:
	explicit SoundEmitter();
	explicit SoundEmitter(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit SoundEmitter(const SoundEmitter& prototype);
	virtual ~SoundEmitter() override;

public:
	COMPONENT_TYPE Get_ComponentType() const override { return COMPONENT_TYPE::SOUND_EMITTER; }
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;

public:
	void Play(const wstring& soundTag) const;

	wstring Get_SoundTag() const { return m_SoundTag; }
	void Set_SoundTag(wstring soundTag);
	Float Get_Volume() const { return m_Volume; }
	void Set_Volume(Float volume);
	Float Get_NearDistance() const { return m_NearDistance; }
	void Set_NearDistance(Float distance);
	Float Get_FarDistance() const { return m_FarDistance; }
	void Set_FarDistance(Float distance);

private:
	HRESULT Post_Load() override;
	Vector3 Get_OwnerPosition() const;
	// 반복 소리를 지금 값으로 다시 건다. Inspector에서 값을 바꾸면 바로 들린다.
	void Restart_Loop();

private:
	wstring m_SoundTag{};
	Float m_Volume = 0.5f;
	Float m_NearDistance = 5.f;
	Float m_FarDistance = 40.f;
	uint32 m_LoopHandle = 0;
	Bool m_Loaded = false;

public:
	Shared<Component> Clone(void* arg = nullptr) override;
	static Shared<SoundEmitter> CreatePrototype();
	static Shared<SoundEmitter> Create(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context);
};

NS_END
