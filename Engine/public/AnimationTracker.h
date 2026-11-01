#pragma once
#include "Component.h"

NS_BEGIN(Engine)

class AnimationTracker final : public Component
{
public:
	typedef struct tagAnimationNotify
	{
		tagAnimationNotify(const wstring& tag, Float pointFrame, const function<void()>& onInvoke)
			: notifyTag{ tag }, type{ NOTIFY_TYPE::POINT }, invokeFrame{ pointFrame }, endFrame{ 0.f }
			, onNotify{ onInvoke }, onEndNotify{ nullptr } {}
		tagAnimationNotify(const wstring& tag, Float invokeFrame, Float endFrame, const function<void()>& onInvoke, const function<void()>& onExit = nullptr)
			: notifyTag{ tag }, type{ NOTIFY_TYPE::RANGE }, invokeFrame{ invokeFrame }, endFrame{ endFrame }
			, onNotify{ onInvoke }, onEndNotify{ onExit } {}

		wstring notifyTag{};
		NOTIFY_TYPE type{};
		Float invokeFrame{};
		Float endFrame{};

		function<void()> onNotify;
		function<void()> onEndNotify; // Point때는 발동 안함
	} ANIMATION_NOTIFY;

public:
	typedef struct tagAnimationTrackerNotify
	{
		wstring notifyTag{};
		NOTIFY_TYPE type{};
		Float invokeProgress{};
		Float endProgress{};

		function<void()> onNotify;
		function<void()> onEndNotify; // Point때는 발동 안함
	} ANIMATION_TRACKER_NOTIFY;

public:
	explicit AnimationTracker();
	explicit AnimationTracker(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit AnimationTracker(const AnimationTracker& rhs);
	~AnimationTracker() override = default;

public:
	void Add_Notify(uint32 animIndex, const ANIMATION_TRACKER_NOTIFY& notify);
	void Add_Notify(uint32 animIndex, std::initializer_list<ANIMATION_TRACKER_NOTIFY> notifies);
	void Update(uint32 animIndex, Float progress);
	void Reset();
	void Clear();
	Bool Is_ActiveNotify(uint32 animIndex, const wstring& notifyTag) const;

public:
	COMPONENT_TYPE Get_ComponentType() const override { return COMPONENT_TYPE::ANIMATION_TRACKER; }
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;

private:
	unordered_map<uint32, vector<ANIMATION_TRACKER_NOTIFY>> m_Notifies;
	unordered_map<uint32, vector<Bool>> m_States;
	uint32 m_PrevAnimIndex{ UINT_MAX };
	Float m_PrevProgress{ 0.f };

public:
	static Shared<AnimationTracker> CreatePrototype();
	static Shared<AnimationTracker> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	Shared<Component> Clone(void* arg = nullptr) override { return nullptr; }
	
};

NS_END