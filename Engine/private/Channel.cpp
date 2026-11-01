#include "Channel.h"

#include "Animation.h"
#include "Bone.h"
#include "SpdLogger.h"
#include "String_Helper.h"

Channel::Channel() : Component{} {}
Channel::Channel(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: Component{ device, context } {}

Channel::Channel(const Channel& rhs)
	: Component{ rhs }, m_NumKeyFrames{ rhs.m_NumKeyFrames }, m_KeyFrames{ rhs.m_KeyFrames }, m_BoneIndex{rhs.m_BoneIndex} 
{
	m_IsFirstUpdate = true; // 클론된 인스턴스는 새로운 상태로 시작해야 함
	m_PrevTrackPosition = -1.f;
	m_Transformation = { Vector3::One, Vector4::UnitW, Vector3::Zero };
}

void Channel::On_Destroy()
{
	m_KeyFrames.reset();
	Component::On_Destroy();
}

HRESULT Channel::Initialize_Prototype(const MODEL_CHANNEL& modelChannel)
{
	m_BoneIndex = modelChannel.boneIndex;
	m_NumKeyFrames = modelChannel.numKeyFrames;
	m_KeyFrames = make_shared<const vector<KEYFRAME>>(modelChannel.keyFrames);

	return Component::Initialize_Prototype();
}

HRESULT Channel::Initialize_Prototype()
{
	return Component::Initialize_Prototype();
}

HRESULT Channel::Initialize(void* arg)
{
	return Component::Initialize(arg);
}

void Channel::Get_ChannelTransform(Float currentTrackPosition, uint32& currentKeyFrameIndex, Float duration, Bool isLoop, _Out_ TRANSFORM_FRAME& outTransform)
{
	if (!m_KeyFrames || m_KeyFrames->empty())
	{
		outTransform = { Vector3::One, Quaternion::Identity, Vector3::Zero };
		return;
	}
	const vector<KEYFRAME>& keyFrames = *m_KeyFrames;
	if (keyFrames.size() == 1 || duration <= 0.f)
	{
		const auto& key = keyFrames.front();
		outTransform = { key.scale, key.rotation, key.position };
		currentKeyFrameIndex = 0;
		return;
	}
	if (currentTrackPosition <= 0.f)
		currentTrackPosition = 0;

	if (currentTrackPosition < keyFrames[currentKeyFrameIndex].trackPosition)
	{
		currentKeyFrameIndex = 0;
	}

	KEYFRAME firstKeyFrame = keyFrames.front();
	KEYFRAME lastKeyFrame = keyFrames.back();

	if (currentTrackPosition >= lastKeyFrame.trackPosition)
	{
		if (false == isLoop)
		{
			outTransform.scale = lastKeyFrame.scale;
			outTransform.rotation = lastKeyFrame.rotation;
			outTransform.position = lastKeyFrame.position;
			return;
		}

		Float ratio = 0.f;
		Float trackRange = duration - lastKeyFrame.trackPosition;

		if (trackRange > 0.f)
			ratio = (currentTrackPosition - lastKeyFrame.trackPosition) / trackRange;

		outTransform.scale = Vector3::Lerp(lastKeyFrame.scale, firstKeyFrame.scale, ratio);
		outTransform.rotation = Quaternion::Slerp(lastKeyFrame.rotation, firstKeyFrame.rotation, ratio);
		outTransform.position = Vector3::Lerp(lastKeyFrame.position, firstKeyFrame.position, ratio);
	}
	else
	{
		while (currentTrackPosition >= keyFrames[currentKeyFrameIndex + 1].trackPosition)
			++currentKeyFrameIndex;

		uint32 curIndex = currentKeyFrameIndex;
		uint32 nextIndex = curIndex + 1;

		Float beforeTrackPos = keyFrames[curIndex].trackPosition;
		Float nextTrackPos = keyFrames[nextIndex].trackPosition;
		Float ratio = (currentTrackPosition - beforeTrackPos) / (nextTrackPos - beforeTrackPos);

		outTransform.scale = Vector3::Lerp(keyFrames[curIndex].scale, keyFrames[nextIndex].scale, ratio);
		outTransform.rotation = Quaternion::Slerp(keyFrames[curIndex].rotation, keyFrames[nextIndex].rotation, ratio);
		outTransform.position = Vector3::Lerp(keyFrames[curIndex].position, keyFrames[nextIndex].position, ratio);
	}
}

void Channel::Update_Velocity(const TRANSFORM_FRAME& currentFrame, Float currentTrackPosition)
{
	if (m_IsFirstUpdate)
	{
		m_PrevTransform = currentFrame;
		m_Transformation = TRANSFORM_FRAME{ Vector3::One, Quaternion::Identity, Vector3::Zero };
		m_IsFirstUpdate = false;
	}
	else
	{
		if (currentTrackPosition < m_PrevTrackPosition && m_KeyFrames && !m_KeyFrames->empty())
		{
			/* 루프가 한 바퀴 돌았다. 이전 위치에서 마지막 키까지 간 거리와 첫 키에서 지금까지 간 거리를 더해
			   경계 프레임의 이동도 빠짐없이 준다. 새로 재생할 때는 Set_Progress가 상태를 지워 여기 오지 않는다. */
			m_Transformation.position = (m_KeyFrames->back().position - m_PrevTransform.position) +
				(currentFrame.position - m_KeyFrames->front().position);
			m_PrevTransform = currentFrame;
		}
		else
		{
			m_Transformation.position = currentFrame.position - m_PrevTransform.position;

			Quaternion qtCurr(currentFrame.rotation);
			Quaternion qtPrev(m_PrevTransform.rotation);
			Quaternion qtInvPrev; qtPrev.Inverse(qtInvPrev);

			Quaternion qtDelta = qtCurr * qtInvPrev;
			m_Transformation.rotation = Vector4(qtDelta.x, qtDelta.y, qtDelta.z, qtDelta.w);

			m_PrevTransform = currentFrame;
		}

	}
	m_PrevTrackPosition = currentTrackPosition;
}

void Channel::Update_TransformationMatrix(uint32& currentKeyFrameIndex, Float currentTrackPosition, Float duration, const vector<Shared<Bone>>& bones, int32 rootNodeIndex, Bool isLoop)
{
	TRANSFORM_FRAME currentFrame{};
	Get_ChannelTransform(currentTrackPosition, currentKeyFrameIndex, duration, isLoop, currentFrame);
	Update_Velocity(currentFrame, currentTrackPosition);

	Matrix boneMatrix{};
	if (m_BoneIndex == rootNodeIndex)
		boneMatrix =
		XMMatrixAffineTransformation(currentFrame.scale, Quaternion::Identity, currentFrame.rotation, Vector3::Zero);
	else 
		boneMatrix =
		XMMatrixAffineTransformation(currentFrame.scale, Quaternion::Identity, currentFrame.rotation, currentFrame.position);

	bones[m_BoneIndex]->Update_TransformationMatrix(boneMatrix);
}

Shared<Channel> Channel::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context, const MODEL_CHANNEL& keyFrame)
{
	auto channel = make_shared<Channel>(device, context);

	if (FAILED(channel->Initialize_Prototype(keyFrame)))
	{
		LOG_ERROR(L"Failed to Created : Channel {}", Helper::To_wString(keyFrame.name));
		MSG_BOX("Failed to Created : Channel");
	}

	return channel;
}

Shared<Component> Channel::Clone(void* arg)
{
	auto instance = make_shared<Channel>(*this);

	if (FAILED(instance->Initialize(arg)))
	{
		MSG_BOX("Failed to Clone : Channel");
		return nullptr;
	}

	return instance;
}


