#pragma once

namespace Engine
{
	using MetadataKeyType = const Char*;

	using DataTypeKey = const Char*;
	using DataTag = const Char*;
	using AssetTypeKey = const Char*;
	using SaveDataKey = const Char*;

	namespace Meta_Key_Type
	{
		inline constexpr MetadataKeyType DataType		= "DataType";	// 자료형
		inline constexpr MetadataKeyType TypeTag		= "TypeTag";	// 데이터 이름 
		inline constexpr MetadataKeyType AssetType		= "AssetType";	// Texture, Model, Object(특별한 타입 없으면 디폴트)
		inline constexpr MetadataKeyType SaveData		= "SaveData";	// Tag(ResourceTag) 등의 표현과 직렬화 대상인지
		inline constexpr MetadataKeyType DisplayName	= "DisplayName";	// Inspector 표시 이름(const Char*), 저장 키는 바뀌지 않음
		inline constexpr MetadataKeyType Min			= "Min";		// Inspector 입력 하한(Float), 숫자·벡터(성분마다) property만
		inline constexpr MetadataKeyType Max			= "Max";		// Inspector 입력 상한(Float), 숫자·벡터(성분마다) property만
		inline constexpr MetadataKeyType ReferenceBase	= "ReferenceBase";	// ObjectRef property가 받는 대상의 등록 base 이름(const Char*)
	}

	namespace Data_Type_Key /* For DataType */
	{
		inline constexpr DataTypeKey wString = "wString";
		inline constexpr DataTypeKey int32 = "int32";
		inline constexpr DataTypeKey uint32 = "uint32";
		inline constexpr DataTypeKey Float = "Float";
		inline constexpr DataTypeKey Double = "Double";
		inline constexpr DataTypeKey Vector2 = "Vector2";
		inline constexpr DataTypeKey Vector3 = "Vector3";
		inline constexpr DataTypeKey Vector4 = "Vector4";
		inline constexpr DataTypeKey Ptr	= "Pointer";
	}
	namespace Data_Tag /* For TypeTag */
	{
		inline constexpr DataTag NoneTag		= "None";			// Default DataType을 따라감
		inline constexpr DataTag Position		= "Position";		// Vector3
		inline constexpr DataTag Quaternion		= "Quaternion";		// Vector4
		inline constexpr DataTag Rotation		= "Rotation";		// Vector3
		inline constexpr DataTag Scale			= "Scale";			// Vector3
		inline constexpr DataTag Color			= "Color";			// Vector4
		inline constexpr DataTag Angle			= "Angle";			// Float 라디안, Inspector는 도로 보여 준다(Min/Max는 라디안)
		inline constexpr DataTag ResourceTag	= "ResourceTag";	// Load 시 리소스 테그, AssetType이 받는 Asset Browser 항목 종류
	}
	namespace Asset_Type_Key  /* For AssetType; Asset Browser 드래그 payload("Type|Tag")의 Type과 같다 */
	{
		inline constexpr AssetTypeKey NoneAsset		= "None"; // Default
		inline constexpr AssetTypeKey GameObject	= "GameObject";
		inline constexpr AssetTypeKey Texture		= "Texture";
		inline constexpr AssetTypeKey Model			= "Model";
		inline constexpr AssetTypeKey Prefab		= "Prefab";	// Prefab 문서 파일 이름(확장자 제외)
	}
	namespace Save_Data_Key /* For SaveData, Serialize */
	{
		inline constexpr SaveDataKey TransformPos		= "TransformPosition";
		inline constexpr SaveDataKey TransformRot		= "TransformRotation";
		inline constexpr SaveDataKey TransformScale		= "TransformScale";
		inline constexpr SaveDataKey TextureTag			= "TextureTag";
		inline constexpr SaveDataKey ModelTag			= "ModelTag";
		inline constexpr SaveDataKey AnimationPreset	= "AnimationPreset";
		inline constexpr SaveDataKey UIAnchor			= "UIAnchor";
		inline constexpr SaveDataKey ObjectReference	= "ObjectReference";
	}

}
