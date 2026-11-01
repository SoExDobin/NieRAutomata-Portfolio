#include "pch.h"
#include "UIManager.h"
#include "ClientApp.h"
#include "CommunicationUI.h"
#include "WP3000Body.h"
#include <cmath>

namespace
{
    constexpr auto VoiceChannel = SOUNDCHANNEL::COMMUNICATION_VOICE;
    constexpr auto SignalChannel = SOUNDCHANNEL::COMMUNICATION_SIGNAL;
    const filesystem::path CommunicationPath = L"../../../Client/bin/resources/UI/Communication";
    constexpr std::array<const Char*, ETOI(COMMUNICATION_CUE::END)> ScenarioNames{
        "CityBriefing", "Em8000Encounter", "Em8000Rage", "AfterEm8000", "CityHandover",
        "ParkBriefing", "ParkEntrance", "RabbitLook", "RabbitEncounter", "RabbitTaunt", "RabbitSulk", "AfterRabbit", "ParkBridge",
        "Em3000Encounter", "Em3000Combat", "Em3000Bullet", "Em3000Groggy", "Em3000Rage", "Em3000Puppets", "Em3000Critical",
        "AfterEm3000" };
}

UIManager::~UIManager() { Cancel_Communication(); }

HRESULT UIManager::Ready_Resources()
{
    if (m_ResourcesReady) return S_OK;
    if (m_ResourcesFailed) return E_FAIL;
    m_ResourcesFailed = true; // A failure is logged once per ClientApp, never retried each frame.
    const auto loadTexture = [](const tChar* file, const tChar* tag) {
        return GAME_INSTANCE->Load_Texture(ETOI(LEVEL::STATIC), (CommunicationPath / file).c_str(), 1, tag);
    };
    if (FAILED(loadTexture(L"Horizontal.dds", L"Comm_Horizontal")) ||
        FAILED(loadTexture(L"Vertical.dds", L"Comm_Vertical")) ||
        FAILED(loadTexture(L"Header.dds", L"Comm_Header")) ||
        FAILED(loadTexture(L"Calling.dds", L"Comm_Calling")) ||
        FAILED(loadTexture(L"Connected.dds", L"Comm_Connected")) ||
        FAILED(loadTexture(L"SoundOnly.dds", L"Comm_SoundOnly")) ||
        FAILED(loadTexture(L"Portrait_6O_Model.png", L"Comm_Portrait_6O")) ||
        FAILED(loadTexture(L"Portrait_9S.png", L"Comm_Portrait_9S")) ||
        FAILED(loadTexture(L"Portrait_Commander_Model.png", L"Comm_Portrait_Commander"))) return E_FAIL;
    if (FAILED(loadTexture(L"Ring.dds", L"Comm_Ring")) ||
        FAILED(loadTexture(L"Ticks.dds", L"Comm_Ticks")) ||
        FAILED(loadTexture(L"PodBeam.png", L"Comm_PodBeam"))) return E_FAIL;
    // Reuse the HUD's tiny solids without changing the authored workbook/cache.
    const auto staticTextureTags = GAME_INSTANCE->Get_TextureTags(ETOI(LEVEL::STATIC));
    for (const auto& [file, tag] : { pair{ L"HUD_Black.png", L"UI_HUD_Black" }, pair{ L"HUD_Ivory.png", L"UI_HUD_Ivory" } })
        if (std::find(staticTextureTags.begin(), staticTextureTags.end(), tag) == staticTextureTags.end() && FAILED(GAME_INSTANCE->Load_Texture(
            ETOI(LEVEL::STATIC), (filesystem::path(L"../../../Client/bin/resources/Texture/UI/HUD") / file).c_str(), 1, tag))) return E_FAIL;
    // ClientApp registers the font with the others on the main thread.
    if (GAME_INSTANCE->Measure_Font(L"Nier_Communication_32", L"H", 0.f).x <= 0.f)
        return E_FAIL;
    for (const auto& [file, tag] : { pair{ L"Call.wav", L"Comm_Call" }, pair{ L"Open.wav", L"Comm_Open" }, pair{ L"Close.wav", L"Comm_Close" } })
        if (!GAME_INSTANCE->Is_SoundLoaded(tag) && FAILED(GAME_INSTANCE->Load_Sound(tag, { (CommunicationPath / file).wstring() })))
            LOG_WARN(L"[Communication] Missing optional signal {}", tag);
    // Voice/<audio event>__<media id>.wav registers under its audio event, the voiceTag Scenario.json names.
    std::error_code error;
    for (const auto& entry : filesystem::directory_iterator(CommunicationPath / L"Voice", error))
    {
        const wstring stem = entry.path().stem().wstring();
        const size_t split = stem.find(L"__");
        if (entry.path().extension() != L".wav" || split == wstring::npos) continue;
        const wstring tag = stem.substr(0, split);
        if (!GAME_INSTANCE->Is_SoundLoaded(tag) && FAILED(GAME_INSTANCE->Load_Sound(tag, { entry.path().wstring() })))
            LOG_WARN(L"[Communication] Voice {} failed to load; using subtitle hold", tag);
    }
    if (error) LOG_WARN(L"[Communication] Voice folder unreadable; scenarios use subtitle holds");
    try
    {
        ifstream input(CommunicationPath / L"Scenario.json");
        nlohmann::json data;
        input >> data;
        for (size_t i = 0; i < ScenarioNames.size(); ++i)
        {
            const auto& cue = data.at(ScenarioNames[i]);
            auto& desc = m_Scenarios[i];
            desc.portraitTag = Helper::To_wString(cue.value("portraitTag", string{}));
            const auto& lines = cue.at("lines");
            if (!lines.is_array() || lines.empty() || lines.size() > 32) return E_FAIL;
            for (const auto& item : lines)
            {
                CommunicationLine line;
                line.speaker = Helper::To_wString(item.at("speaker").get<string>());
                line.text = Helper::To_wString(item.at("text").get<string>());
                line.voiceTag = Helper::To_wString(item.value("voiceTag", string{}));
                line.holdSeconds = item.value("holdSeconds", 4.f);
                line.showPodUI = item.value("showPodUI", true);
                line.soundOnly = item.value("soundOnly", false);
                if (line.text.empty() || line.text.size() > 256 || !std::isfinite(line.holdSeconds) ||
                    line.holdSeconds < 1.f || line.holdSeconds > 30.f) return E_FAIL;
                desc.lines.push_back(std::move(line));
            }
        }
    }
    catch (const std::exception& error)
    {
        LOG_ERROR(L"[Communication] Invalid Scenario.json: {}", Helper::To_wString(error.what()));
        return E_FAIL;
    }
    m_ResourcesReady = true;
    m_ResourcesFailed = false;
    return S_OK;
}

Bool UIManager::Play_Communication(const CommunicationDesc& desc)
{
    const uint32 level = GAME_INSTANCE->Get_CurrentLevelIndex();
    if (m_State != State::IDLE || (level != ETOI(LEVEL::GAMEPLAY) && level != ETOI(LEVEL::AMUSEMENT_PARK)) ||
        desc.lines.empty() || desc.lines.size() > 32) return false;
    for (const auto& line : desc.lines)
        if (line.text.empty() || line.text.size() > 256 || line.speaker.size() > 64 ||
            !std::isfinite(line.holdSeconds) || line.holdSeconds < 1.f || line.holdSeconds > 30.f) return false;
    if (FAILED(Ready_Resources())) return false;
    const auto view = GAME_INSTANCE->Instantiate<CommunicationUI>(level);
    if (!view) return false;
    if (FAILED(view->Ready_Layout(desc.pod.lock(), desc.portraitTag)))
    {
        view->Destroy_Subtree();
        return false;
    }
    m_Level = static_cast<LEVEL>(level);
    m_Current = desc;
    m_CurrentCue = COMMUNICATION_CUE::END;
    m_LineIndex = 0;
    m_View = view;
    Begin_Line();
    return true;
}

void UIManager::Play_Signal(const tChar* tag)
{
    GAME_INSTANCE->StopSound(SignalChannel);
    if (GAME_INSTANCE->Is_SoundLoaded(tag)) GAME_INSTANCE->PlaySoundFX(tag, SignalChannel, 0.35f);
}
void UIManager::Enter_State(State state) { m_State = state; m_Elapsed = 0.f; }
void UIManager::Begin_Line()
{
    const auto view = m_View.lock();
    if (!view) { Finish_Communication(); return; }
    const auto& line = m_Current.lines[m_LineIndex];
    view->Set_PodVisible(line.showPodUI);
    view->Show_Portrait(Is_Remote(line));
    const Bool panelOpen = m_LineIndex > 0 && m_Current.lines[m_LineIndex - 1].showPodUI;
    if (!line.showPodUI)
    {
        Start_Line();
        return;
    }
    // Only a call from someone else (an operator, the commander) rings; the Pod beside the player just speaks.
    if (!Is_Remote(line))
    {
        if (!panelOpen) view->Show_Connected();
        Start_Line();
        return;
    }
    // Rings once per conversation, also when the call comes in over the Pod's own report; after the player's
    // reply the window reopens on the live link.
    const auto begin = m_Current.lines.begin();
    if (std::any_of(begin, begin + m_LineIndex, [this](const CommunicationLine& earlier) { return Is_Remote(earlier); }))
    {
        if (panelOpen)
        {
            Start_Line();
            return;
        }
        view->Show_Connected();
        Enter_State(State::OPENING);
        Play_Signal(L"Comm_Open");
    }
    else
    {
        view->Show_Calling();
        Enter_State(State::CALLING);
        Play_Signal(L"Comm_Call");
    }
}
void UIManager::Start_Line()
{
    const auto view = m_View.lock();
    if (!view) { Finish_Communication(); return; }
    const auto& line = m_Current.lines[m_LineIndex];
    view->Set_PodVisible(line.showPodUI);
    view->Show_Line(line.speaker, line.text);
    GAME_INSTANCE->StopSound(VoiceChannel);
    m_HasVoice = !line.voiceTag.empty() && GAME_INSTANCE->Is_SoundLoaded(line.voiceTag);
    if (m_HasVoice)
    {
        // Voice files are levelled to -16 LUFS (normalize_voices.py), above the ~-19 LUFS level BGM.
        GAME_INSTANCE->PlaySoundFX(line.voiceTag, VoiceChannel, 1.f);
        m_HasVoice = GAME_INSTANCE->Is_SoundPlaying(VoiceChannel);
    }
    else if (!line.voiceTag.empty()) LOG_WARN(L"[Communication] Unloaded voice {}; using subtitle hold", line.voiceTag);
    Enter_State(State::LINE);
}
Bool UIManager::Is_CommunicationPlaying() const { return m_State != State::IDLE || !m_Pending.empty(); }
void UIManager::Finish_Communication()
{
    // Short battle exchanges leave room for combat sounds. Phase changes can bypass this pause, and so does 9S's call:
    // it explains the puppets the rage scream just brought down, so it follows the rage exchange at once.
    if (m_CurrentCue >= COMMUNICATION_CUE::EM3000_ENCOUNTER && m_CurrentCue < COMMUNICATION_CUE::AFTER_EM3000 &&
        (m_Pending.empty() || m_Pending.front() != COMMUNICATION_CUE::EM3000_PUPPETS))
        m_ScenarioDelay = 4.f;
    GAME_INSTANCE->StopSound(VoiceChannel);
    GAME_INSTANCE->StopSound(SignalChannel);
    if (const auto view = m_View.lock()) view->Destroy_Subtree();
    m_View.reset();
    m_Current = {};
    m_CurrentCue = COMMUNICATION_CUE::END;
    m_HasVoice = false;
    Enter_State(State::IDLE);
}
void UIManager::Cancel_Communication()
{
    Finish_Communication();
    m_Pending.clear();
    m_ScenarioDelay = 0.f;
}
void UIManager::End_Level(LEVEL level)
{
    if (m_Level != level) return;
    Cancel_Communication();
    // Leaving the level (or an Editor Stop) rearms its cues for the next visit or Play.
    const size_t first = level == LEVEL::AMUSEMENT_PARK ? ETOI(COMMUNICATION_CUE::PARK_BRIEFING) : 0;
    const size_t last = level == LEVEL::AMUSEMENT_PARK ? m_Queued.size() : ETOI(COMMUNICATION_CUE::PARK_BRIEFING);
    for (size_t i = first; i < last; ++i) m_Queued[i] = false;
    m_Level = LEVEL::LEVEL_END;
}
Shared<GameObject> UIManager::Find_Pod() const
{
    if (m_Level == LEVEL::LEVEL_END) return nullptr;
    for (const auto& [id, object] : GAME_INSTANCE->Get_GameObjects(ETOI(m_Level)))
        if (object && !object->Is_Destroy() && object->Is_Active() && Cast<WP3000Body>(object))
            return object;
    return nullptr;
}
void UIManager::Queue_Scenario(COMMUNICATION_CUE cue)
{
    const auto index = static_cast<size_t>(cue);
    if (index >= m_Queued.size() || m_Queued[index]) return;
    const auto level = static_cast<LEVEL>(GAME_INSTANCE->Get_CurrentLevelIndex());
    const Bool parkCue = index >= ETOI(COMMUNICATION_CUE::PARK_BRIEFING);
    if ((parkCue && level != LEVEL::AMUSEMENT_PARK) || (!parkCue && level != LEVEL::GAMEPLAY)) return;
    // Boss state callbacks may precede the level's first UI update.
    if (m_Level != level)
    {
        Cancel_Communication();
        m_Level = level;
    }
    const Bool rabbitCue = cue >= COMMUNICATION_CUE::RABBIT_LOOK && cue <= COMMUNICATION_CUE::AFTER_RABBIT;
    const auto first = rabbitCue ? COMMUNICATION_CUE::RABBIT_LOOK :
        (parkCue ? COMMUNICATION_CUE::PARK_BRIEFING : COMMUNICATION_CUE::CITY_BRIEFING);
    const auto rage = rabbitCue ? COMMUNICATION_CUE::RABBIT_ENCOUNTER :
        (parkCue ? COMMUNICATION_CUE::EM3000_RAGE : COMMUNICATION_CUE::EM8000_RAGE);
    const auto after = rabbitCue ? COMMUNICATION_CUE::AFTER_RABBIT :
        (parkCue ? COMMUNICATION_CUE::AFTER_EM3000 : COMMUNICATION_CUE::AFTER_EM8000);
    if (parkCue && cue < COMMUNICATION_CUE::AFTER_EM3000 && m_Queued[ETOI(COMMUNICATION_CUE::AFTER_EM3000)]) return;
    if ((cue < after && m_Queued[ETOI(after)]) || (cue < rage && m_Queued[ETOI(rage)])) return;
    const Bool phaseChange = cue == rage || cue == after || cue == COMMUNICATION_CUE::EM3000_ENCOUNTER ||
        cue == COMMUNICATION_CUE::EM3000_GROGGY;
    if (phaseChange)
    {
        // Optional rabbit speech retires only its own group; the bridge/mission queue survives.
        if (cue == after && !rabbitCue) Cancel_Communication();
        else if (m_CurrentCue >= first && m_CurrentCue < cue) Finish_Communication();
        m_Pending.erase(std::remove_if(m_Pending.begin(), m_Pending.end(), [first, cue](COMMUNICATION_CUE pending) {
            return pending >= first && pending < cue;
        }), m_Pending.end());
        for (size_t i = ETOI(first); i < index; ++i) m_Queued[i] = true;
        m_ScenarioDelay = 0.f;
    }
    m_Queued[index] = true;
    if (cue == COMMUNICATION_CUE::AFTER_RABBIT) m_Pending.push_front(cue);
    else m_Pending.push_back(cue);
    if (cue == COMMUNICATION_CUE::AFTER_EM8000) m_ScenarioDelay = 1.2f;
}
void UIManager::Update(LEVEL level, Float timeDelta, Bool entryReady)
{
    if (m_Level != level)
    {
        Cancel_Communication();
        m_Level = level;
    }
    const Float dt = std::isfinite(timeDelta) ? std::max(timeDelta, 0.f) : 0.f;
    if (!entryReady) return;
    m_ScenarioDelay = std::max(m_ScenarioDelay - dt, 0.f);
    if (m_State == State::IDLE)
    {
        if (m_Pending.empty() || m_ScenarioDelay > 0.f) return;
        if (FAILED(Ready_Resources()))
        {
            LOG_ERROR(L"[Communication] Resources unavailable; scenario communications disabled for this ClientApp");
            m_Pending.clear();
            return;
        }
        const auto cue = m_Pending.front();
        auto desc = m_Scenarios[static_cast<size_t>(cue)];
        desc.pod = Find_Pod();
        if (desc.pod.expired() && std::any_of(desc.lines.begin(), desc.lines.end(), [](const CommunicationLine& line) { return line.showPodUI; }))
            return; // Remote speech waits for the actual Pod; player-only subtitles need no anchor.
        m_Pending.pop_front();
        if (Play_Communication(desc)) m_CurrentCue = cue;
        else LOG_ERROR(L"[Communication] Failed to start runtime view");
        return;
    }
    const auto view = m_View.lock();
    if (!view || view->Is_Destroy()) { Finish_Communication(); return; }
    m_Elapsed += dt;
    switch (m_State)
    {
    case State::CALLING:
        if ((m_Elapsed >= 1.6f && !GAME_INSTANCE->Is_SoundPlaying(SignalChannel)) || m_Elapsed >= 3.f)
        { view->Show_Connected(); Enter_State(State::OPENING); Play_Signal(L"Comm_Open"); }
        break;
    case State::OPENING:
        if ((m_Elapsed >= 0.35f && !GAME_INSTANCE->Is_SoundPlaying(SignalChannel)) || m_Elapsed >= 3.f) Start_Line();
        break;
    case State::LINE:
    {
        const auto& line = m_Current.lines[m_LineIndex];
        const Bool finished = m_HasVoice ? m_Elapsed >= 0.5f && !GAME_INSTANCE->Is_SoundPlaying(VoiceChannel) : m_Elapsed >= line.holdSeconds;
        if (finished || m_Elapsed >= 120.f)
        {
            GAME_INSTANCE->StopSound(VoiceChannel);
            view->Hide_Line();
            ++m_LineIndex;
            // A2 reacts to the park first; the panorama pauses the following lines through the level's UI gate.
            if (m_CurrentCue == COMMUNICATION_CUE::PARK_ENTRANCE && m_LineIndex == 1)
                if (const auto client = ClientApp::GetInstance())
                    client->Play_CameraEvent(CAMERA_EVENT::PARK_ARRIVAL);
            // Close the actual window before a player-only reply, or at the end of the conversation.
            if (line.showPodUI && (m_LineIndex == m_Current.lines.size() || !m_Current.lines[m_LineIndex].showPodUI))
            {
                Play_Signal(L"Comm_Close");
                view->Set_CloseProgress(0.f);
                Enter_State(State::CLOSING);
            }
            else if (m_LineIndex < m_Current.lines.size()) Enter_State(State::GAP);
            else Finish_Communication();
        }
        break;
    }
    case State::GAP:
    {
        // A speaker going on pauses for breath; a reply from someone else waits longer.
        const Bool sameSpeaker = m_Current.lines[m_LineIndex].speaker == m_Current.lines[m_LineIndex - 1].speaker;
        if (m_Elapsed >= (sameSpeaker ? 0.5f : 0.9f)) Begin_Line();
        break;
    }
    case State::CLOSING:
        view->Set_CloseProgress(m_Elapsed / 0.7f);
        if ((m_Elapsed >= 0.7f && !GAME_INSTANCE->Is_SoundPlaying(SignalChannel)) || m_Elapsed >= 2.f)
        {
            view->Set_PodVisible(false);
            if (m_LineIndex < m_Current.lines.size()) Enter_State(State::GAP);
            else Finish_Communication();
        }
        break;
    default: break;
    }
}
