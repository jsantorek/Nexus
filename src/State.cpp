#include "State.h"

namespace State
{
	ENexusState		Nexus						= ENexusState::NONE;
	EDxState		Directx						= EDxState::NONE;
	EEntryMethod	EntryMethod					= EEntryMethod::NONE;
	EMultiboxState	MultiboxState				= EMultiboxState::NONE;
	bool			IsChainloading				= false;
	bool			IsImGuiInitialized			= false;

	bool			IsDeveloperMode				= false;
	bool			IsConsoleEnabled			= false;
	bool			IsVanilla					= false;
	bool			IsMumbleDisabled			= false;

	void Initialize()
	{
		bool first = true;
		bool customMumble = false;

		int argc;
		LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);

		for (int i = 0; i < argc; i++)
		{
			std::wstring paramW = argv[i];
			std::string token = WStrToStr(paramW);
			
			std::string cmp = token;

			if (first) { first = false; continue; } // skip location param

			std::transform(cmp.begin(), cmp.end(), cmp.begin(), ::tolower);

			// token -> is the unmodified string as from the commandline
			// cmp -> same as token, except it's been normalised (aka written in lowercase)

			if (cmp == "-ggdev") { IsDeveloperMode = true; }
			if (cmp == "-ggconsole") { IsConsoleEnabled = true; }
			if (cmp == "-ggvanilla") { IsVanilla = true; }
			if (cmp == "-sharearchive") { MultiboxState = MultiboxState | EMultiboxState::ARCHIVE_SHARED; }
			if (cmp == "-multi") { MultiboxState = MultiboxState | EMultiboxState::LOCAL_SHARED; }

			if (cmp == "-mumble" && i + 1 <= argc)
			{
				std::wstring mumbleNameW = argv[i + 1];
				std::string mumbleName = WStrToStr(mumbleNameW);

				customMumble = true;
				MumbleLink = (LinkedMem*)DataLink::ShareResource(DL_MUMBLE_LINK, sizeof(LinkedMem), mumbleName.c_str());

				token.append(" ");
				token.append(mumbleName);
				Parameters.push_back(token);
				i++; // manual increment to skip
			}
			else
			{
				Parameters.push_back(token);
			}
		}

		///////////////////////////////////

		if (!customMumble)
		{
			MumbleLink = (LinkedMem*)DataLink::ShareResource(DL_MUMBLE_LINK, sizeof(LinkedMem), "MumbleLink");
		}

		// TODO:
		// close "AN-Mutex-Window-Guild Wars 2"
	}
}

EMultiboxState operator|(EMultiboxState lhs, EMultiboxState rhs)
{
	return static_cast<EMultiboxState>(static_cast<int>(lhs) | static_cast<int>(rhs));
}
EMultiboxState operator&(EMultiboxState lhs, EMultiboxState rhs)
{
	return static_cast<EMultiboxState>( static_cast<int>(lhs) & static_cast<int>(rhs) );
}