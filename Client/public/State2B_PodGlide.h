#pragma once
#include "State2B.h"

NS_BEGIN(Client)

// Holding Space in the air (from the jump, the double jump or the air combo): the player hangs from the pod and descends
// slowly.
class CLIENT_DLL State2B_PodGlide final : public State2B
{
public:
	explicit State2B_PodGlide(const wstring& tag, const Shared<Pl0000>& owner);
	~State2B_PodGlide() override = default;

public:
	Bool StateEnterInvoke() override;
	void Update(Float timeDelta) override;
	void Late_Update(Float timeDelta) override;
	void StateExitInvoke() override;

private:
	const Float n_GlideFallSpeed = { 5.f };
	const Float n_GlideMoveSpeed = { 7.5f };
	// pl0000_001f / wp3000_001f grab the pod in frames 0~90; the hanging part from frame 100 of 1138 repeats.
	const Float n_GlideLoopStart = { 100.f / 1138.f };

	Float m_SavedGravity{ 0.f };
	Bool m_IsDescending{ false };

public:
	static Shared<State2B_PodGlide> Create(const wstring& tag, const Shared<Pl0000>& owner);
};

NS_END
