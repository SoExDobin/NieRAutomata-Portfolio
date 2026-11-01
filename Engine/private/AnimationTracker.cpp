#include "AnimationTracker.h"

#include "Game.h"
#include "SpdLogger.h"

AnimationTracker::AnimationTracker() : Component{} {}
AnimationTracker::AnimationTracker(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: Component{device, context} {}
AnimationTracker::AnimationTracker(const AnimationTracker& rhs)
	: Component{rhs}{}

HRESULT AnimationTracker::Initialize_Prototype()
{
	return Component::Initialize_Prototype();
}

HRESULT AnimationTracker::Initialize(void* arg)
{
	return Component::Initialize(arg);
}


void AnimationTracker::Add_Notify(uint32 animIndex, const ANIMATION_TRACKER_NOTIFY& notify)
{
	m_Notifies[animIndex].push_back(notify);
	m_States[animIndex].push_back(false);
}

void AnimationTracker::Add_Notify(uint32 animIndex, std::initializer_list<ANIMATION_TRACKER_NOTIFY> notifies)
{
	for (const auto& notify : notifies)
		Add_Notify(animIndex, notify);
}

void AnimationTracker::Update(uint32 animIndex, Float progress)
{
	if (m_IsDestroy || !m_IsActive) return;

	// 끝난 클립의 RANGE를 닫고 모든 Notify를 다시 발동할 수 있게 되돌린다.
	auto rewind = [this](uint32 index)
	{
		if (index == UINT_MAX || !m_Notifies.contains(index))
			return;
		auto& notifies = m_Notifies[index];
		auto& states = m_States[index];
		for (uint32 i = 0; i < notifies.size(); ++i)
		{
			if (notifies[i].type == NOTIFY_TYPE::RANGE && states[i] && notifies[i].onEndNotify)
				notifies[i].onEndNotify();
			states[i] = false;
		}
	};

	if (m_PrevAnimIndex != animIndex)
	{
		rewind(m_PrevAnimIndex);
		m_PrevAnimIndex = animIndex;
	}
	else if (progress < m_PrevProgress)
	{
		// 루프 클립이 처음으로 돌아왔다: 걷기·달리기 발소리처럼 매 바퀴 다시 울려야 한다.
		rewind(animIndex);
	}
	m_PrevProgress = progress;

	if (false == m_Notifies.contains(animIndex)) return;

	auto& animNotifies = m_Notifies[animIndex];
	auto& notifyStates = m_States[animIndex];

	// 설정을 하고 도달 이후 플레그를 종료
	for (uint32 i = 0; i < animNotifies.size(); ++i)
	{
		const auto& notify = animNotifies[i];

		if (NOTIFY_TYPE::POINT == notify.type)
		{

			if (false == notifyStates[i] && progress >= notify.invokeProgress)
			{
				if (notify.onNotify)
					notify.onNotify();

				notifyStates[i] = true;
			}
		}
		else
		{
			Bool inRange = (progress >= notify.invokeProgress && progress <= notify.endProgress);

			if (false == notifyStates[i] && inRange)
			{
				if (notify.onNotify)
					notify.onNotify();

				notifyStates[i] = true;
			}
			else if (false == inRange && notifyStates[i])
			{
				if (notify.onEndNotify)
					notify.onEndNotify();

				notifyStates[i] = false;
			}
		}
	}
}

void AnimationTracker::Reset()
{
	for (auto& [animIndex, states] : m_States)
	{
		for (uint32 i = 0; i < states.size(); ++i)
		{
			if (states[i] && m_Notifies[animIndex].size())
			{
				if (m_Notifies[animIndex][i].type == NOTIFY_TYPE::RANGE &&
					m_Notifies[animIndex][i].onEndNotify)
					m_Notifies[animIndex][i].onEndNotify();
			}
			states[i] = false;
		}
	}
	m_PrevAnimIndex = UINT32_MAX;
}

void AnimationTracker::Clear()
{
	Reset();
	m_Notifies.clear();
	m_States.clear();
}

Bool AnimationTracker::Is_ActiveNotify(uint32 animIndex, const wstring& notifyTag) const
{
	auto it = m_Notifies.find(animIndex);
	if (it == m_Notifies.end()) return false;
	auto stateIt = m_States.find(animIndex);
	if (stateIt == m_States.end()) return false;
	for (size_t i = 0; i < it->second.size(); ++i)
	{
		if (it->second[i].notifyTag == notifyTag)
			return stateIt->second[i];
	}
	return false;
}

Shared<AnimationTracker> AnimationTracker::CreatePrototype()
{
	auto prototype = make_shared<AnimationTracker>(GAME_INSTANCE->Get_Device(), GAME_INSTANCE->Get_Context());

	if (FAILED(prototype->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : AnimationTracker");
		return nullptr;
	}

	return prototype;
}

Shared<AnimationTracker> AnimationTracker::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
	auto prototype = make_shared<AnimationTracker>(device, context);

	if (FAILED(prototype->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : AnimationTracker");
		return nullptr;
	}

	return prototype;
}
