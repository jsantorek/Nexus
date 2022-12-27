#include <d3d11.h>
#include <PathCch.h>
#include <cassert>
#include <thread>

#include "core.h"
#include "Paths.h"
#include "State.h"
#include "Hooks.h"
#include "Renderer.h"
#include "Shared.h"

#include "minhook/mh_hook.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"

#include "Logging/LogHandler.h"
#include "Logging/ConsoleLogger.h"
#include "Logging/FileLogger.h"

#include "Mumble/Mumble.h"

#include "Keybinds/KeybindHandler.h"

#define IMPL_CHAINLOAD // arcdps workaround

/* handles */
HWND	hGW2_HWND	= nullptr;
HMODULE	hGW2		= nullptr;
HMODULE	hAddonHost	= nullptr;
HMODULE	hD3D11		= nullptr;
HMODULE	hSys11		= nullptr;

/* init/shutdown */
void InitializePaths()
{
	/* get self dll path */
	GetModuleFileNameW(hAddonHost, Path::F_HOST_DLL, MAX_PATH);

	/* get current directory */
	memcpy(Path::D_GW2, Path::F_HOST_DLL, MAX_PATH);
	PathCchRemoveFileSpec(Path::D_GW2, MAX_PATH);

	/* get temp dll path */
	memcpy(Path::F_TEMP_DLL, Path::D_GW2, MAX_PATH);
	PathCchAppend(Path::F_TEMP_DLL, MAX_PATH, L"d3d11.tmp");

	/* get chainload dll path */
	memcpy(Path::F_CHAINLOAD_DLL, Path::D_GW2, MAX_PATH);
	PathCchAppend(Path::F_CHAINLOAD_DLL, MAX_PATH, L"d3d11_chainload.dll");

	/* get system dll path */
	GetSystemDirectoryW(Path::F_SYSTEM_DLL, MAX_PATH);
	PathCchAppend(Path::F_SYSTEM_DLL, MAX_PATH, L"d3d11.dll");
}
void InitializeLogging()
{
	ConsoleLogger* cLog = new ConsoleLogger();
	cLog->SetLogLevel(ELogLevel::ALL);
	Logger->Register(cLog);

	FileLogger* fLog = new FileLogger("rcAddonHost.log");
	fLog->SetLogLevel(ELogLevel::INFO);
	Logger->Register(fLog);
}
void InitializeImGui()
{
	// create imgui context
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.Fonts->AddFontDefault();
	io.Fonts->Build();

	// Init imgui
	ImGui_ImplWin32_Init(hGW2_HWND);
	ImGui_ImplDX11_Init(Renderer::Device, Renderer::DeviceContext);
	ImGui::GetIO().ImeWindowHandle = hGW2_HWND;

	// create buffers
	ID3D11Texture2D* pBackBuffer;
	Renderer::SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
	Renderer::Device->CreateRenderTargetView(pBackBuffer, NULL, &Renderer::RenderTargetView);
	pBackBuffer->Release();

	State::IsImGuiInitialized = true;
}
void Initialize()
{
	InitializePaths();
	InitializeLogging();

	MH_Initialize();

	MumbleLink = Mumble::Create();
	KeybindHandler::LoadKeybinds();
}
void ShutdownImGui()
{
	if (State::IsImGuiInitialized)
	{
		State::IsImGuiInitialized = false;

		ImGui_ImplDX11_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
		if (Renderer::RenderTargetView) { Renderer::RenderTargetView->Release(); Renderer::RenderTargetView = NULL; }
	}
}
void Shutdown()
{
	if (!State::IsShutdown)
	{
		State::IsShutdown = true;

		ShutdownImGui();

		Mumble::Destroy();

		MH_Uninitialize();

		if (hD3D11) { FreeLibrary(hD3D11); }
	}
}

/* hk */
LRESULT __stdcall hkWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	// don't pass to game if keybind
	if (KeybindHandler::WndProc(hWnd, uMsg, wParam, lParam)) { return 0; }

	// don't pass to game if imgui
	if (State::IsImGuiInitialized)
	{
		ImGuiIO& io = ImGui::GetIO();
		if (io.WantCaptureMouse)
		{
			switch (uMsg)
			{
			case WM_LBUTTONDOWN:	io.MouseDown[0] = true;															return 0;
			case WM_RBUTTONDOWN:	io.MouseDown[1] = true;															return 0;

			case WM_LBUTTONUP:		io.MouseDown[0] = false;														break;
			case WM_RBUTTONUP:		io.MouseDown[1] = false;														break;

			case WM_MOUSEWHEEL:		io.MouseWheel += (float)GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA;	return 0;
			case WM_MOUSEHWHEEL:	io.MouseWheelH += (float)GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA;	return 0;
			}
		}

		if (io.WantTextInput)
		{
			switch (uMsg)
			{
			case WM_KEYDOWN:
			case WM_SYSKEYDOWN:
				if (wParam < 256)
					io.KeysDown[wParam] = 1;
				return 0;
			case WM_KEYUP:
			case WM_SYSKEYUP:
				if (wParam < 256)
					io.KeysDown[wParam] = 0;
				return 0;
			case WM_CHAR:
				// You can also use ToAscii()+GetKeyboardState() to retrieve characters.
				if (wParam > 0 && wParam < 0x10000)
					io.AddInputCharacterUTF16((unsigned short)wParam);
				return 0;
			}
		}
	}

	if (uMsg == WM_DESTROY)
	{
		Logger->LogCritical(L"::Destroy()");
	}

	return Hooks::GW2_WndProc(hWnd, uMsg, wParam, lParam);
}
HRESULT __stdcall hkDXGIPresent(IDXGISwapChain* pChain, UINT SyncInterval, UINT Flags)
{
	if (Renderer::SwapChain != pChain)
	{
		Renderer::SwapChain = pChain;

		if (Renderer::Device)
		{
			Renderer::DeviceContext->Release();
			Renderer::DeviceContext = 0;
			Renderer::Device->Release();
			Renderer::Device = 0;
		}

		Renderer::SwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&Renderer::Device);
		Renderer::Device->GetImmediateContext(&Renderer::DeviceContext);

		DXGI_SWAP_CHAIN_DESC swapChainDesc;
		Renderer::SwapChain->GetDesc(&swapChainDesc);

		hGW2_HWND = swapChainDesc.OutputWindow;
		Hooks::GW2_WndProc = (WNDPROC)SetWindowLongPtr(hGW2_HWND, GWLP_WNDPROC, (LONG_PTR)hkWndProc);
	}

	if (State::IsImGuiInitializable && !State::IsImGuiInitialized)
	{
		InitializeImGui();
	}

	if (State::IsImGuiInitialized)
	{
		// imgui define new frame
		ImGui_ImplWin32_NewFrame();
		ImGui_ImplDX11_NewFrame();
		ImGui::NewFrame();

		// Draw ImGui here!
		ImGui::ShowDemoWindow();

		// imgui end frame
		ImGui::EndFrame();

		// render
		ImGui::Render();

		// finish drawing
		Renderer::DeviceContext->OMSetRenderTargets(1, &Renderer::RenderTargetView, NULL);
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
	}

	return Hooks::DXGI_Present(pChain, SyncInterval, Flags);
}
HRESULT __stdcall hkDXGIResizeBuffers(IDXGISwapChain* pChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
{
	static const char* func_name = "hkDXGIResizeBuffers";
	Logger->Log(func_name);

	ShutdownImGui();

	return Hooks::DXGI_ResizeBuffers(pChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
}

/* dx */
BOOL DxLoad()
{
	if (!State::IsDxLoaded)
	{
		State::IsDxLoaded = true;

#ifdef IMPL_CHAINLOAD
		/* attempt to chainload */
		/* sanity check that the current dll isn't the chainload */
		if (Path::F_HOST_DLL != Path::F_CHAINLOAD_DLL)
		{
			Logger->Log(L"Attempting to chainload.");

			State::IsChainloading = true;

			hD3D11 = LoadLibraryW(Path::F_CHAINLOAD_DLL);
		}
#endif

		/* ifn chainload, load system dll */
		if (!hD3D11)
		{
#ifdef IMPL_CHAINLOAD
			if (State::IsChainloading)
			{
				Logger->Log(L"No chainload found or failed to load.");
			}
#endif
			State::IsChainloading = false;

			hD3D11 = LoadLibraryW(Path::F_SYSTEM_DLL);

			assert(hD3D11 && "Could not load system d3d11.dll");

			Logger->Log(L"Loaded System DLL: %s", Path::F_SYSTEM_DLL);
		}
	}

	return (hD3D11 != NULL);
}

HRESULT __stdcall D3D11CoreCreateDevice(IDXGIFactory* pFactory, IDXGIAdapter* pAdapter, UINT Flags, const D3D_FEATURE_LEVEL* pFeatureLevels, UINT FeatureLevels, ID3D11Device& ppDevice)
{
	static decltype(&D3D11CoreCreateDevice) func;
	static const char* func_name = "D3D11CoreCreateDevice";
	Logger->Log(func_name);

#ifdef IMPL_CHAINLOAD
	if (State::IsDxCreated)
	{
		Logger->LogWarning(L"DirectX Create already called. Chainload bounced back. Loading System d3d11.dll");

		hSys11 = LoadLibraryW(Path::F_SYSTEM_DLL);

		if (FindFunction(hSys11, &func, func_name) == false)
		{
			return 0;
		}
	}

	State::IsDxCreated = true;
#endif

	if (DxLoad() == false) { return 0; }

	if (func == 0)
	{
		if (FindFunction(hD3D11, &func, func_name) == false)
		{
			return 0;
		}
	}

	return func(pFactory, pAdapter, Flags, pFeatureLevels, FeatureLevels, ppDevice);
}
HRESULT __stdcall D3D11CreateDevice(IDXGIAdapter* pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags, const D3D_FEATURE_LEVEL* pFeatureLevels, UINT FeatureLevels, UINT SDKVersion, ID3D11Device** ppDevice, D3D_FEATURE_LEVEL* pFeatureLevel, ID3D11DeviceContext** ppImmediateContext)
{
	static decltype(&D3D11CreateDevice) func;
	static const char* func_name = "D3D11CreateDevice";
	Logger->Log(func_name);

#ifdef IMPL_CHAINLOAD
	if (State::IsDxCreated)
	{
		Logger->LogWarning(L"DirectX Create already called. Chainload bounced back. Loading System d3d11.dll");

		hSys11 = LoadLibraryW(Path::F_SYSTEM_DLL);

		if (FindFunction(hSys11, &func, func_name) == false)
		{
			return 0;
		}
	}

	State::IsDxCreated = true;
#endif

	if (DxLoad() == false) { return 0; }

	if (func == 0)
	{
		if (FindFunction(hD3D11, &func, func_name) == false)
		{
			return 0;
		}
	}

	WNDCLASSEXW wc;
	memset(&wc, 0, sizeof(wc));

	wc.cbSize = sizeof(wc);
	wc.lpfnWndProc = DefWindowProcW;
	wc.hInstance = GetModuleHandleW(0);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW);
	wc.lpszClassName = L"TempDxWndClass";
	RegisterClassExW(&wc);

	HWND wnd = CreateWindowExW(0, wc.lpszClassName, 0, WS_OVERLAPPEDWINDOW, 0, 0, 128, 128, 0, 0, wc.hInstance, 0);
	if (wnd) {
		DXGI_SWAP_CHAIN_DESC swap_desc = {};
		swap_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swap_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swap_desc.BufferCount = 1;
		swap_desc.SampleDesc.Count = 1;
		swap_desc.OutputWindow = wnd;
		swap_desc.Windowed = TRUE;

		ID3D11Device* device;
		ID3D11DeviceContext* context;
		IDXGISwapChain* swap;

		if (SUCCEEDED(D3D11CreateDeviceAndSwapChain(0, D3D_DRIVER_TYPE_HARDWARE, 0, 0, 0, 0, D3D11_SDK_VERSION, &swap_desc, &swap, &device, 0, &context))) {
			LPVOID* vtbl;

			vtbl = *(LPVOID**)swap;
			//dxgi_release = hook_vtbl_fn(vtbl, 2, dxgi_release_hook);
			//dxgi_present = hook_vtbl_fn(vtbl, 8, dxgi_present_hook);
			//dxgi_resize_buffers = hook_vtbl_fn(vtbl, 13, dxgi_resize_buffers_hook);

			MH_CreateHook(vtbl[8], (LPVOID)&hkDXGIPresent, (LPVOID*)&Hooks::DXGI_Present);
			MH_CreateHook(vtbl[13], (LPVOID)&hkDXGIResizeBuffers, (LPVOID*)&Hooks::DXGI_ResizeBuffers);
			MH_EnableHook(MH_ALL_HOOKS);

			context->Release();
			device->Release();
			swap->Release();
		}

		DestroyWindow(wnd);
	}

	UnregisterClassW(wc.lpszClassName, wc.hInstance);

	//std::thread([]() { Sleep(10000); g_IsImGuiInitializable = true; }).detach();
	State::IsImGuiInitializable = true;

	return func(pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion, ppDevice, pFeatureLevel, ppImmediateContext);
}
HRESULT __stdcall D3D11CreateDeviceAndSwapChain(IDXGIAdapter* pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags, const D3D_FEATURE_LEVEL* pFeatureLevels, UINT FeatureLevels, UINT SDKVersion, const DXGI_SWAP_CHAIN_DESC* pSwapChainDesc, IDXGISwapChain** ppSwapChain, ID3D11Device** ppDevice, D3D_FEATURE_LEVEL* pFeatureLevel, ID3D11DeviceContext** ppImmediateContext)
{
	static decltype(&D3D11CreateDeviceAndSwapChain) func;
	static const char* func_name = "D3D11CreateDeviceAndSwapChain";
	Logger->Log(func_name);

#ifdef IMPL_CHAINLOAD
	if (State::IsDxCreated)
	{
		Logger->LogWarning(L"DirectX Create already called. Chainload bounced back. Loading System d3d11.dll");

		hSys11 = LoadLibraryW(Path::F_SYSTEM_DLL);

		if (FindFunction(hSys11, &func, func_name) == false)
		{
			return 0;
		}
	}

	State::IsDxCreated = true;
#endif

	if (DxLoad() == false) { return 0; }

	if (func == 0)
	{
		if (FindFunction(hD3D11, &func, func_name) == false)
		{
			return 0;
		}
	}

	return func(pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion, pSwapChainDesc, ppSwapChain, ppDevice, pFeatureLevel, ppImmediateContext);
}

/* entry */
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		hAddonHost = hModule;
		hGW2 = GetModuleHandle(NULL);

		Initialize();

		Logger->LogDebug(L"%s %s", L"[ATTACH]", Path::F_HOST_DLL);
		Logger->LogInfo(L"Version: " __DATE__ " " __TIME__);
		break;
	case DLL_PROCESS_DETACH:
		//Logger->LogDebug(L"%s %s", L"[DETACH]", g_Path_HostDll);

		Shutdown();

		break;
	}
	return true;
}