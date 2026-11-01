#ifndef PCH_H
#define PCH_H

#include <string>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <memory>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <DirectXMath.h>
#include <directxtk/SimpleMath.h>

#include "Engine_Macro.h"
#include "Engine_Typedef.h"
#include "Engine_Enum.h"
#include "Engine_Struct.h"
#include "Engine_ModelStruct.h"

#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>

#define BIN(_DATA_) reinterpret_cast<const Char*>(_DATA_)

namespace Tool
{
}
using namespace Tool;

using namespace Engine;
using namespace Tool;
using namespace std;

#endif //PCH_H
