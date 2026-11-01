#include "Animation.h"

#include <utility>
#include "Channel.h"
#include "SpdLogger.h"
#include "String_Helper.h"
#include "Bone.h"
#include <cmath>

Animation::Animation() : Component{} {}
Animation::Animation(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: Component{ device, context } {}

Animation::Animation(const Animation& rhs)
	: Component{ rhs }, 
	m_Name{ rhs.m_Name },
	m_Duration{ rhs.m_Duration }, 
	m_TickPerSecond{ rhs.m_TickPerSecond },
	m_NumChannels{rhs.m_NumChannels}, 
	m_CurrentKeyFrameIndices{rhs.m_CurrentKeyFrameIndices},
	m_ChannelIndexByBone{rhs.m_ChannelIndexByBone},
	m_IsLocalTransformationPresent{rhs.m_IsLocalTransformationPresent}
{
	for (uint32 i = 0; i < m_NumChannels; ++i)
	{
		m_Channels.push_back(static_pointer_cast<Channel>(rhs.m_Channels[i]->Clone()));
	}
}

void Animation::On_Destroy()
{
	m_Channels.clear();
	m_CurrentKeyFrameIndices.clear();
	m_ChannelIndexByBone.clear();
	Component::On_Destroy();
}

HRESULT Animation::Initialize_Prototype(const MODEL_ANIMATION& modelAnimation)
{
	m_Name = Helper::To_wString(modelAnimation.name);
	m_Duration = modelAnimation.duration;
	m_TickPerSecond = modelAnimation.tickPerSecond;
	m_NumChannels = modelAnimation.numChannel;

	m_CurrentKeyFrameIndices.resize(m_NumChannels);

	m_Channels.clear();
	for (uint32 i = 0; i < m_NumChannels; ++i)
	{
		auto channel = Channel::Create(m_Device, m_Context, modelAnimation.channels[i]);
		if (nullptr == channel)
			return E_FAIL;

		m_Channels.push_back(channel);
	}

	// 채널의 본 번호는 로드 때 모델 본으로 재매핑된 값이라 이후 바뀌지 않는다.
	// 같은 본을 가리키는 채널이 여럿이면 앞의 것을 쓴다.
	int32 maxBoneIndex = -1;
	for (const auto& channel : m_Channels)
		maxBoneIndex = std::max(maxBoneIndex, channel->Get_BoneIndex());
	m_ChannelIndexByBone.assign(static_cast<size_t>(maxBoneIndex + 1), -1);
	for (uint32 i = 0; i < m_NumChannels; ++i)
	{
		const int32 boneIndex = m_Channels[i]->Get_BoneIndex();
		if (boneIndex >= 0 && m_ChannelIndexByBone[boneIndex] < 0)
			m_ChannelIndexByBone[boneIndex] = static_cast<int32>(i);
	}

	return S_OK;
}

HRESULT Animation::Initialize_Prototype()
{
	return Component::Initialize_Prototype();
}

HRESULT Animation::Initialize(void* arg)
{
	return Component::Initialize(arg);
}

void Animation::Set_Progress(Float progress)
{
	m_CurrentTrackPosition = m_Duration * progress;

	std::fill(m_CurrentKeyFrameIndices.begin(), m_CurrentKeyFrameIndices.end(), 0);
	
	for (auto& channel : m_Channels)
	{
		channel->ResetVelocityState();
		// m_IsFirstUpdate=true, m_PrevTrackPosition=-1.f
	}
}

const TRANSFORM_FRAME& Animation::Get_TransformVelocity(int32 boneIndex) const
{
	static TRANSFORM_FRAME emptyFrame{ Vector3::One, Vector4(0.f, 0.f, 0.f, 1.f), Vector3::Zero };
	for (const auto& channel : m_Channels)
	{
		if (channel->Get_BoneIndex() == boneIndex)
			return channel->Get_TransformDelta();
	}
	return emptyFrame;
}

Bool Animation::Update_TransformationMatrix(Float timeDelta, const vector<Shared<Bone>>& bones, Bool isLoop, int32 rootNodeIndex)
{
	// Zero-duration clips are authored static poses (e.g. sheathed weapons).
	// Sample their keys without fmod(0, 0) or skipping the pose on non-loop playback.
	if (m_Duration <= 0.f)
	{
		m_CurrentTrackPosition = 0.f;
		for (uint32 i = 0; i < m_NumChannels; ++i)
			m_Channels[i]->Update_TransformationMatrix(m_CurrentKeyFrameIndices[i], 0.f,
				0.f, bones, m_IsLocalTransformationPresent ? rootNodeIndex : -1, false);
		return !isLoop;
	}
	m_CurrentTrackPosition += m_TickPerSecond * timeDelta; // 트랙의 시간 비율 * timedelta 을 누적하여 현재 트랙 지점을 업데이트

	Bool isFinished = false;
	if (m_CurrentTrackPosition >= m_Duration) //  현재 트랙이 애니메이션 길이를 넘으면
	{
		if (false == isLoop)
		{
			/* 마지막 프레임에 고정한다. 끝 포즈로 한 번 더 평가해 끝까지 남은 루트 이동을 주고,
			   그 뒤 프레임은 같은 위치라 루트 델타가 0이 된다. */
			m_CurrentTrackPosition = m_Duration;
			isFinished = true;
		}
		else
			m_CurrentTrackPosition = fmod(m_CurrentTrackPosition, m_Duration); // 나머지 시간 보존 (Aliasing 방지)
	}

	for (uint32 i = 0; i < m_NumChannels; ++i)
	{
		if (m_IsLocalTransformationPresent) // 루트 노드의 영향을 받으면
			m_Channels[i]->Update_TransformationMatrix(m_CurrentKeyFrameIndices[i], m_CurrentTrackPosition, m_Duration, bones, rootNodeIndex, isLoop);
		else
			m_Channels[i]->Update_TransformationMatrix(m_CurrentKeyFrameIndices[i], m_CurrentTrackPosition, m_Duration, bones, -1, isLoop);
	}

	return isFinished;
}

void Animation::Blend_TransformationMatrix(Float timeDelta, const Shared<Animation>& nextAnim, Float blendRatio, const vector<Shared<Bone>>& bones, Bool isCurLoop, Bool isNextLoop, int32 rootNodeIndex)
{
	// 1. 각 애니메이션의 시간 업데이트 (나머지 보존)
	m_CurrentTrackPosition += m_TickPerSecond * timeDelta;
	if (m_CurrentTrackPosition >= m_Duration)
	{
		if (isCurLoop && m_Duration > 0.f)
			m_CurrentTrackPosition = fmod(m_CurrentTrackPosition, m_Duration);
		else
			m_CurrentTrackPosition = m_Duration;
	}
		
	
	nextAnim->m_CurrentTrackPosition += nextAnim->m_TickPerSecond * timeDelta;
	if (nextAnim->m_CurrentTrackPosition >= nextAnim->m_Duration)
	{
		if (isNextLoop && nextAnim->m_Duration > 0.f)
			nextAnim->m_CurrentTrackPosition = fmod(nextAnim->m_CurrentTrackPosition, nextAnim->m_Duration);
		else
			nextAnim->m_CurrentTrackPosition = nextAnim->m_Duration;
	}
		

	// Sparse clips do not share channel counts or array order. Match by the
	// remapped model bone index; a missing channel holds the existing local pose.
	for (uint32 boneIndex = 0; boneIndex < bones.size(); ++boneIndex)
	{
		const int32 current = boneIndex < m_ChannelIndexByBone.size() ? m_ChannelIndexByBone[boneIndex] : -1;
		const int32 next = boneIndex < nextAnim->m_ChannelIndexByBone.size() ? nextAnim->m_ChannelIndexByBone[boneIndex] : -1;
		if (current < 0 && next < 0) continue;
		TRANSFORM_FRAME held{ Vector3::One, Quaternion::Identity, Vector3::Zero };
		Quaternion rotation;
		Matrix localPose = bones[boneIndex]->Get_TransformationMatrix();
		localPose.Decompose(held.scale, rotation, held.position);
		held.rotation = rotation;
		TRANSFORM_FRAME curTrans = held, nextTrans = held;
		if (current >= 0)
		{
			const auto& channel = m_Channels[current];
			channel->Get_ChannelTransform(m_CurrentTrackPosition, m_CurrentKeyFrameIndices[current], m_Duration, isCurLoop, curTrans);
			channel->Update_Velocity(curTrans, m_CurrentTrackPosition);
		}
		if (next >= 0)
		{
			const auto& channel = nextAnim->m_Channels[next];
			channel->Get_ChannelTransform(nextAnim->m_CurrentTrackPosition, nextAnim->m_CurrentKeyFrameIndices[next], nextAnim->m_Duration, isNextLoop, nextTrans);
			channel->Update_Velocity(nextTrans, nextAnim->m_CurrentTrackPosition);
		}

		Vector3 targetScale = Vector3::Lerp(curTrans.scale, nextTrans.scale, blendRatio);
		Vector4 targetRot = Quaternion::Slerp(curTrans.rotation, nextTrans.rotation, blendRatio);
		Vector3 targetPos = Vector3::Lerp(curTrans.position, nextTrans.position, blendRatio);

		Matrix targetMatrix{};
		if (boneIndex == rootNodeIndex)
			targetMatrix = XMMatrixAffineTransformation(targetScale, Quaternion::Identity, targetRot, Vector3::Zero);
		else
			targetMatrix = XMMatrixAffineTransformation(targetScale, Quaternion::Identity, targetRot, targetPos);

		bones[boneIndex]->Update_TransformationMatrix(targetMatrix);
	}
}

Shared<Animation> Animation::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context, const MODEL_ANIMATION& animationData)
{
	auto animation = make_shared<Animation>(device, context);

	if (FAILED(animation->Initialize_Prototype(animationData)))
	{
		LOG_ERROR(L"Failed to Created : Animation {}", Helper::To_wString(animationData.name));
		MSG_BOX("Failed to Created : Animation");
		return nullptr;
	}

	return animation;
}

Shared<Component> Animation::Clone(void* arg)
{
	auto animation = make_shared<Animation>(*this);

	if (FAILED(animation->Initialize(arg)))
	{
		LOG_ERROR(L"Failed to Cloned : Animation");
		MSG_BOX("Failed to Cloned : Animation");
	}

	return animation;
}
