///----------------------------------------------------------------------------------------------------
/// Copyright (c) Raidcore.GG - All rights reserved.
///
/// Name         :  KeybindHandler.cpp
/// Description  :  Provides functions for keybinds.
/// Authors      :  K. Bieniek
///----------------------------------------------------------------------------------------------------

#include "KeybindHandler.h"

#include <cstdarg>
#include <filesystem>
#include <fstream>
#include <string>

#include "Consts.h"
#include "Index.h"
#include "Shared.h"
#include "State.h"

#include "Util/Strings.h"
#include "Util/Inputs.h"

#include "nlohmann/json.hpp"
using json = nlohmann::json;

namespace Keybinds
{
	void ADDONAPI_RegisterWithString(const char* aIdentifier, KEYBINDS_PROCESS aKeybindHandler, const char* aKeybind)
	{
		KeybindApi->Register(aIdentifier, EKeybindHandlerType::DownOnly, aKeybindHandler, aKeybind);
	}

	void ADDONAPI_RegisterWithStruct(const char* aIdentifier, KEYBINDS_PROCESS aKeybindHandler, Keybind aKeybind)
	{
		KeybindApi->Register(aIdentifier, EKeybindHandlerType::DownOnly, aKeybindHandler, aKeybind);
	}

	void ADDONAPI_RegisterWithString2(const char* aIdentifier, KEYBINDS_PROCESS2 aKeybindHandler, const char* aKeybind)
	{
		KeybindApi->Register(aIdentifier, EKeybindHandlerType::DownAndRelease, aKeybindHandler, aKeybind);
	}

	void ADDONAPI_RegisterWithStruct2(const char* aIdentifier, KEYBINDS_PROCESS2 aKeybindHandler, Keybind aKeybind)
	{
		KeybindApi->Register(aIdentifier, EKeybindHandlerType::DownAndRelease, aKeybindHandler, aKeybind);
	}

	void ADDONAPI_InvokeKeybind(const char* aIdentifier, bool aIsRelease)
	{
		KeybindApi->Invoke(aIdentifier, aIsRelease);
	}

	void ADDONAPI_Deregister(const char* aIdentifier)
	{
		KeybindApi->Deregister(aIdentifier);
	}
}

static bool IsLookupTableBuilt = false;
static std::map<unsigned short, std::string> ScancodeLookupTable;

void BuildscanCodeLookupTable()
{
	if (IsLookupTableBuilt) { return; }

	for (long long i = 0; i < 255; i++)
	{
		KeyLParam key{};
		key.ScanCode = i;
		char* buff = new char[64];
		std::string str;
		GetKeyNameTextA(static_cast<LONG>(KMFToLParam(key)), buff, 64);
		str.append(buff);

		ScancodeLookupTable[key.GetScanCode()] = str;

		key.ExtendedFlag = 1;
		buff = new char[64];
		str = "";
		GetKeyNameTextA(static_cast<LONG>(KMFToLParam(key)), buff, 64);
		str.append(buff);

		ScancodeLookupTable[key.GetScanCode()] = str;

		delete[] buff;
	};
}

std::string CKeybindApi::ScancodeToString(unsigned short aScanCode)
{
	if (!IsLookupTableBuilt)
	{
		BuildscanCodeLookupTable();
	}

	auto it = ScancodeLookupTable.find(aScanCode);
	if (it != ScancodeLookupTable.end())
	{
		return it->second;
	}

	return "";
}

Keybind CKeybindApi::KBFromString(std::string aKeybind)
{
	if (!IsLookupTableBuilt)
	{
		BuildscanCodeLookupTable();
	}

	Keybind kb{};

	if (aKeybind == "(null)" || aKeybind == "(NULL)") { return kb; }

	aKeybind = String::ToUpper(aKeybind);
	std::string delimiter = "+";

	size_t pos = 0;
	std::string token;
	while ((pos = aKeybind.find(delimiter)) != std::string::npos)
	{
		token = aKeybind.substr(0, pos);
		aKeybind.erase(0, pos + delimiter.length());

		if (token == "ALT")
		{
			kb.Alt = true;
		}
		else if (token == "CTRL")
		{
			kb.Ctrl = true;
		}
		else if (token == "SHIFT")
		{
			kb.Shift = true;
		}
	}

	for (auto it = ScancodeLookupTable.begin(); it != ScancodeLookupTable.end(); ++it)
	{
		if (it->second == aKeybind)
		{
			kb.Key = it->first;
			break;
		}
	}

	return kb;
}

std::string CKeybindApi::KBToString(Keybind aKeybind, bool padded)
{
	if (!IsLookupTableBuilt)
	{
		BuildscanCodeLookupTable();
	}

	if (!aKeybind.Key) { return "(null)"; }

	char* buff = new char[100];
	std::string str;

	if (aKeybind.Alt)
	{
		GetKeyNameTextA(MapVirtualKeyA(VK_MENU, MAPVK_VK_TO_VSC) << 16, buff, 100);
		str.append(buff);
		str.append(padded ? " + " : "+");
	}

	if (aKeybind.Ctrl)
	{
		GetKeyNameTextA(MapVirtualKeyA(VK_CONTROL, MAPVK_VK_TO_VSC) << 16, buff, 100);
		str.append(buff);
		str.append(padded ? " + " : "+");
	}

	if (aKeybind.Shift)
	{
		GetKeyNameTextA(MapVirtualKeyA(VK_SHIFT, MAPVK_VK_TO_VSC) << 16, buff, 100);
		str.append(buff);
		str.append(padded ? " + " : "+");
	}

	HKL hkl = GetKeyboardLayout(0);
	UINT vk = MapVirtualKeyA(aKeybind.Key, MAPVK_VSC_TO_VK);

	if (vk >= 65 && vk <= 90 || vk >= 48 && vk <= 57)
	{
		GetKeyNameTextA(aKeybind.Key << 16, buff, 100);
		str.append(buff);
	}
	else
	{
		auto it = ScancodeLookupTable.find(aKeybind.Key);
		if (it != ScancodeLookupTable.end())
		{
			str.append(it->second);
		}
	}

	delete[] buff;

	str = String::ToUpper(str);

	return String::ConvertMBToUTF8(str);
}

CKeybindApi::CKeybindApi()
{
	this->Load();
}

UINT CKeybindApi::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	Keybind kb{};

	switch (uMsg)
	{
		case WM_ACTIVATEAPP:
		{
			this->ReleaseAll();
			break;
		}

		case WM_SYSKEYDOWN:
		case WM_KEYDOWN:
		{
			if (wParam > 255) break;

			KeyLParam keylp = LParamToKMF(lParam);

			if (wParam == VK_MENU) { this->IsAltHeld = true; }
			else if (wParam == VK_CONTROL) { this->IsCtrlHeld = true; }
			else if (wParam == VK_SHIFT) { this->IsShiftHeld = true; }

			kb.Alt = this->IsAltHeld;
			kb.Ctrl = this->IsCtrlHeld;
			kb.Shift = this->IsShiftHeld;
			kb.Key = keylp.GetScanCode();

			// if shift, ctrl or alt set key to 0
			if (wParam == VK_SHIFT || wParam == VK_CONTROL || wParam == VK_MENU)
			{
				kb.Key = 0;
			}

			if (kb == Keybind{})
			{
				return uMsg;
			}

			if (keylp.PreviousKeyState)
			{
				return uMsg;
			}

			if (this->IsCapturing)
			{
				/* store the currently held bind */
				this->CapturedKeybind = kb;

				/* prevent invoking keybinds */
				break;
			}

			auto heldBind = std::find_if(this->Registry.begin(), this->Registry.end(), [kb](auto activeKeybind) { return activeKeybind.second.Bind == kb; });

			if (heldBind == this->Registry.end()) /* if held bind does not match any registered */
			{
				break;
			}

			if (std::find(this->HeldKeys.begin(), this->HeldKeys.end(), kb.Key) == this->HeldKeys.end())
			{
				this->HeldKeys.push_back(kb.Key);
			}

			/* track the actual bind/id combo too */
			if (this->HeldKeybinds.find(heldBind->first) == this->HeldKeybinds.end())
			{
				this->HeldKeybinds[heldBind->first] = heldBind->second;
			}

			if (heldBind->first == KB_TOGGLEHIDEUI)
			{
				// invoke but do not return, pass through to game (multi hide)
				this->Invoke(heldBind->first);
				break;
			}
			else
			{
				// if Invoke returns true, pass 0 to the wndproc to stop processing
				return this->Invoke(heldBind->first) ? 0 : 1;
			}

			break;
		}

		case WM_SYSKEYUP:
		case WM_KEYUP:
		{
			if (wParam > 255) break;

			if (wParam == VK_MENU) { this->IsAltHeld = false; }
			else if (wParam == VK_CONTROL) { this->IsCtrlHeld = false; }
			else if (wParam == VK_SHIFT) { this->IsShiftHeld = false; }

			/* only check if not currently setting keybind */
			if (this->IsCapturing)
			{
				return uMsg;
			}

			std::vector<std::string> heldBindsPop;

			//const std::lock_guard<std::mutex> lock(this->MutexHeldKeys);
			for (auto& bind : this->HeldKeybinds)
			{
				if (wParam == VK_SHIFT && bind.second.Bind.Shift)
				{
					this->Invoke(bind.first, true);
					this->IsShiftHeld = false;
					heldBindsPop.push_back(bind.first);
				}
				else if (wParam == VK_CONTROL && bind.second.Bind.Ctrl)
				{
					this->Invoke(bind.first, true);
					this->IsCtrlHeld = false;
					heldBindsPop.push_back(bind.first);
				}
				else if (wParam == VK_MENU && bind.second.Bind.Alt)
				{
					this->Invoke(bind.first, true);
					this->IsAltHeld = false;
					heldBindsPop.push_back(bind.first);
				}
				else
				{
					KeyLParam keylp = LParamToKMF(lParam);
					unsigned short scanCode = keylp.GetScanCode();

					if (scanCode == bind.second.Bind.Key)
					{
						this->Invoke(bind.first, true);
						this->HeldKeys.erase(std::find(this->HeldKeys.begin(), this->HeldKeys.end(), scanCode));
						heldBindsPop.push_back(bind.first);
					}
				}
			}

			for (auto bind : heldBindsPop)
			{
				this->HeldKeybinds.erase(bind);
			}

			break;
		}
	}

	// don't pass keys to game if currently editing keybinds
	if (this->IsCapturing)
	{
		if (uMsg == WM_SYSKEYDOWN || uMsg == WM_KEYDOWN || uMsg == WM_SYSKEYUP || uMsg == WM_KEYUP)
		{
			return 0;
		}
	}

	return uMsg;
}

void CKeybindApi::Register(const char* aIdentifier, EKeybindHandlerType aKeybindHandlerType, void* aKeybindHandler, const char* aKeybind)
{
	Keybind requestedBind = CKeybindApi::KBFromString(aKeybind);
	this->Register(aIdentifier, aKeybindHandlerType, aKeybindHandler, requestedBind);
}

void CKeybindApi::Register(const char* aIdentifier, EKeybindHandlerType aKeybindHandlerType, void* aKeybindHandler, Keybind aKeybind)
{
	std::string str = aIdentifier;

	Keybind requestedBind = aKeybind;

	/* check if another identifier, already uses the keybind */
	std::string res = this->IsInUse(requestedBind);

	if (res != str && res != "")
	{
		/* another identifier uses the same combination */
		requestedBind = {};
	}

	{
		/* explicitly scoping here because the subsequent Save call will also lock the mutex */
		const std::lock_guard<std::mutex> lock(this->Mutex);

		/* check if this keybind is not already set */
		if (this->Registry.find(str) == Registry.end())
		{
			// FIXME: Double lookup.
			this->Registry[str].Bind = requestedBind;
		}

		// FIXME: Even more lookups.
		this->Registry[str].HandlerType = aKeybindHandlerType;
		this->Registry[str].Handler = aKeybindHandler;
	}

	this->Save();
}

void CKeybindApi::Deregister(const char* aIdentifier)
{
	std::string str = aIdentifier;

	{
		/* explicitly scoping here because the subsequent Save call will also lock the mutex */
		const std::lock_guard<std::mutex> lock(this->Mutex);

		auto it = this->Registry.find(str);
		if (it != this->Registry.end())
		{
			it->second.Handler = nullptr;
		}
	}

	this->Save();
}

std::string CKeybindApi::IsInUse(Keybind aKeybind)
{
	/* sanity check */
	if (aKeybind == Keybind{}) { return ""; }

	const std::lock_guard<std::mutex> lock(this->Mutex);

	/* check if another identifier, already uses the keybind */
	for (auto& [identifier, keybind] : this->Registry)
	{
		if (keybind.Bind == aKeybind)
		{
			return identifier;
		}
	}

	return "";
}

void CKeybindApi::Set(std::string aIdentifier, Keybind aKeybind)
{
	std::string res = this->IsInUse(aKeybind);

	if (res != aIdentifier && res != "") { return; }

	{
		const std::lock_guard<std::mutex> lock(this->Mutex);

		// FIXME: Double lookup.
		this->Registry[aIdentifier].Bind = aKeybind;
	}

	this->Save();
}

bool CKeybindApi::Invoke(std::string aIdentifier, bool aIsRelease)
{
	bool called = false;

	const std::lock_guard<std::mutex> lock(this->Mutex);

	auto& bind = this->Registry[aIdentifier];

	void* handlerFunc = bind.Handler;
	EKeybindHandlerType type = bind.HandlerType;

	if (handlerFunc)
	{
		if ((bind.HandlerType == EKeybindHandlerType::DownOnly && !aIsRelease) ||
			bind.HandlerType == EKeybindHandlerType::DownAndRelease)
		{
			std::thread([aIdentifier, type, handlerFunc, aIsRelease]()
			{
				switch (type)
				{
					case EKeybindHandlerType::DownOnly:
						((KEYBINDS_PROCESS)handlerFunc)(aIdentifier.c_str());
						break;
					case EKeybindHandlerType::DownAndRelease:
						((KEYBINDS_PROCESS2)handlerFunc)(aIdentifier.c_str(), aIsRelease);
						break;
				}
			}).detach();

			called = true;
		}
	}

	return called;
}

void CKeybindApi::Delete(std::string aIdentifier)
{
	const std::lock_guard<std::mutex> lock(this->Mutex);

	this->Registry.erase(aIdentifier);
}

int CKeybindApi::Verify(void* aStartAddress, void* aEndAddress)
{
	int refCounter = 0;

	const std::lock_guard<std::mutex> lock(this->Mutex);

	for (auto& [identifier, activekb] : this->Registry)
	{
		if (activekb.Handler >= aStartAddress && activekb.Handler <= aEndAddress)
		{
			activekb.Handler = nullptr;
			refCounter++;
		}
	}

	return refCounter;
}

std::map<std::string, ActiveKeybind> CKeybindApi::GetRegistry() const
{
	const std::lock_guard<std::mutex> lock(this->Mutex);

	return this->Registry;
}

Keybind CKeybindApi::GetCapturedKeybind() const
{
	const std::lock_guard<std::mutex> lock(this->Mutex);

	return this->CapturedKeybind;
}

void CKeybindApi::StartCapturing()
{
	if (!this->IsCapturing)
	{
		this->ReleaseAll();
	}

	this->IsCapturing = true;
}

void CKeybindApi::EndCapturing()
{
	this->IsCapturing = false;

	this->ReleaseAll();

	CapturedKeybind = Keybind{};
}

void CKeybindApi::Load()
{
	if (!std::filesystem::exists(Index::F_KEYBINDS)) { return; }

	const std::lock_guard<std::mutex> lock(this->Mutex);

	try
	{
		std::ifstream file(Index::F_KEYBINDS);

		json keybinds = json::parse(file);
		for (json binding : keybinds)
		{
			if (binding.is_null() ||
				binding["Key"].is_null() ||
				binding["Alt"].is_null() ||
				binding["Ctrl"].is_null() ||
				binding["Shift"].is_null())
			{
				Logger->Debug(CH_KEYBINDS, "One or more fields of keybind were null.");
				continue;
			}

			Keybind kb{};
			binding["Key"].get_to(kb.Key);
			binding["Alt"].get_to(kb.Alt);
			binding["Ctrl"].get_to(kb.Ctrl);
			binding["Shift"].get_to(kb.Shift);

			std::string identifier = binding["Identifier"].get<std::string>();

			// FIXME: Double lookup.
			this->Registry[identifier].Bind = kb;
		}

		file.close();
	}
	catch (json::parse_error& ex)
	{
		Logger->Warning(CH_KEYBINDS, "Keybinds.json could not be parsed. Error: %s", ex.what());
	}
}

void CKeybindApi::Save()
{
	const std::lock_guard<std::mutex> lock(this->Mutex);

	json keybinds = json::array();

	for (auto& it : this->Registry)
	{
		Keybind kb = it.second.Bind;
		std::string id = it.first;

		json binding =
		{
			{"Identifier",	id},
			{"Key",			kb.Key},
			{"Alt",			kb.Alt},
			{"Ctrl",		kb.Ctrl},
			{"Shift",		kb.Shift}
		};

		keybinds.push_back(binding);
	}

	std::ofstream file(Index::F_KEYBINDS);

	file << keybinds.dump(1, '\t') << std::endl;

	file.close();
}

void CKeybindApi::ReleaseAll()
{
	this->IsAltHeld = false;
	this->IsCtrlHeld = false;
	this->IsShiftHeld = false;

	//const std::lock_guard<std::mutex> lock(this->MutexHeldKeys);
	for (auto& bind : this->HeldKeybinds)
	{
		this->Invoke(bind.first, true);
	}

	this->HeldKeys.clear();
	this->HeldKeybinds.clear();
}
