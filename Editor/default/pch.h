#pragma once

#include "targetver.h"
#define WIN32_LEAN_AND_MEAN
#include <malloc.h>
#include <memory.h>
#include <stdlib.h>
#include <tchar.h>
#include <windows.h>

#include <algorithm>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <vector>

// Engine Headers
#include "Engine_Define.h"
#include "Client_Define.h"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

// ImGui Headers
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

// Editor Headers
#include "Editor_Define.h"

#include "Game.h"
#include "SpdLogger.h"
#include "String_Helper.h"


using namespace Engine;
using namespace Client;
using namespace Helper;
using namespace std;
