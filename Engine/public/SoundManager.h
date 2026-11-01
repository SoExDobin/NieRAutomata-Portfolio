#pragma once

#include "EngineManager.h"

NS_BEGIN(Engine)

class SoundManager : public EngineManager
{
public:
	// 사운드 프리셋: 한 태그에 소리 여러 개를 묶고, 재생할 때마다 order대로 하나를 고른다.
	typedef struct tagSoundInfo
	{
		vector<FMOD_SOUND*> sounds;
		Float volume = 1.0f;
		SOUND_ORDER order = SOUND_ORDER::SEQUENCE;
		uint32 last = UINT32_MAX;   // 마지막으로 고른 소리
	} SOUND_DESC;

public:
	explicit SoundManager();
	virtual ~SoundManager() override;

public:
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void *arg) override;

public:
	// position을 주면 그 자리에서 나는 소리로 재생해 리스너(카메라)와의 거리만큼 줄어든다. 없으면 거리와 무관한 2D.
	void PlaySoundFxOnce(const wstring& soundKey, SOUNDCHANNEL id, Float volume = -1.f, const Vector3* position = nullptr);
	void PlaySoundFx(const wstring& soundKey, SOUNDCHANNEL id, Float volume = -1.f, const Vector3* position = nullptr);
	Bool Is_SoundLoaded(const wstring& soundKey) const;
	Bool Is_ChannelPlaying(SOUNDCHANNEL id) const;
	Float Get_ChannelLevel(SOUNDCHANNEL id) const;
	HRESULT StopChannel(SOUNDCHANNEL id);
	// position에서 나는 소리: nearDistance까지는 volume 그대로, 멀어질수록 줄어 farDistance에서 들리지 않는다.
	// loop면 계속 나며 돌려준 핸들(0 = 실패)로 Stop_Loop3D한다. 한 번 나는 소리는 0을 돌려준다.
	uint32 Play_3D(const wstring& soundKey, Float volume, const Vector3& position, Float nearDistance, Float farDistance, Bool loop);
	void Stop_Loop3D(uint32 handle);
	// 반복 소리를 position으로 옮긴다. 움직이는 소유자가 매 프레임 불러 소리가 따라오게 한다.
	void Move_Loop3D(uint32 handle, const Vector3& position);
	void Reset_Sequence(const wstring& soundKey);
	// 매 프레임 카메라 월드 행렬로 3D 리스너를 옮기고 FMOD를 갱신한다.
	void Update_Listener(const Matrix& cameraWorld);
	HRESULT StopAll();
	HRESULT SetChannelVolume(SOUNDCHANNEL id, Float volume);
	HRESULT LoadSoundFile(const wstring& path);
	HRESULT Load_Sound(const wstring& soundTag, const vector<wstring>& soundFilePaths, SOUND_ORDER order = SOUND_ORDER::SEQUENCE);
	// fadeInSeconds > 0이면 0에서 volume까지 그 시간 동안 올린다.
	void PlaySoundLoopSection(const wstring& soundKey,
							  SOUNDCHANNEL id,
							  Float volume,
							  uint32 loopStartMs,
							  uint32 loopEndMs,
							  Bool playIntro,
							  Float fadeInSeconds = 0.f);
	// 채널 소리를 지금 크기에서 gain(볼륨에 곱하는 배율, 0~1)까지 seconds 동안 바꾼다. stop이면 다 줄어든 뒤 멈춘다.
	// 멈추지 않고 0으로 줄이면 들리지 않은 채 계속 흘러, 다시 올리면 그동안 흐른 자리부터 들린다.
	void Fade_Channel(SOUNDCHANNEL id, Float gain, Float seconds, Bool stop);

public:
	HRESULT Clear_SoundSources();

private:
	FMOD_SOUND* Next_Sound(SOUND_DESC& desc);
	void Start_Channel(FMOD_CHANNEL* channel, Float volume, const Vector3* position);
	// 채널의 페이드 점을 지우고 from에서 to까지 seconds 동안 바뀌게 건다. 끝나는 DSP 시각을 돌려준다.
	unsigned long long Fade(FMOD_CHANNEL* channel, Float from, Float to, Float seconds) const;

private:
	unordered_map<wstring, SOUND_DESC> m_SoundSource;
	FMOD_CHANNEL *m_SoundChannel[ETOI(SOUNDCHANNEL::MAX_CHANNELS)] = {};
	unordered_map<uint32, FMOD_CHANNEL*> m_Loops3D;
	uint32 m_NextLoop3D = 1;
	FMOD_SYSTEM *m_System = nullptr;

public:
	static Unique<SoundManager> Create();
};

NS_END
