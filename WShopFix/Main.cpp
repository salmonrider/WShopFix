#include "Includes.h"

std::string GetConfigPath()
{
	return ArkApi::Tools::GetCurrentDir() + "/ArkApi/Plugins/WShopFix/config.json";
}

void ReadConfig()
{
	const std::string config_path = GetConfigPath();
	std::ifstream file{ config_path };
	if (!file.is_open())
		throw std::runtime_error("Can't open config.json");

	file >> config;

	file.close();
}

void ReloadConfigConsole(APlayerController* pc, FString* command, bool) {
	if (pc) {
		AShooterPlayerController* _pc = static_cast<AShooterPlayerController*>(pc);

		try {
			ReadConfig();
			ArkApi::GetApiUtils().SendServerMessage(_pc, FColorList::Green, "Reloaded Config!");
		}
		catch (std::exception e) {
			Log::GetLog()->error("Failed to read config! {}", e.what());
		}
	}
}

void ReloadConfigRcon(RCONClientConnection* rcon_connection, RCONPacket* rcon_packet, UWorld* /*unused*/)
{
	FString reply;

	try {
		ReadConfig();
		reply = "Reloaded Config!";
	}
	catch (std::exception e) {
		Log::GetLog()->error("Failed to read config! {}", e.what());
	}

	rcon_connection->SendMessageW(rcon_packet->Id, 0, &reply);
}

UClass* WShopBuffClass = nullptr;

int vulnerableFunctionIndex = -1;

TArray<uint64> IgnoreRequests;

DECLARE_HOOK(UObject_ProcessInternal, void, UObject*, FFrame*, void* const);

void Hook_UObject_ProcessInternal(UObject* _this, FFrame* Stack, void* const Result)
{
	if (Stack->NodeField()->InternalIndexField() == vulnerableFunctionIndex) {
		if (_this) {
			APrimalBuff* buff = static_cast<APrimalBuff*>(_this);

			if (buff) {
				if (buff->GetOwnerController()) {
					uint64 steamId = ArkApi::GetApiUtils().GetSteamIdFromController(buff->GetOwnerController());

					FString* Command = nullptr;

					for (UProperty* prop = Stack->NodeField()->PropertyLinkField(); prop; prop = prop->PropertyLinkNextField())
					{
						if (prop->NameField().ToString().Contains("console"))
						{
							Command = reinterpret_cast<FString*>(Stack->LocalsField() + prop->Offset_InternalField());
						}
					}

					if (Command) {
						if (!Command->StartsWith("wshop", ESearchCase::IgnoreCase) || Command->Contains("||", ESearchCase::IgnoreCase)) {
							if (!IgnoreRequests.Contains(steamId)) {
								nlohmann::json j;
								j["content"] = fmt::format("Player is trying to execute non-WShop commands {} ({})", steamId, Command->ToString());

								if (config.value("WebhookLink", "").length() > 0) {
									API::Requests::Get().CreatePostRequest(config.value("WebhookLink", ""), [](bool success, std::string result) {}, j.dump(), "application/json");
								}

								IgnoreRequests.Add(steamId);

								return;
							}
							else return;
						}
					}
				}
			}
		}
	}

	UObject_ProcessInternal_original(_this, Stack, Result);
}

void FinishLoad() {
	FString WShopBuffPath("Blueprint'/Game/Mods/WShop/Buff/Buff_Base_WShop.Buff_Base_WShop'");

	WShopBuffClass = UVictoryCore::BPLoadClass(&WShopBuffPath);
	if (!WShopBuffClass) {
		Log::GetLog()->error("No WShop Buff Class! (Not using WShop)");
		return;
	}

	UFunction* vulnerableFunction = WShopBuffClass->FindFunctionByName(FName("ExectuteConsoleCmdOnServer", EFindName::FNAME_Find), EIncludeSuperFlag::ExcludeSuper);
	if (!vulnerableFunction) {
		Log::GetLog()->error("Function not found!");
		return;
	}

	vulnerableFunctionIndex = vulnerableFunction->InternalIndexField();
}

void Load()
{
	Log::Get().Init("WShopFix");
	try
	{
		ReadConfig();
	}
	catch (const std::exception& error)
	{
		Log::GetLog()->error(error.what());
		throw;
	}

	API::Timer::Get().DelayExecute(&FinishLoad, 0);

	ArkApi::GetCommands().AddRconCommand("WShopFix.Reload", &ReloadConfigRcon);
	ArkApi::GetCommands().AddConsoleCommand("WShopFix.Reload", &ReloadConfigConsole);

	ArkApi::GetHooks().SetHook("UObject.ProcessInternal", &Hook_UObject_ProcessInternal, &UObject_ProcessInternal_original);
}

void Unload()
{
	ArkApi::GetCommands().RemoveRconCommand("WShopFix.Reload");
	ArkApi::GetCommands().RemoveConsoleCommand("WShopFix.Reload");

	ArkApi::GetHooks().DisableHook("UObject.ProcessInternal", &Hook_UObject_ProcessInternal);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		Load();
		break;
	case DLL_PROCESS_DETACH:
		Unload();
		break;
	}
	return TRUE;
}
