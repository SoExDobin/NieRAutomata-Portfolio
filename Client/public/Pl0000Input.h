#pragma once
#include "ScriptComponent.h"

NS_BEGIN(Client)

class CLIENT_DLL Pl0000Input final : public ScriptComponent
{
public:
	enum class KEY_STATE { NONE, DOWN, PRESSED, UP, MULTI_CLICKED };

public:
	typedef struct tagInputInfo
	{
		KEY_STATE state = KEY_STATE::NONE;
		Float doubleClickTimer{ 999.f };
		Float holdTimer{ 0.f };
		Float prevHoldTimer{ 0.f };
		Bool isMultiClickHold{ false };
	} INPUT_INFO;

public:
	explicit Pl0000Input();
	explicit Pl0000Input(const ComPtr<ID3D11Device> &device, const ComPtr<ID3D11DeviceContext> &context);
	explicit Pl0000Input(const Pl0000Input &rhs);
	virtual ~Pl0000Input() override = default;

	// ===================================================================
	// 키보드 단일 키 상태 판별
	// ===================================================================
public:
	Bool Is_None(uByte keyID) const { return m_KeyInfos[keyID].state == KEY_STATE::NONE; }
	Bool Is_KeyDown(uByte keyID) const { return m_KeyInfos[keyID].state == KEY_STATE::DOWN; }
	Bool Is_KeyUp(uByte keyID) const { return m_KeyInfos[keyID].state == KEY_STATE::UP; }
	Bool Is_KeyPress(uByte keyID) const { return m_KeyInfos[keyID].state == KEY_STATE::PRESSED || m_KeyInfos[keyID].state == KEY_STATE::MULTI_CLICKED; }
	Bool Is_KeyMultiClick(uByte keyID) const { return m_KeyInfos[keyID].state == KEY_STATE::MULTI_CLICKED; }
	Bool Is_KeyHold(uByte keyID, Float holdThreshold) const
	{
		return (m_KeyInfos[keyID].state == KEY_STATE::PRESSED) && (m_KeyInfos[keyID].holdTimer >= holdThreshold);
	}
	Bool Is_KeyMultiClickHold(uByte keyID, Float holdThreshold) const
	{
		return Is_KeyHold(keyID, holdThreshold) && m_KeyInfos[keyID].isMultiClickHold;
	}
	Bool Is_KeySingleClickHold(uByte keyID, Float holdThreshold) const
	{
		return Is_KeyHold(keyID, holdThreshold) && !m_KeyInfos[keyID].isMultiClickHold;
	}
	Bool Is_NoneOrUp(uByte keyID) const {
		return m_KeyInfos[keyID].state == KEY_STATE::NONE || m_KeyInfos[keyID].state == KEY_STATE::UP;
	}
	Float Get_PrevHoldTimer(uByte keyID) const { return m_KeyInfos[keyID].prevHoldTimer; }


	// ===================================================================
	// WASD 통합 제어 (Bool 반환 / vector 반환)
	// ===================================================================
public:
	Bool Is_WASD_None() const;
	Bool Is_WASD_UP() const;
	Bool Is_WASD_Down() const;
	Bool Is_WASD_Press() const;
	Bool Is_WASD_DoubleClick() const;
	Bool Is_WASD_Hold(Float holdThreshold) const;
	Bool Is_WASD_Diagonal() const;
	Bool Is_WASD_NoneOrUp() const;
	Bool Is_WASD_SingleClickHold(Float holdThreshold) const;
	Bool Is_WASD_DoubleClickedHold(Float holdThreshold) const;

	Bool Is_WA_Press() const;
	Bool Is_WA_DoubleClick() const;
	Bool Is_WA_Hold(Float holdThreshold) const;
	Bool Is_WA_DoubleClickedHold(Float holdThreshold) const;

	Bool Is_WD_Press() const;
	Bool Is_WD_DoubleClick() const;
	Bool Is_WD_Hold(Float holdThreshold) const;
	Bool Is_WD_DoubleClickedHold(Float holdThreshold) const;

	Bool Is_SA_Press() const;
	Bool Is_SA_DoubleClick() const;
	Bool Is_SA_Hold(Float holdThreshold) const;
	Bool Is_SA_DoubleClickedHold(Float holdThreshold) const;

	Bool Is_SD_Press() const;
	Bool Is_SD_DoubleClick() const;
	Bool Is_SD_Hold(Float holdThreshold) const;
	Bool Is_SD_DoubleClickedHold(Float holdThreshold) const;

	// ===================================================================
	// 마우스 버튼 상태 판별
	// ===================================================================
public:
	Bool Is_MouseDown(Engine::DIMB buttonID) const { return m_MouseInfos[ETOI(buttonID)].state == KEY_STATE::DOWN; }
	Bool Is_MouseUp(Engine::DIMB buttonID) const { return m_MouseInfos[ETOI(buttonID)].state == KEY_STATE::UP; }
	Bool Is_MousePress(Engine::DIMB buttonID) const { return m_MouseInfos[ETOI(buttonID)].state == KEY_STATE::PRESSED || m_MouseInfos[ETOI(buttonID)].state == KEY_STATE::MULTI_CLICKED; }
	Bool Is_MouseDoubleClick(Engine::DIMB buttonID) const { return m_MouseInfos[ETOI(buttonID)].state == KEY_STATE::MULTI_CLICKED; }
	Bool Is_MouseHold(Engine::DIMB buttonID, Float holdThreshold) const
	{
		return (m_MouseInfos[ETOI(buttonID)].state == KEY_STATE::PRESSED) && (m_MouseInfos[ETOI(buttonID)].holdTimer >= holdThreshold);
	}

	// ===================================================================
	// 마우스 클릭 콤보 큐 (NieR 공격 시퀀스용)
	// ===================================================================
	//  좌클릭 1번       : {LB}
	//  좌좌좌좌좌좌좌    : {LB, LB, LB, LB, LB, LB, LB}
	
	// ===================================================================
public:
	const vector<Engine::DIMB> &Get_MouseComboQueue() const { return m_MouseComboQueue; }
	uint32 Get_MouseComboCount() const { return static_cast<uint32>(m_MouseComboQueue.size()); }
	void Clear_MouseComboQueue() { m_MouseComboQueue.clear(); m_ComboTimer = 0.f; }
	void Set_ComboTimeout(Float timeout) { m_ComboTimeout = timeout; }

	Bool Is_MouseComboMatch(const vector<Engine::DIMB> &pattern) const;
	Bool Is_MouseComboStartsWith(const vector<Engine::DIMB> &pattern) const;
	Engine::DIMB Get_MouseComboLast() const;

	// ===================================================================
	// 마우스 이동
	// ===================================================================
public:
	Long Get_MouseMove(DIMM axisType) const { return m_MouseMovement[ETOI(axisType)]; }

	// ===================================================================
	// 매 프레임 갱신
	// ===================================================================
public:
	void Update_Pl0000_InputState(Float timeDelta);

private:
	void Update_Pl0000_KeyState(Byte rawState, INPUT_INFO &outInfo, Float timeDelta);

	// ===================================================================
	// 컴포넌트 라이프사이클
	// ===================================================================
public:
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void *arg = nullptr) override;
	void On_Disable() override;
	void On_Enable() override;

	// ===================================================================
	// 멤버 변수
	// ===================================================================
private:
	INPUT_INFO m_KeyInfos[KEY_MAX];
	INPUT_INFO m_MouseInfos[ETOI(DIMB::END)];
	LONG m_MouseMovement[ETOI(DIMB::END)];

	Float m_DoubleClickThreshold = 0.20f;

	// 마우스 콤보 큐
	vector<Engine::DIMB> m_MouseComboQueue;
	Float m_ComboTimer{ 0.f };
	Float m_ComboTimeout{ 0.8f };

public:
	static Shared<Pl0000Input> Create(const ComPtr<ID3D11Device> &device, const ComPtr<ID3D11DeviceContext> &context);
	Shared<Component> Clone(void *arg = nullptr) override;
};

NS_END
