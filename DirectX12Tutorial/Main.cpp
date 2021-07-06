
#include <windows.h>
#include <wrl/client.h>
#include <d3d12.h>
#include <dxgi1_6.h>


HWND g_hWnd = nullptr;

const UINT g_frameCount = 2;

Microsoft::WRL::ComPtr<ID3D12Device> g_device = nullptr;
Microsoft::WRL::ComPtr<ID3D12CommandQueue> g_commandQueue = nullptr;
Microsoft::WRL::ComPtr<IDXGISwapChain3> g_swapChain = nullptr;
Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> g_rtvHeap = nullptr;
Microsoft::WRL::ComPtr<ID3D12Resource> g_renderTargets[g_frameCount];
Microsoft::WRL::ComPtr<ID3D12CommandAllocator> g_commandAllocator = nullptr;
Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> g_commandList = nullptr;
Microsoft::WRL::ComPtr<ID3D12PipelineState> g_pipelineState = nullptr;
Microsoft::WRL::ComPtr<ID3D12Fence> g_fence = nullptr;

UINT g_rtvDescriptorSize = 0;
UINT g_frameIndex = 0;
UINT64 g_fenceValue;
HANDLE g_fenceEvent;

HRESULT InitWindow(HINSTANCE hInstance, int nCmdShow);
LRESULT CALLBACK WndProc(HWND hWnd,UINT message,WPARAM wParam, LPARAM lParam);

void Update();
void Render();
void Cleanup();
void WaitForPreviousFrame();

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

	Cleanup();

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
	windowClass.hInstance = hInstance;
	windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	windowClass.lpszClassName = L"DX12TutorialClass";
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

	UINT dxgiFactoryFlags = 0;

#ifdef _DEBUG
	{
		// Enable the debug layer.
		Microsoft::WRL::ComPtr<ID3D12Debug> debugController;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
		{
			debugController->EnableDebugLayer();

			dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
		}
	}
#endif

	Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
	result = CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory));
	if (FAILED(result))
	{
		return result;
	}

	Microsoft::WRL::ComPtr<IDXGIAdapter1> hardwareAdapter;

	// Select available hardware adapter that supperts DirectX12.
	for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != factory->EnumAdapters1(adapterIndex, &hardwareAdapter); ++adapterIndex)
	{
		DXGI_ADAPTER_DESC1 desc;
		hardwareAdapter->GetDesc1(&desc);

		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
		{
			continue;
		}

		if (SUCCEEDED(D3D12CreateDevice(hardwareAdapter.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)))
		{
			break;
		}
	}
 
	// Create device.
	result = D3D12CreateDevice(hardwareAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&g_device));
	if (FAILED(result))
	{
		return result;
	}

	// Create the command queue.
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	result = g_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&g_commandQueue));
	if (FAILED(result))
	{
		return result;
	}

	// Create the swap chain.
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.BufferCount = g_frameCount;
	swapChainDesc.Width = windowRect.right - windowRect.left;
	swapChainDesc.Height = windowRect.bottom - windowRect.top;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.SampleDesc.Count = 1;

	Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain;

	result = factory->CreateSwapChainForHwnd(g_commandQueue.Get(), g_hWnd, &swapChainDesc, nullptr, nullptr, &swapChain);
	if (FAILED(result))
	{
		return result;
	}

	result = swapChain.As(&g_swapChain);
	if (FAILED(result))
	{
		return result;
	}
	g_frameIndex = g_swapChain->GetCurrentBackBufferIndex();

	// Create descriptor heaps.
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
	rtvHeapDesc.NumDescriptors = g_frameCount;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	result = g_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&g_rtvHeap));
	if (FAILED(result))
	{
		return result;
	}

	g_rtvDescriptorSize = g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	// Create frame resources.
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle(g_rtvHeap->GetCPUDescriptorHandleForHeapStart());

	for (UINT n = 0; n < g_frameCount; n++)
	{
		result = g_swapChain->GetBuffer(n, IID_PPV_ARGS(&g_renderTargets[n]));
		if (FAILED(result))
		{
			return result;
		}
		g_device->CreateRenderTargetView(g_renderTargets[n].Get(), nullptr, rtvHandle);
		rtvHandle.ptr += g_rtvDescriptorSize;
	}

	// Create command allocator.
	result = g_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&g_commandAllocator));
	if (FAILED(result))
	{
		return result;
	}

	// Create the command list.
	result = g_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_commandAllocator.Get(), nullptr, IID_PPV_ARGS(&g_commandList));
	if (FAILED(result))
	{
		return result;
	}

	result = g_commandList->Close();
	if (FAILED(result))
	{
		return result;
	}

	// Create synchronization objects.
	result = g_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_fence));
	if (FAILED(result))
	{
		return result;
	}
	g_fenceValue = 1;

	g_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (g_fenceEvent == nullptr)
	{
		return S_FALSE;
	}

	ShowWindow(g_hWnd, nCmdShow);

	return S_OK;
}


LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{

	switch (message)
	{
	case WM_PAINT:
		Update();
		Render();
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}


void Update()
{

}


void Render()
{
	g_commandAllocator->Reset();

	g_commandList->Reset(g_commandAllocator.Get(), g_pipelineState.Get());

	D3D12_RESOURCE_BARRIER resourceBarrir = {};
	resourceBarrir.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	resourceBarrir.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	resourceBarrir.Transition.pResource = g_renderTargets[g_frameIndex].Get();
	resourceBarrir.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	resourceBarrir.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	resourceBarrir.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	g_commandList->ResourceBarrier(1, &resourceBarrir);

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = {};
	rtvHandle.ptr = SIZE_T(INT64(g_rtvHeap->GetCPUDescriptorHandleForHeapStart().ptr) + INT64(g_frameIndex) * INT64(g_rtvDescriptorSize));

	const float clearColor[] = { 0.0f,0.2f,0.4f,1.0f };
	g_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

	D3D12_RESOURCE_BARRIER newResourceBarrir = {};
	newResourceBarrir.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	newResourceBarrir.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	newResourceBarrir.Transition.pResource = g_renderTargets[g_frameIndex].Get();
	newResourceBarrir.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	newResourceBarrir.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	newResourceBarrir.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	g_commandList->ResourceBarrier(1, &newResourceBarrir);

	g_commandList->Close();

	// Execute the command list.
	ID3D12CommandList* ppCommandLists[] = { g_commandList.Get() };
	g_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	g_swapChain->Present(1, 0);

	WaitForPreviousFrame();

}


void Cleanup()
{
	WaitForPreviousFrame();

	CloseHandle(g_fenceEvent);
}


void WaitForPreviousFrame()
{
	const UINT64 fence = g_fenceValue;
	g_commandQueue->Signal(g_fence.Get(), fence);
	g_fenceValue++;

	if (g_fence->GetCompletedValue() < fence)
	{
		g_fence->SetEventOnCompletion(fence, g_fenceEvent);
		WaitForSingleObject(g_fenceEvent, INFINITE);
	}

	g_frameIndex = g_swapChain->GetCurrentBackBufferIndex();

}
