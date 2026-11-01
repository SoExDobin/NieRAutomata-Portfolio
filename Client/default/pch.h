#ifndef PCH_H
#define PCH_H

#include "Engine_Define.h"
#include "Client_Define.h"

// Client-wide Engine surface.
#include "Game.h"
#include "GameObject.h"
#include "LayerRegistry.h"
#include "Navigation.h"
#include "SpdLogger.h"
#include "TagRegistry.h"

// Client setting import dependencies stay on the Client side.  In
// particular, none of these parsers are included by Engine sources or RTTR
// registration units.
#include <fstream>
#include <regex>

// zlib declares a global Byte type; isolate it from Engine::Byte in unity
// builds and expose only MinizipByte while these headers are parsed.
#define Byte MinizipByte
#include <minizip/unzip.h>
#include <minizip/iowin32.h>
#undef Byte

#include <pugixml.hpp>

#endif //PCH_H
