#pragma once
#include "State2B.h"

NS_BEGIN(Client)

// In the air in front of a wall whose top is in reach (Pl0000's Ledge Inspector values), the player grabs the edge and
// climbs onto it with pl0000_0080. Entering checks for the ledge and refuses when there is none, so the air states just
// ask for this state every frame. The clip's rise is fitted to the ledge's height and the step in follows its root.
class CLIENT_DLL State2B_Ledge final : public State2B
{
public:
	explicit State2B_Ledge(const wstring& tag, const Shared<Pl0000>& owner);
	~State2B_Ledge() override = default;

public:
	Bool StateEnterInvoke() override;
	void Update(Float timeDelta) override;
	void Late_Update(Float timeDelta) override;
	void StateExitInvoke() override;

private:
	// Fills the climb from the static world ahead of the feet; false when nothing there can be climbed.
	Bool Find_Ledge();

private:
	Float m_TopY{};			// the ledge's top surface
	Float m_Height{};		// top above the feet when grabbed
	Vector3 m_Facing{};		// into the wall
	Float m_Pull{};			// how far the feet are drawn to the wall during the rise
	Bool m_IsOnTop{ false };

public:
	static Shared<State2B_Ledge> Create(const wstring& tag, const Shared<Pl0000>& owner);
};

NS_END
