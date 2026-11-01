#include "pch.h"
#include "PlayerStates.h"
#include "2B.h"

// --- IDLE ---
void C2B_Idle::Enter()
{
	// TODO: Play Idle Animation
}

void C2B_Idle::Update(Float timeDelta)
{
	// TODO: Check for input to transition to MOVE
}

void C2B_Idle::Late_Update(Float timeDelta)
{
}

void C2B_Idle::Exit()
{
}

// --- MOVE ---
void C2B_Move::Enter()
{
	// TODO: Play Move Animation
}

void C2B_Move::Update(Float timeDelta)
{
	// TODO: Update position, check for input to transition to IDLE
}

void C2B_Move::Late_Update(Float timeDelta)
{
}

void C2B_Move::Exit()
{
}
