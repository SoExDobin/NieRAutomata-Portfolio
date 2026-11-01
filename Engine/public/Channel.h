#pragma once
#include "Component.h"

NS_BEGIN(Engine)

class Bone;

class Channel final : public Component
{
public:
	explicit Channel();
	explicit Channel(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit Channel(const Channel& rhs);
	~Channel() override = default;

public:
	void On_Destroy() override;

public:
	COMPONENT_TYPE Get_ComponentType() const override { return COMPONENT_TYPE::CHANNEL; }
	HRESULT Initialize_Prototype(const MODEL_CHANNEL& modelChannel);
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;

public:
	const TRANSFORM_FRAME& Get_TransformDelta() const { return m_Transformation; }
	void ResetVelocityState() { m_IsFirstUpdate = true; m_PrevTrackPosition = -1.f; }
	int32 Get_BoneIndex() const { return m_BoneIndex; }
	void Get_ChannelTransform(Float currentTrackPosition, uint32& currentKeyFrameIndex, Float duration, Bool isLoop, _Out_ TRANSFORM_FRAME& outTransform);
	void Update_Velocity(const TRANSFORM_FRAME& currentFrame, Float currentTrackPosition);
	void Update_TransformationMatrix(uint32& currentKeyFrameIndex, Float currentTrackPosition, Float duration, const vector<Shared<Bone>>& bones, int32 rootNodeIndex, Bool isLoop);

private:
	uint32				m_NumKeyFrames{};
	// 키프레임은 읽기 전용이라 클론끼리 공유한다(인스턴스마다 복사하지 않는다).
	Shared<const vector<KEYFRAME>>	m_KeyFrames;
	int32				m_BoneIndex{ -1 };

	TRANSFORM_FRAME		m_Transformation{};
	TRANSFORM_FRAME		m_PrevTransform{};
	Float				m_PrevTrackPosition{ -1.f };
	Bool				m_IsFirstUpdate{ true };
	Bool				m_IsFirstFrame{ true };

public:
	static Shared<Channel> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context, const MODEL_CHANNEL& keyFrame);
	Shared<Component> Clone(void* arg = nullptr) override;

};

NS_END