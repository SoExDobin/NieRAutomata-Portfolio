#pragma once
#include "StateEm0000.h"

NS_BEGIN(Client)

// Stands in place until its sight finds the player. A dormant statue (Emb001) holds its pose instead and plays the
// wake clip once it is woken. A park visitor (Monster::Is_Passive) strolls about where it was spawned instead,
// throwing paper confetti (PAPER_THROW) wherever it stops, until something hits it.
class CLIENT_DLL StateEm0000_Idle final : public StateEm0000
{
public:
	explicit StateEm0000_Idle(const wstring& tag, const Shared<Em0000>& owner);
	~StateEm0000_Idle() override = default;

public:
	Bool StateEnterInvoke() override;
	void Update(Float timeDelta) override;

private:
	void Update_Visit(Float timeDelta);

private:
	enum class VISIT { REST, WALK, THROW };
	VISIT m_Visit{ VISIT::REST };
	Float m_VisitTime{};		// REST: left to wait; WALK, THROW: spent so far
	Vector3 m_VisitPoint{};

public:
	static Shared<StateEm0000_Idle> Create(const wstring& tag, const Shared<Em0000>& owner);
};

NS_END
