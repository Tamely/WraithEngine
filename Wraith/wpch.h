#pragma once

#include <iostream>
#include <memory>
#include <utility>
#include <algorithm>
#include <functional>

#include <string>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

#include "Core/Log.h"
#include "Containers/Array.h"

#include "Debug/Instrumentor.h"

#ifdef W_PLATFORM_WINDOWS
	#include <Windows.h>
#endif
