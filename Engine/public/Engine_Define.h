#ifndef Engine_Define_h__
#define Engine_Define_h__

#include <algorithm>
#include <ctime>
#include <filesystem>
#include <format>
#include <functional>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <concepts>
#include <random>
#include <io.h>


#include <d3d11.h>
#define DIRECTX_TOOLKIT_IMPORT
#include <DirectXMath.h>
#include <d3dcompiler.h>
#include <DirectXCollision.h>

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

#include <directxtk/DDSTextureLoader.h>
#include <directxtk/SimpleMath.h>
#include <directxtk/WICTextureLoader.h>
#include <directxtk/PrimitiveBatch.h>
#include <directxtk/Effects.h>
#include <directxtk/VertexTypes.h>
#include <directxtk/SpriteBatch.h>
#include <directxtk/SpriteFont.h>
#include <directxtk/ScreenGrab.h>
#include <wrl.h>

#include <Effects11/d3dx11effect.h>

#include "fmod.hpp"
#include "fmod_common.h"
#include "fmod_errors.h"

#include "recastnavigation/Recast.h"

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#define MAGIC_ENUM_RANGE_MAX 900 

#include "magic_enum/magic_enum.hpp"

#pragma warning(push)
#pragma warning(disable: 26819) 
#include <nlohmann/json.hpp>
#pragma warning(pop)

#include "Engine_Enum.h"
#include "Engine_Function.h"
#include "Engine_Macro.h"
#include "Engine_Struct.h"
#include "Engine_RTTR_Metadata.h"
#include "Engine_Typedef.h"
#include "Engine_ID.h"
#include "Engine_ModelStruct.h"
#include "Engine_NavStruct.h"
#include "Engine_Rendering_Const.h"
#include "Shader_CBuffer_Define.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;
using namespace Microsoft::WRL;
using namespace std;
using namespace Engine;

#pragma warning(disable : 4251)
#pragma warning(disable : 26498)

#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#include <cstdlib>

#ifndef DBG_NEW
#define DBG_NEW NEW(_NORMAL_BLOCK, __FILE__, __LINE__)
#define NEW DBG_NEW
#endif

#endif

#endif // Engine_Define_h__
