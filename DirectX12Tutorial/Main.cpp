
#include <windows.h>
#include <wrl/client.h>
#include <memory>
#include <d3d12.h>
#include <dxgi1_6.h>

#include <Device.h>


HWND g_hWnd = nullptr;

std::unique_ptr<Device> g_device;

HRESULT InitWindow(HINSTANCE hInstance, int nCmdShow);
LRESULT CALLBACK WndProc(HWND hWnd,UINT message,WPARAM wParam, LPARAM lParam);


int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int nCmdShow)
{
	// Initailize window
	if (FAILED(InitWindow(hInstance, nCmdShow)))
	{
		return 0;
	}

	// Main message loop
	MSG msg = {};
	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	g_device->OnDestroy();

	return (int)msg.wParam;
}


HRESULT InitWindow(HINSTANCE hInstance, int nCmdShow)
{
	HRESULT result = S_OK;

	// Initialize the window class.
	WNDCLASSEX windowClass = {};
	windowClass.cbSize = sizeof(WNDCLASSEX);
	windowClass.style = CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc = WndProc;
	windowClass.cbClsExtra = 0;
	windowClass.cbWndExtra = 0;
	windowClass.hInstance = hInstance;
	windowClass.hIcon = LoadIcon(hInstance, IDI_APPLICATION);
	windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	windowClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	windowClass.lpszMenuName = NULL;
	windowClass.lpszClassName = L"DX12TutorialClass";
	windowClass.hIconSm = LoadIcon(hInstance, IDI_APPLICATION);
	if (!RegisterClassEx(&windowClass))
	{
		return E_FAIL;
	}

	RECT windowRect = { 0,0,1280,720 };
	AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);
	g_hWnd = CreateWindowEx(0, windowClass.lpszClassName, L"DirectX12 Tutorial", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, windowRect.right - windowRect.left, windowRect.bottom - windowRect.top, nullptr, nullptr, hInstance, nullptr);
	if (!g_hWnd)
	{
		return E_FAIL;
	}

	g_device = std::make_unique<Device>(g_hWnd);
	g_device->OnInit();

	ShowWindow(g_hWnd, nCmdShow);

	return S_OK;
}


LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{

	switch (message)
	{
	case WM_PAINT:
		g_device->OnUpdate();
		g_device->OnRender();
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}
