#include "SoundManager.h"

#include "Random_Helper.h"
#include <cmath>

NS_BEGIN(Engine)

SoundManager::SoundManager() : EngineManager{}
	, m_System{nullptr}
{
	for (int i = 0; i < ETOI(SOUNDCHANNEL::MAX_CHANNELS); ++i)
	{
		m_SoundChannel[i] = nullptr;
	}
}

SoundManager::~SoundManager()
{
	Clear_SoundSources();
	if (m_System) {
		FMOD_System_Close(m_System);
		FMOD_System_Release(m_System);
		m_System = nullptr;
	}
}

namespace
{
	constexpr int32 SOUND_VOICE_COUNT = 256;
	// 모든 소리에 곱해지는 마스터 볼륨.
	constexpr Float SOUND_MASTER_VOLUME = 0.75f;
}

HRESULT SoundManager::Initialize_Prototype()
{
	EngineManager::Initialize_Prototype();

	FMOD_System_Create(&m_System, FMOD_VERSION);
	// 채널 슬롯(SOUNDCHANNEL)과 별개로, 한 번에 낼 수 있는 소리 수. 반복 환경음과 겹치는 효과음이 서로 끊지 않게 넉넉히 둔다.
	FMOD_System_Init(m_System, SOUND_VOICE_COUNT, FMOD_INIT_NORMAL, nullptr);

	FMOD_CHANNELGROUP* master = nullptr;
	if (FMOD_OK == FMOD_System_GetMasterChannelGroup(m_System, &master) && master)
		FMOD_ChannelGroup_SetVolume(master, SOUND_MASTER_VOLUME);

	return S_OK;
}

HRESULT SoundManager::Initialize(void *arg)
{
	return S_OK;
}

namespace
{
	// 위치 소리의 거리 감쇠: SOUND_3D_MIN_DISTANCE까지는 원래 크기, SOUND_3D_MAX_DISTANCE에서 0(거리 제곱 곡선).
	constexpr Float SOUND_3D_MIN_DISTANCE = 5.f;
	constexpr Float SOUND_3D_MAX_DISTANCE = 60.f;

	FMOD_VECTOR To_FmodVector(const Vector3& v) { return FMOD_VECTOR{ v.x, v.y, v.z }; }

	// 채널의 페이드 배율을 parentClock 시각 값으로 읽는다. 페이드 점 사이는 직선, 점이 없으면 1.
	Float Get_FadeGain(FMOD_CHANNEL* channel, unsigned long long parentClock)
	{
		unsigned int count = 0;
		if (FMOD_OK != FMOD_Channel_GetFadePoints(channel, &count, nullptr, nullptr) || 0 == count)
			return 1.f;
		vector<unsigned long long> clocks(count);
		vector<Float> gains(count);
		FMOD_Channel_GetFadePoints(channel, &count, clocks.data(), gains.data());
		if (parentClock <= clocks.front())
			return gains.front();
		for (unsigned int i = 1; i < count; ++i)
		{
			if (parentClock < clocks[i])
			{
				const Float t = static_cast<Float>(parentClock - clocks[i - 1]) / static_cast<Float>(clocks[i] - clocks[i - 1]);
				return gains[i - 1] + (gains[i] - gains[i - 1]) * t;
			}
		}
		return gains.back();
	}
}

void SoundManager::PlaySoundFxOnce(const wstring& soundKey, SOUNDCHANNEL id, Float volume, const Vector3* position)
{
	auto iter = m_SoundSource.find(soundKey);
	if (iter == m_SoundSource.end())
	{
		return;
	}

	int32 iID = ETOI(id);

	FMOD_BOOL bPlay = FALSE;
	if (m_SoundChannel[iID])
	{
		FMOD_Channel_IsPlaying(m_SoundChannel[iID], &bPlay);
	}

	if (volume >= 0.f)
	{
		iter->second.volume = volume;
	}

	if (!bPlay)
	{
		FMOD_System_PlaySound(m_System, Next_Sound(iter->second), nullptr, TRUE, &m_SoundChannel[iID]);
		Start_Channel(m_SoundChannel[iID], iter->second.volume, position);
	}
	else
	{
		FMOD_Channel_SetVolume(m_SoundChannel[iID], iter->second.volume);
	}
}

Bool SoundManager::Is_SoundLoaded(const wstring& soundKey) const
{
	const auto found = m_SoundSource.find(soundKey);
	return found != m_SoundSource.end() && !found->second.sounds.empty();
}

Bool SoundManager::Is_ChannelPlaying(SOUNDCHANNEL id) const
{
    const int index = ETOI(id);
    if (index < 0 || index >= ETOI(SOUNDCHANNEL::MAX_CHANNELS) || !m_SoundChannel[index]) return false;
    FMOD_BOOL playing = FALSE;
    return FMOD_Channel_IsPlaying(m_SoundChannel[index], &playing) == FMOD_OK && playing;
}

Float SoundManager::Get_ChannelLevel(SOUNDCHANNEL id) const
{
    if (!Is_ChannelPlaying(id)) return 0.f;
    auto channel = m_SoundChannel[ETOI(id)];
    FMOD_BOOL paused = FALSE, muted = FALSE;
    if (FMOD_Channel_GetPaused(channel, &paused) != FMOD_OK || paused ||
        FMOD_Channel_GetMute(channel, &muted) != FMOD_OK || muted) return 0.f;

    // Query this channel's output, rather than the master mix (BGM, combat and call tones).
    // Resolve the DSP each time so a stopped/reused channel cannot leave a cached handle.
    FMOD_DSP* dsp = nullptr;
    if (FMOD_Channel_GetDSP(channel, FMOD_CHANNELCONTROL_DSP_TAIL, &dsp) != FMOD_OK) return 0.f;
    FMOD_BOOL enabled = FALSE;
    if (FMOD_DSP_GetMeteringEnabled(dsp, nullptr, &enabled) != FMOD_OK) return 0.f;
    if (!enabled)
    {
        FMOD_DSP_SetMeteringEnabled(dsp, FALSE, TRUE);
        return 0.f; // The next audio mix supplies the first meter sample.
    }
    FMOD_DSP_METERING_INFO meter{};
    if (FMOD_DSP_GetMeteringInfo(dsp, nullptr, &meter) != FMOD_OK || meter.numsamples <= 0) return 0.f;
    Float level = 0.f;
    for (int i = 0; i < std::min<int>(meter.numchannels, 32); ++i)
        if (std::isfinite(meter.rmslevel[i])) level = std::max(level, meter.rmslevel[i]);
    return std::clamp(level, 0.f, 1.f);
}

void SoundManager::PlaySoundFx(const wstring &soundKey, SOUNDCHANNEL id, Float volume, const Vector3* position)
{
	auto iter = m_SoundSource.find(soundKey);
	if (iter == m_SoundSource.end())
	{
		return;
	}

	int32 iID = ETOI(id);

	if (volume >= 0.f)
	{
		iter->second.volume = volume;
	}

	FMOD_System_PlaySound(m_System, Next_Sound(iter->second), nullptr, TRUE, &m_SoundChannel[iID]);
	Start_Channel(m_SoundChannel[iID], iter->second.volume, position);
}

// 일시정지 상태로 만든 채널에 볼륨과 (있으면) 위치를 준 뒤 재생한다. 위치를 먼저 넣어야 첫 샘플부터 감쇠가 맞다.
void SoundManager::Start_Channel(FMOD_CHANNEL* channel, Float volume, const Vector3* position)
{
	if (nullptr == channel)
		return;

	FMOD_Channel_SetVolume(channel, volume);
	if (nullptr != position)
	{
		FMOD_Channel_SetMode(channel, FMOD_3D | FMOD_3D_LINEARSQUAREROLLOFF);
		FMOD_Channel_Set3DMinMaxDistance(channel, SOUND_3D_MIN_DISTANCE, SOUND_3D_MAX_DISTANCE);
		const FMOD_VECTOR fmodPosition = To_FmodVector(*position);
		FMOD_Channel_Set3DAttributes(channel, &fmodPosition, nullptr);
	}
	FMOD_Channel_SetPaused(channel, FALSE);
}

void SoundManager::Update_Listener(const Matrix& cameraWorld)
{
	// 엔진은 왼손 좌표계(D3D)이고 FMOD 기본값도 왼손이다. 카메라는 +Z(3행)를 본다.
	const FMOD_VECTOR position = To_FmodVector(cameraWorld.Translation());
	const FMOD_VECTOR forward = To_FmodVector(Vector3{ cameraWorld._31, cameraWorld._32, cameraWorld._33 });
	const FMOD_VECTOR up = To_FmodVector(Vector3{ cameraWorld._21, cameraWorld._22, cameraWorld._23 });
	FMOD_System_Set3DListenerAttributes(m_System, 0, &position, nullptr, &forward, &up);
	FMOD_System_Update(m_System);
}

HRESULT SoundManager::StopChannel(SOUNDCHANNEL id)
{
	int iID = ETOI(id);
	if (m_SoundChannel[iID])
	{
		FMOD_Channel_Stop(m_SoundChannel[iID]);
	}

	return S_OK;
}

uint32 SoundManager::Play_3D(const wstring& soundKey, Float volume, const Vector3& position, Float nearDistance, Float farDistance, Bool loop)
{
	auto iter = m_SoundSource.find(soundKey);
	if (iter == m_SoundSource.end())
		return 0;

	FMOD_CHANNEL* channel = nullptr;
	FMOD_System_PlaySound(m_System, Next_Sound(iter->second), nullptr, TRUE, &channel);
	if (nullptr == channel)
		return 0;

	FMOD_Channel_SetMode(channel, (loop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF) | FMOD_3D | FMOD_3D_LINEARSQUAREROLLOFF);
	if (loop)
		FMOD_Channel_SetLoopCount(channel, -1);
	FMOD_Channel_Set3DMinMaxDistance(channel, nearDistance, farDistance);
	const FMOD_VECTOR fmodPosition = To_FmodVector(position);
	FMOD_Channel_Set3DAttributes(channel, &fmodPosition, nullptr);
	FMOD_Channel_SetVolume(channel, volume);
	FMOD_Channel_SetPaused(channel, FALSE);
	if (!loop)
		return 0;

	const uint32 handle = m_NextLoop3D++;
	m_Loops3D[handle] = channel;
	return handle;
}

void SoundManager::Stop_Loop3D(uint32 handle)
{
	auto iter = m_Loops3D.find(handle);
	if (iter == m_Loops3D.end())
		return;
	FMOD_Channel_Stop(iter->second);
	m_Loops3D.erase(iter);
}

void SoundManager::Move_Loop3D(uint32 handle, const Vector3& position)
{
	auto iter = m_Loops3D.find(handle);
	if (iter == m_Loops3D.end())
		return;
	const FMOD_VECTOR fmodPosition = To_FmodVector(position);
	FMOD_Channel_Set3DAttributes(iter->second, &fmodPosition, nullptr);
}

// 순서 프리셋을 처음 소리부터 다시 고르게 한다. 콤보 한 타가 시작될 때 불러, 중간에 끊긴 타의 순서가 다음 타로 넘어가지 않게 한다.
void SoundManager::Reset_Sequence(const wstring& soundKey)
{
	auto iter = m_SoundSource.find(soundKey);
	if (iter != m_SoundSource.end())
		iter->second.last = UINT32_MAX;
}

HRESULT SoundManager::StopAll()
{
	for (int i = 0; i < ETOI(SOUNDCHANNEL::MAX_CHANNELS); ++i)
	{
		if (m_SoundChannel[i])
		{
			FMOD_Channel_Stop(m_SoundChannel[i]);
		}
	}

	return S_OK;
}

HRESULT SoundManager::SetChannelVolume(SOUNDCHANNEL id, Float volume)
{
	int iID = ETOI(id);
	
	if (m_SoundChannel[iID])
	{
		FMOD_Channel_SetVolume(m_SoundChannel[iID], volume);
		FMOD_System_Update(m_System);
	}

	return S_OK;
}

HRESULT SoundManager::LoadSoundFile(const wstring& path)
{
	std::wstring basePath = path;
	if (!basePath.empty())
	{
		Char back = basePath.back();
		if (back != L'/' && back != L'\\')
		{
			basePath += L'/';
		}
	}

	vector<wstring> extensions = { L"*.wav", L"*.mp3" };
	bool bFound = false;

	for (const auto& ext : extensions)
	{
		std::wstring wsSearchPath = basePath + ext;

		_wfinddata_t fd;
		intptr_t hFind = _wfindfirst(wsSearchPath.c_str(), &fd);
		if (hFind == -1)
		{
			continue;
		}

		bFound = true;

		do
		{
			std::wstring wsFullPath = basePath + fd.name;

			char szFullPath[MAX_PATH] = {};
			WideCharToMultiByte(CP_ACP, 0,
								wsFullPath.c_str(), -1,
								szFullPath, MAX_PATH,
								nullptr, nullptr);

			FMOD_SOUND *pSound = nullptr;
			FMOD_RESULT eRes = FMOD_System_CreateSound(m_System,
													   szFullPath,
													   FMOD_DEFAULT,
													   0,
													   &pSound);

			if (eRes == FMOD_OK)
			{
				std::wstring wsKey = fd.name;

				tagSoundInfo info;
				info.sounds.push_back(pSound);
				info.volume = 1.0f;

				m_SoundSource.emplace(wsKey, info);
			}

		} while (_wfindnext(hFind, &fd) != -1);

		_findclose(hFind);
	}

	if (!bFound)
	{
		// MSG_BOX("There is no Sound File");
		return E_FAIL;
	}

	FMOD_System_Update(m_System);

	return S_OK;
}

HRESULT SoundManager::Load_Sound(const wstring& soundTag, const vector<wstring>& soundFilePaths, SOUND_ORDER order)
{
	tagSoundInfo info;
	info.order = order;
	for (const wstring& soundFilePath : soundFilePaths)
	{
		char szFullPath[MAX_PATH] = {};
		WideCharToMultiByte(CP_ACP, 0,
							soundFilePath.c_str(), -1,
							szFullPath, MAX_PATH,
							nullptr, nullptr);

		FMOD_SOUND *pSound = nullptr;
		if (FMOD_OK != FMOD_System_CreateSound(m_System, szFullPath, FMOD_DEFAULT, 0, &pSound))
		{
			for (FMOD_SOUND* loaded : info.sounds)
				FMOD_Sound_Release(loaded);
			return E_FAIL;
		}
		info.sounds.push_back(pSound);
	}
	if (info.sounds.empty())
		return E_FAIL;

	auto iter = m_SoundSource.find(soundTag);
	if (iter != m_SoundSource.end())
	{
		for (FMOD_SOUND* sound : iter->second.sounds)
			FMOD_Sound_Release(sound);
	}

	m_SoundSource[soundTag] = info;
	FMOD_System_Update(m_System);
	return S_OK;
}

FMOD_SOUND* SoundManager::Next_Sound(SOUND_DESC& desc)
{
	const uint32 count = static_cast<uint32>(desc.sounds.size());
	uint32 pick = 0;
	if (desc.order == SOUND_ORDER::SEQUENCE)
	{
		pick = desc.last < count ? (desc.last + 1) % count : 0;
	}
	else if (count > 1)
	{
		// 바로 전 소리는 빼고 고른다. 같은 소리가 연달아 나면 변형을 둔 의미가 없다.
		pick = static_cast<uint32>(Helper::Random_Int(0, static_cast<int32>(count) - (desc.last < count ? 2 : 1)));
		if (desc.last < count && pick >= desc.last)
			++pick;
	}
	desc.last = pick;
	return desc.sounds[pick];
}

void SoundManager::PlaySoundLoopSection(
	const wstring& soundKey,
	SOUNDCHANNEL id,
	Float volume,
	uint32 loopStartMs,
	uint32 loopEndMs,
	Bool playIntro,
	Float fadeInSeconds)
{
	auto iter = m_SoundSource.find(soundKey);
	if (iter == m_SoundSource.end())
	{
		return;
	}

	int iID = ETOI(id);
	FMOD_SOUND *pSound = Next_Sound(iter->second);

	// FMOD의 루프 끝은 그 샘플까지 포함한다. 길이와 같은 ms를 넘기면 INVALID_PARAM으로 거절되고 루프 지점 없이
	// 파일 처음부터 반복하므로, 샘플 단위로 바꿔 끝을 마지막 샘플로 자른다.
	unsigned int lenPcm = 0;
	FMOD_Sound_GetLength(pSound, &lenPcm, FMOD_TIMEUNIT_PCM);
	Float frequency = 0.f;
	FMOD_Sound_GetDefaults(pSound, &frequency, nullptr);
	if (lenPcm == 0 || frequency <= 0.f)
	{
		return;
	}
	const auto To_Pcm = [&](uint32 ms)
	{
		const unsigned long long pcm = static_cast<unsigned long long>(ms) * static_cast<unsigned long long>(frequency) / 1000ull;
		return static_cast<unsigned int>(std::min<unsigned long long>(pcm, lenPcm - 1));
	};
	const unsigned int loopStart = To_Pcm(loopStartMs);
	const unsigned int loopEnd = To_Pcm(loopEndMs);
	if (loopEnd <= loopStart + 1)
	{
		return;
	}

	if (m_SoundChannel[iID])
	{
		FMOD_Channel_Stop(m_SoundChannel[iID]);
	}

	FMOD_System_PlaySound(m_System, pSound, nullptr, TRUE, &m_SoundChannel[iID]);

	FMOD_CHANNEL *ch = m_SoundChannel[iID];
	if (!ch)
	{
		return;
	}

	FMOD_Channel_SetMode(ch, FMOD_LOOP_NORMAL);
	FMOD_Channel_SetLoopPoints(ch,
							   loopStart, FMOD_TIMEUNIT_PCM,
							   loopEnd, FMOD_TIMEUNIT_PCM);

	if (!playIntro)
	{
		FMOD_Channel_SetPosition(ch, loopStart, FMOD_TIMEUNIT_PCM);
	}

	if (volume >= 0.f)
	{
		iter->second.volume = volume;
	}

	FMOD_Channel_SetVolume(ch, iter->second.volume);
	if (fadeInSeconds > 0.f)
		Fade(ch, 0.f, 1.f, fadeInSeconds);
	FMOD_Channel_SetPaused(ch, FALSE);

	FMOD_System_Update(m_System);
}

void SoundManager::Fade_Channel(SOUNDCHANNEL id, Float gain, Float seconds, Bool stop)
{
	FMOD_CHANNEL* channel = m_SoundChannel[ETOI(id)];
	FMOD_BOOL playing = FALSE;
	if (nullptr == channel || FMOD_OK != FMOD_Channel_IsPlaying(channel, &playing) || !playing)
		return;

	unsigned long long now = 0;
	FMOD_Channel_GetDSPClock(channel, nullptr, &now);
	const unsigned long long end = Fade(channel, Get_FadeGain(channel, now), gain, seconds);
	// 멈추기로 했던 채널을 다시 올리면 그 멈춤도 거둔다.
	FMOD_Channel_SetDelay(channel, 0, stop ? end : 0, stop ? TRUE : FALSE);
}

unsigned long long SoundManager::Fade(FMOD_CHANNEL* channel, Float from, Float to, Float seconds) const
{
	unsigned long long now = 0;
	FMOD_Channel_GetDSPClock(channel, nullptr, &now);
	int rate = 0;
	FMOD_System_GetSoftwareFormat(m_System, &rate, nullptr, nullptr);
	const unsigned long long end = now + static_cast<unsigned long long>(std::max(seconds, 0.f) * static_cast<Float>(rate));
	FMOD_Channel_RemoveFadePoints(channel, 0, ULLONG_MAX);
	FMOD_Channel_AddFadePoint(channel, now, from);
	FMOD_Channel_AddFadePoint(channel, end, to);
	return end;
}

HRESULT SoundManager::Clear_SoundSources()
{
	for (auto& [handle, channel] : m_Loops3D)
		FMOD_Channel_Stop(channel);
	m_Loops3D.clear();

	for (auto& Mypair : m_SoundSource)
	{
		for (FMOD_SOUND* sound : Mypair.second.sounds)
		{
			FMOD_Sound_Release(sound);
		}
	}
	m_SoundSource.clear();

	return S_OK;
}



Unique<SoundManager> SoundManager::Create()
{
	auto instance = make_unique<SoundManager>();

	if (FAILED(instance->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Clone : SoundManager");
		return nullptr;
	}

	return instance;
}

NS_END
