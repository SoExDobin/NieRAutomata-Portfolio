#include "pch.h"
#include "Pl0000Input.h"

#include <Game.h>

Pl0000Input::Pl0000Input() : ScriptComponent{}
{
	ZeroMemory(&m_MouseMovement, sizeof(LONG) * ETOI(DIMB::END));
}
Pl0000Input::Pl0000Input(const ComPtr<ID3D11Device> &device, const ComPtr<ID3D11DeviceContext> &context)
	: ScriptComponent{device, context}
{
	ZeroMemory(&m_MouseMovement, sizeof(LONG) * ETOI(DIMB::END));
}
Pl0000Input::Pl0000Input(const Pl0000Input &rhs)
	: ScriptComponent{rhs}
{
	ZeroMemory(&m_MouseMovement, sizeof(LONG) * ETOI(DIMB::END));
}

HRESULT Pl0000Input::Initialize_Prototype()
{
	return ScriptComponent::Initialize_Prototype();
}

HRESULT Pl0000Input::Initialize(void *arg)
{
	return ScriptComponent::Initialize(arg);
}

void Pl0000Input::On_Disable()
{
	ScriptComponent::On_Disable();
}

void Pl0000Input::On_Enable()
{
	ScriptComponent::On_Enable();
}

// ===================================================================
// 매 프레임 갱신
// ===================================================================
void Pl0000Input::Update_Pl0000_InputState(Float timeDelta)
{
	if (!Is_Active() || !GAME_INSTANCE->Get_InputEnabled())
	{
		for (auto& info : m_KeyInfos) info = INPUT_INFO{};
		for (auto& info : m_MouseInfos) info = INPUT_INFO{};
		ZeroMemory(m_MouseMovement, sizeof(m_MouseMovement));
		Clear_MouseComboQueue();
		return;
	}

	// --- 키보드 ---
	for (uint32 i = 0; i < KEY_MAX; ++i)
	{
		Byte rawState = GAME_INSTANCE->Get_DIKeyState(static_cast<uByte>(i));
		Update_Pl0000_KeyState(rawState, m_KeyInfos[i], timeDelta);
	}

	// --- 마우스 버튼 + 콤보 큐 ---
	for (uint32 i = 0; i < ETOI(DIMB::END); ++i)
	{
		DIMB buttonID = static_cast<DIMB>(i);
		Byte rawState = GAME_INSTANCE->Get_DIMouseState(buttonID);
		Update_Pl0000_KeyState(rawState, m_MouseInfos[i], timeDelta);

		// DOWN 이 발생한 프레임에만 큐에 적재
		if (m_MouseInfos[i].state == KEY_STATE::DOWN)
		{
			m_MouseComboQueue.push_back(buttonID);
			m_ComboTimer = 0.f;
		}
	}

	// --- 콤보 타임아웃 ---
	if (!m_MouseComboQueue.empty())
	{
		m_ComboTimer += timeDelta;
		if (m_ComboTimer > m_ComboTimeout)
		{
			m_MouseComboQueue.clear();
		}
	}

	// --- 마우스 이동 ---
	for (uint32 i = 0; i < ETOI(DIMM::END); ++i)
	{
		DIMM axisType = static_cast<DIMM>(i);
		m_MouseMovement[i] = GAME_INSTANCE->Get_DIMouseMove(axisType);
	}
}

// ===================================================================
// 키 상태 전이 (DOWN → PRESSED → UP → NONE)
// ===================================================================
void Pl0000Input::Update_Pl0000_KeyState(Byte rawState, INPUT_INFO &outInfo, Float timeDelta)
{
	Bool isPressed = (rawState & 0x80) != 0;

	outInfo.doubleClickTimer += timeDelta;

	if (isPressed)
	{
		if (outInfo.state == KEY_STATE::NONE || outInfo.state == KEY_STATE::UP)
		{
			// 이전 UP → 다시 눌림: 더블클릭 판정
			if (outInfo.doubleClickTimer <= m_DoubleClickThreshold)
			{
				outInfo.state = KEY_STATE::MULTI_CLICKED;
				outInfo.isMultiClickHold = true;
			}
			else
			{
				outInfo.state = KEY_STATE::DOWN;
				outInfo.isMultiClickHold = false;
			}
			outInfo.doubleClickTimer = 0.f;
			outInfo.holdTimer = 0.f;
		}
		else // DOWN, PRESSED, MULTI_CLICKED → 유지 중
		{
			outInfo.state = KEY_STATE::PRESSED;
			outInfo.holdTimer += timeDelta;
		}
	}
	else
	{
		if (outInfo.state == KEY_STATE::DOWN || outInfo.state == KEY_STATE::PRESSED || outInfo.state == KEY_STATE::MULTI_CLICKED)
		{
			outInfo.state = KEY_STATE::UP;
			outInfo.prevHoldTimer = outInfo.holdTimer;
			outInfo.holdTimer = 0.f;
		}
		else
		{
			outInfo.state = KEY_STATE::NONE;
		}
	}
}

// ===================================================================
// WASD 통합 제어 (Bool)
// ===================================================================
Bool Pl0000Input::Is_WASD_None() const
{
	return Is_None(UBYTE(DIKEYBOARD_W)) && Is_None(UBYTE(DIKEYBOARD_A)) &&
		Is_None(UBYTE(DIKEYBOARD_S)) && Is_None(UBYTE(DIKEYBOARD_D));
}

Bool Pl0000Input::Is_WASD_UP() const
{
	return Is_KeyUp(UBYTE(DIKEYBOARD_W)) || Is_KeyUp(UBYTE(DIKEYBOARD_A)) ||
		Is_KeyUp(UBYTE(DIKEYBOARD_S)) || Is_KeyUp(UBYTE(DIKEYBOARD_D));
}

Bool Pl0000Input::Is_WASD_Down() const
{
	return Is_KeyDown(UBYTE(DIKEYBOARD_W)) || Is_KeyDown(UBYTE(DIKEYBOARD_A)) ||
		Is_KeyDown(UBYTE(DIKEYBOARD_S)) || Is_KeyDown(UBYTE(DIKEYBOARD_D));
}

Bool Pl0000Input::Is_WASD_Press() const
{
	return Is_KeyPress(UBYTE(DIKEYBOARD_W)) || Is_KeyPress(UBYTE(DIKEYBOARD_A)) ||
		Is_KeyPress(UBYTE(DIKEYBOARD_S)) || Is_KeyPress(UBYTE(DIKEYBOARD_D));
}

Bool Pl0000Input::Is_WASD_DoubleClick() const
{
	return Is_KeyMultiClick(UBYTE(DIKEYBOARD_W)) || Is_KeyMultiClick(UBYTE(DIKEYBOARD_A)) ||
		Is_KeyMultiClick(UBYTE(DIKEYBOARD_S)) || Is_KeyMultiClick(UBYTE(DIKEYBOARD_D));
}

Bool Pl0000Input::Is_WASD_Hold(Float holdThreshold) const
{
	return Is_KeyHold(UBYTE(DIKEYBOARD_W), holdThreshold) || Is_KeyHold(UBYTE(DIKEYBOARD_A), holdThreshold) ||
		Is_KeyHold(UBYTE(DIKEYBOARD_S), holdThreshold) || Is_KeyHold(UBYTE(DIKEYBOARD_D), holdThreshold);
}

Bool Pl0000Input::Is_WASD_Diagonal() const
{
	Bool w = Is_KeyPress(UBYTE(DIKEYBOARD_W));
	Bool s = Is_KeyPress(UBYTE(DIKEYBOARD_S));
	Bool a = Is_KeyPress(UBYTE(DIKEYBOARD_A));
	Bool d = Is_KeyPress(UBYTE(DIKEYBOARD_D));

	// 상반된 축 동시 입력은 대각선이 아님
	if (w && s) return false;
	if (a && d) return false;

	return (w || s) && (a || d);
}

Bool Pl0000Input::Is_WASD_NoneOrUp() const
{
	return Is_NoneOrUp(UBYTE(DIKEYBOARD_W)) && Is_NoneOrUp(UBYTE(DIKEYBOARD_A)) &&
		Is_NoneOrUp(UBYTE(DIKEYBOARD_S)) && Is_NoneOrUp(UBYTE(DIKEYBOARD_D));
}

Bool Pl0000Input::Is_WASD_SingleClickHold(Float holdThreshold) const
{
	return Is_KeySingleClickHold(UBYTE(DIKEYBOARD_W), holdThreshold) ||
		Is_KeySingleClickHold(UBYTE(DIKEYBOARD_A), holdThreshold) ||
		Is_KeySingleClickHold(UBYTE(DIKEYBOARD_S), holdThreshold) ||
		Is_KeySingleClickHold(UBYTE(DIKEYBOARD_D), holdThreshold);
}

Bool Pl0000Input::Is_WASD_DoubleClickedHold(Float holdThreshold) const
{
	return Is_KeyMultiClickHold(UBYTE(DIKEYBOARD_W), holdThreshold) ||
		Is_KeyMultiClickHold(UBYTE(DIKEYBOARD_A), holdThreshold) ||
		Is_KeyMultiClickHold(UBYTE(DIKEYBOARD_S), holdThreshold) ||
		Is_KeyMultiClickHold(UBYTE(DIKEYBOARD_D), holdThreshold);
}

Bool Pl0000Input::Is_WA_Press() const
{
	return Is_KeyPress(UBYTE(DIKEYBOARD_W)) && Is_KeyPress(UBYTE(DIKEYBOARD_A));
}

Bool Pl0000Input::Is_WA_DoubleClick() const
{
	if (Is_KeyMultiClick(UBYTE(DIKEYBOARD_W)) && Is_KeyPress(UBYTE(DIKEYBOARD_A)))
		return true;
	if (Is_KeyPress(UBYTE(DIKEYBOARD_W)) && Is_KeyMultiClick(UBYTE(DIKEYBOARD_A)))
		return true;
	
	return false;
}

Bool Pl0000Input::Is_WA_Hold(Float holdThreshold) const
{
	if (Is_KeyHold(UBYTE(DIKEYBOARD_W), holdThreshold) && Is_KeyPress(UBYTE(DIKEYBOARD_A)))
		return true;
	if (Is_KeyPress(UBYTE(DIKEYBOARD_W)) && Is_KeyHold(UBYTE(DIKEYBOARD_A), holdThreshold))
		return true;

	return false;
}

Bool Pl0000Input::Is_WA_DoubleClickedHold(Float holdThreshold) const
{
	if (Is_KeyMultiClickHold(UBYTE(DIKEYBOARD_W), holdThreshold) && Is_KeyPress(UBYTE(DIKEYBOARD_A)))
		return true;
	if (Is_KeyPress(UBYTE(DIKEYBOARD_W)) && Is_KeyMultiClickHold(UBYTE(DIKEYBOARD_A), holdThreshold))
		return true;

	return false;
}

Bool Pl0000Input::Is_WD_Press() const
{
	return Is_KeyPress(UBYTE(DIKEYBOARD_W)) && Is_KeyPress(UBYTE(DIKEYBOARD_D));
}

Bool Pl0000Input::Is_WD_DoubleClick() const
{
	if (Is_KeyMultiClick(UBYTE(DIKEYBOARD_W)) && Is_KeyPress(UBYTE(DIKEYBOARD_D)))
		return true;
	if (Is_KeyPress(UBYTE(DIKEYBOARD_W)) && Is_KeyMultiClick(UBYTE(DIKEYBOARD_D)))
		return true;

	return false;
}

Bool Pl0000Input::Is_WD_Hold(Float holdThreshold) const
{
	if (Is_KeyHold(UBYTE(DIKEYBOARD_W), holdThreshold) && Is_KeyPress(UBYTE(DIKEYBOARD_D)))
		return true;
	if (Is_KeyPress(UBYTE(DIKEYBOARD_W)) && Is_KeyHold(UBYTE(DIKEYBOARD_D), holdThreshold))
		return true;

	return false;
}

Bool Pl0000Input::Is_WD_DoubleClickedHold(Float holdThreshold) const
{
	if (Is_KeyMultiClickHold(UBYTE(DIKEYBOARD_W), holdThreshold) && Is_KeyPress(UBYTE(DIKEYBOARD_D)))
		return true;
	if (Is_KeyPress(UBYTE(DIKEYBOARD_W)) && Is_KeyMultiClickHold(UBYTE(DIKEYBOARD_D), holdThreshold))
		return true;

	return false;
}

Bool Pl0000Input::Is_SA_Press() const
{
	return Is_KeyPress(UBYTE(DIKEYBOARD_S)) && Is_KeyPress(UBYTE(DIKEYBOARD_A));
}

Bool Pl0000Input::Is_SA_DoubleClick() const
{
	if (Is_KeyMultiClick(UBYTE(DIKEYBOARD_S)) && Is_KeyPress(UBYTE(DIKEYBOARD_A)))
		return true;
	if (Is_KeyPress(UBYTE(DIKEYBOARD_S)) && Is_KeyMultiClick(UBYTE(DIKEYBOARD_A)))
		return true;

	return false;
}

Bool Pl0000Input::Is_SA_Hold(Float holdThreshold) const
{
	if (Is_KeyHold(UBYTE(DIKEYBOARD_S), holdThreshold) && Is_KeyPress(UBYTE(DIKEYBOARD_A)))
		return true;
	if (Is_KeyPress(UBYTE(DIKEYBOARD_S) && Is_KeyHold(UBYTE(DIKEYBOARD_A), holdThreshold)))
		return true;

	return false;
}

Bool Pl0000Input::Is_SA_DoubleClickedHold(Float holdThreshold) const
{
	if (Is_KeyMultiClickHold(UBYTE(DIKEYBOARD_S), holdThreshold) && Is_KeyPress(UBYTE(DIKEYBOARD_A)))
		return true;
	if (Is_KeyPress(UBYTE(DIKEYBOARD_S)) && Is_KeyMultiClickHold(UBYTE(DIKEYBOARD_A), holdThreshold))
		return true;

	return false;
}

Bool Pl0000Input::Is_SD_Press() const
{
	return Is_KeyPress(UBYTE(DIKEYBOARD_S)) && Is_KeyPress(UBYTE(DIKEYBOARD_D));
}

Bool Pl0000Input::Is_SD_DoubleClick() const
{
	if (Is_KeyMultiClick(UBYTE(DIKEYBOARD_S)) && Is_KeyPress(UBYTE(DIKEYBOARD_D)))
		return true;
	if (Is_KeyPress(UBYTE(DIKEYBOARD_S)) && Is_KeyMultiClick(UBYTE(DIKEYBOARD_D)))
		return true;

	return false;
}

Bool Pl0000Input::Is_SD_Hold(Float holdThreshold) const
{
	if (Is_KeyHold(UBYTE(DIKEYBOARD_S), holdThreshold) && Is_KeyPress(UBYTE(DIKEYBOARD_D)))
		return true;
	if (Is_KeyPress(UBYTE(DIKEYBOARD_S) && Is_KeyHold(UBYTE(DIKEYBOARD_D), holdThreshold)))
		return true;

	return false;
}

Bool Pl0000Input::Is_SD_DoubleClickedHold(Float holdThreshold) const
{
	if (Is_KeyMultiClickHold(UBYTE(DIKEYBOARD_S), holdThreshold) && Is_KeyPress(UBYTE(DIKEYBOARD_D)))
		return true;
	if (Is_KeyPress(UBYTE(DIKEYBOARD_S)) && Is_KeyMultiClickHold(UBYTE(DIKEYBOARD_D), holdThreshold))
		return true;

	return false;
}

// ===================================================================
// 마우스 콤보 큐 판별
// ===================================================================
Bool Pl0000Input::Is_MouseComboMatch(const vector<Engine::DIMB> &pattern) const
{
	if (m_MouseComboQueue.size() != pattern.size())
		return false;

	for (size_t i = 0; i < pattern.size(); ++i)
	{
		if (m_MouseComboQueue[i] != pattern[i])
			return false;
	}
	return true;
}

Bool Pl0000Input::Is_MouseComboStartsWith(const vector<Engine::DIMB> &pattern) const
{
	if (m_MouseComboQueue.size() < pattern.size())
		return false;

	for (size_t i = 0; i < pattern.size(); ++i)
	{
		if (m_MouseComboQueue[i] != pattern[i])
			return false;
	}
	return true;
}

Engine::DIMB Pl0000Input::Get_MouseComboLast() const
{
	if (m_MouseComboQueue.empty())
		return Engine::DIMB::END;

	return m_MouseComboQueue.back();
}

// ===================================================================
// 팩토리
// ===================================================================
Shared<Pl0000Input> Pl0000Input::Create(const ComPtr<ID3D11Device> &device, const ComPtr<ID3D11DeviceContext> &context)
{
	auto prototype = make_shared<Pl0000Input>(device, context);

	if (FAILED(prototype->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : Pl0000Input");
		return nullptr;
	}

	return prototype;
}

Shared<Component> Pl0000Input::Clone(void *arg)
{
	auto instance = make_shared<Pl0000Input>(*this);

	if (FAILED(instance->Initialize(arg)))
	{
		MSG_BOX("Failed to Clone : Pl0000Input");
		return nullptr;
	}

	return instance;
}
