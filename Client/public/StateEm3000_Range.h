#pragma once
#include "StateEm3000.h"

NS_BEGIN(Client)

class EmLaser;

// Phase 1 ranged attack: start clip, attack loop, end clip of one pattern. The first visits go through the patterns
// in order, then they are picked at random.
class CLIENT_DLL StateEm3000_Range final : public StateEm3000
{
public:
	explicit StateEm3000_Range(const wstring& tag, const Shared<Em3000>& owner);
	~StateEm3000_Range() override = default;

private:
	HRESULT Initialize() override;

public:
	Bool StateEnterInvoke() override;
	void Update(Float timeDelta) override;
	void StateExitInvoke() override;

private:
	enum class PATTERN : uint8
	{
		LASER,				// danmak 1 clips: sixteen floor beams sweep one full turn (EmLaser)
		HOMING,				// danmak 2 clips: homing lasers from the chest (Em3000Homing)
		RING,				// 0560 alone (it holds its own charge): rings fall around the boss (EmRing)
		WAVE_BARRAGE,		// danmak 1 (05a0-05a2): rising, bobbing bullets from the thirteen gun ports of the base drum
		STRAIGHT_BARRAGE,	// danmak 2 (05a5-05a7): straight bullets from the ports
		HOWITZER,			// danmak 3 (05c0-05c2): shells lobbed on the 05c1 notifies
		RISING_BARRAGE,		// danmak 1, the spin looped for RisingTime: bullets that climb ever higher as they fly out
		END
	};
	struct CLIPS
	{
		ANIM start;
		ANIM loop;
		ANIM end;
		Bool hasEnd{ true };
	};

	CLIPS Get_Clips(PATTERN pattern) const;
	void Begin_Attack() const;
	Bool Is_AttackOver() const;
	void End_Attack() const;

	Weak<EmLaser> m_Laser{};

	PATTERN m_Pattern{};
	uint32 m_Visits{};
	Float m_LoopTime{};		// seconds in the loop clip

	static constexpr Float RisingTime = 7.f;	// two turns of 05a1

public:
	static constexpr uint32 PatternCount = ETOI(PATTERN::END);	// its first visits take the patterns in order
	static Shared<StateEm3000_Range> Create(const wstring& tag, const Shared<Em3000>& owner);
};

NS_END
