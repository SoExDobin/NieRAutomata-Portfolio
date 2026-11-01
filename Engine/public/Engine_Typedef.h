#ifndef Engine_Typedef_h__
#define Engine_Typedef_h__

#include <memory>
#include <directxtk/SimpleMath.h>

namespace Engine
{
	template <typename T>
	inline const std::vector<T> EMPTY_VECTOR{};

	using Bool = bool;

	using Byte = signed char;
	using uByte = unsigned char;
	using Char = char;
	using tChar = wchar_t;

	using Short = signed short;
	using uShort = unsigned short;

	using uint8 = unsigned __int8;
	using uint16 = unsigned __int16;
	using uint32 = unsigned __int32;
	using uint32 = uint32_t;

	using int8 = signed __int8;
	using int16 = signed __int16;
	using int32 = signed __int32;
	using int32 = int32_t;

	using Long = signed long;
	using uLong = unsigned long;

	using Float = float;
	using Double = double;

	template <typename T>
	using Shared = std::shared_ptr<T>;

	template <typename T>
	using Unique = std::unique_ptr<T>;

	template <typename T>
	using Weak = std::weak_ptr<T>;

	typedef		DirectX::XMFLOAT2				Float2;
	typedef		DirectX::XMFLOAT3				Float3;
	typedef		DirectX::XMFLOAT4				Float4;
	typedef		DirectX::XMFLOAT4X4				Float4x4;

	using Vector2		= DirectX::SimpleMath::Vector2;
	using Vector3		= DirectX::SimpleMath::Vector3;
	using Vector4		= DirectX::SimpleMath::Vector4;
	using Matrix		= DirectX::SimpleMath::Matrix;
	using Quaternion	= DirectX::SimpleMath::Quaternion;
	using Color			= DirectX::SimpleMath::Color;
}

#endif // Engine_Typedef_h__
