#pragma once
#include "Client_Define.h"
#include "Engine_Define.h"
#include <array>
#include <deque>

NS_BEGIN(Engine)
class GameObject;
NS_END

NS_BEGIN(Client)
class CommunicationUI;

struct CommunicationLine
{
    wstring speaker;
    wstring text;
    wstring voiceTag; // Japanese recording tag; empty uses the bounded subtitle hold.
    Float holdSeconds{ 4.f };
    Bool showPodUI{ true }; // Player speech uses subtitles/voice without a communication panel or signals.
    Bool soundOnly{}; // A Pod line inside an operator call shows SOUND ONLY instead of the operator's portrait.
};

struct CommunicationDesc
{
    Weak<GameObject> pod;
    wstring portraitTag; // Optional baked portrait texture; empty uses SOUND ONLY.
    vector<CommunicationLine> lines;
};

// Each level's cues run entry → encounter → rage → after-boss; PARK_BRIEFING starts the Level 4 group.
// CITY_BRIEFING (0) and CITY_HANDOVER (4, the Level 4 exit call) come from an AreaTrigger's Communication field;
// PARK_BRIEFING (5) and PARK_ENTRANCE (6) come from separate Level 4 AreaTriggers.
// Scenes store these numbers (AreaTrigger Communication, MonsterSpawner Cue); Level 4's Comm_ParkBridge holds PARK_BRIDGE (12),
// so a cue added before it renumbers that trigger too.
// RABBIT_TAUNT/RABBIT_SULK are the rabbit's own lines at 75%/50% HP; EM3000_BULLET is em3000's at her first barrage.
enum class COMMUNICATION_CUE { CITY_BRIEFING, EM8000_ENCOUNTER, EM8000_RAGE, AFTER_EM8000, CITY_HANDOVER,
    PARK_BRIEFING, PARK_ENTRANCE, RABBIT_LOOK, RABBIT_ENCOUNTER, RABBIT_TAUNT, RABBIT_SULK, AFTER_RABBIT, PARK_BRIDGE,
    EM3000_ENCOUNTER, EM3000_COMBAT, EM3000_BULLET, EM3000_GROGGY, EM3000_RAGE, EM3000_PUPPETS, EM3000_CRITICAL,
    AFTER_EM3000, END };

// ClientApp owns this manager. It has no independent singleton or input/time-scale authority.
class CLIENT_DLL UIManager final
{
public:
    UIManager() = default;
    ~UIManager();
    Bool Play_Communication(const CommunicationDesc& desc);
    void Cancel_Communication();
    Bool Is_CommunicationPlaying() const;
    void Queue_Scenario(COMMUNICATION_CUE cue);
    // The cue has been queued (or superseded) during this level visit; spawners wait on it.
    Bool Is_ScenarioQueued(COMMUNICATION_CUE cue) const { return cue < COMMUNICATION_CUE::END && m_Queued[static_cast<size_t>(cue)]; }
    void Update(LEVEL level, Float timeDelta, Bool entryReady);
    void End_Level(LEVEL level);
    // Communication textures, signals, voices and Scenario.json. The first loader calls it so the first call does not stall.
    HRESULT Ready_Resources();

private:
    enum class State { IDLE, CALLING, OPENING, LINE, GAP, CLOSING };
    Shared<GameObject> Find_Pod() const;
    void Begin_Line();
    void Start_Line();
    void Enter_State(State state);
    void Play_Signal(const tChar* tag);
    // Someone other than the player's Pod: the operator portrait is shown and the call signals ring.
    Bool Is_Remote(const CommunicationLine& line) const { return line.showPodUI && !m_Current.portraitTag.empty() && !line.soundOnly; }
    void Finish_Communication();

    LEVEL m_Level{ LEVEL::LEVEL_END };
    Bool m_ResourcesReady{};
    Bool m_ResourcesFailed{};
    std::array<CommunicationDesc, ETOI(COMMUNICATION_CUE::END)> m_Scenarios{};
    std::array<Bool, ETOI(COMMUNICATION_CUE::END)> m_Queued{};
    std::deque<COMMUNICATION_CUE> m_Pending;
    Float m_ScenarioDelay{};
    CommunicationDesc m_Current;
    COMMUNICATION_CUE m_CurrentCue{ COMMUNICATION_CUE::END };
    Weak<CommunicationUI> m_View;
    size_t m_LineIndex{};
    State m_State{ State::IDLE };
    Float m_Elapsed{};
    Bool m_HasVoice{};
};
NS_END
