#pragma once
#include "Component.h"

NS_BEGIN(Engine)

class Bone;
class Channel;

class Animation final : public Component
{
public:
	explicit Animation();
	explicit Animation(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit Animation(const Animation& rhs);
	~Animation() override = default;

public:
	void On_Destroy() override;

public:
	COMPONENT_TYPE Get_ComponentType() const override { return COMPONENT_TYPE::ANIMATION; }
	HRESULT Initialize_Prototype(const MODEL_ANIMATION& modelAnimation);
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;

public:
	const TRANSFORM_FRAME& Get_TransformVelocity(int32 boneIndex) const;
	const wstring& Get_AnimationName() { return m_Name; }
	Float Get_Progress() const { return m_Duration > 0.f ? m_CurrentTrackPosition / m_Duration : 1.f; }
	void Set_Progress(Float progress);
	Float Get_Duration() const { return m_Duration; }
	Float Get_TickPerSecond() const { return m_TickPerSecond; }

public:
	Bool Is_LocalTransformationPresent() const { return m_IsLocalTransformationPresent; }
	void Set_LocalTransformationPresent(Bool isActive) { m_IsLocalTransformationPresent = isActive; }

public:
	Bool Update_TransformationMatrix(Float timeDelta, const vector<Shared<Bone>>& bones, Bool isLoop, int32 rootNodeIndex = -1);
	void Blend_TransformationMatrix(Float timeDelta, const Shared<Animation>& nextAnim, Float blendRatio, const vector<Shared<Bone>>& bones, Bool isCurLoop, Bool isNextLoop, int32 rootNodeIndex = -1);

private:
	wstring m_Name;
	Float m_Duration{}; // 애니메이션의 길이 절대 값
	Float m_TickPerSecond{};
	Float m_CurrentTrackPosition{};

	uint32 m_NumChannels{};
	vector<Shared<Channel>>	m_Channels;
	vector<uint32>			m_CurrentKeyFrameIndices;
	vector<int32>			m_ChannelIndexByBone; // 본 번호 → 채널 번호, 채널이 없는 본은 -1

private:
	Bool m_IsLocalTransformationPresent{ false }; // 루트 노드에 Local Transform 이 있는지 체크


public:
	static Shared<Animation> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context, const MODEL_ANIMATION& animationData);
	Shared<Component> Clone(void* arg = nullptr) override;
};

NS_END
