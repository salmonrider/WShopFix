#pragma once

#include <API/ARK/Ark.h>
#include <fstream>
#include <iostream>
#include <string>
#include <json.hpp>

#include "Requests.h"
#include "Timer.h"

#pragma comment(lib, "ArkApi.lib")

inline nlohmann::json config;

struct FFrame
{
	UFunction*& NodeField() { return *GetNativePointerField<UFunction**>(this, "FFrame.Node"); }
	uint8*& LocalsField() { return *GetNativePointerField<uint8**>(this, "FFrame.Locals"); }
};