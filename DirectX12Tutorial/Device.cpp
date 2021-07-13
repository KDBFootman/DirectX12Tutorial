
#include "Device.h"


//----------------------------------------------------------------------------------------------------
Device::Device(HWND hWnd)
	:m_device(nullptr)
	,m_commandQueue(nullptr)
	,m_swapChain(nullptr)
	,m_rtvHeap(nullptr)
	,m_renderTargets()
	,m_commandAllocator(nullptr)
	,m_commandList(nullptr)
	,m_pipelineState(nullptr)
	,m_fence(nullptr)
	,m_hWnd(hWnd)
	,m_frameIndex(0)
	,m_rtvDescriptorSize(0)
	,m_fenceValue(0)
	,m_fenceEvent()
{
}


//----------------------------------------------------------------------------------------------------
Device::~Device()
{

}


//----------------------------------------------------------------------------------------------------
void Device::OnInit()
{
	LoadPipeline();
	LoadAssets();
}


//----------------------------------------------------------------------------------------------------
void Device::OnUpdate()
{

}


//----------------------------------------------------------------------------------------------------
void Device::OnRender()
{
	// Record all the commands we need to render the scene into the command list.
	if (FAILED(PopulateCommandList()))
	{
		return;
	}

	// Execute the command list.
	ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	m_swapChain->Present(1, 0);

	WaitForPreviousFrame();

}


//----------------------------------------------------------------------------------------------------
void Device::OnDestroy()
{
	WaitForPreviousFrame();

	CloseHandle(m_fenceEvent);
}


//----------------------------------------------------------------------------------------------------
HRESULT Device::LoadPipeline()
{
	HRESULT hr;
	UINT dxgiFactoryFlags = 0;

	// Enable the debug layer.
#ifdef _DEBUG
	{
		Microsoft::WRL::ComPtr<ID3D12Debug> debugController;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
		{
			debugController->EnableDebugLayer();

			dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
		}
	}
#endif // _DEBUG

	Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
	hr = CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory));
	if (FAILED(hr))
	{
		return hr;
	}

	// Select available hardware adapter that supperts DirectX12.
	Microsoft::WRL::ComPtr<IDXGIAdapter1> hardwareAdapter;
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

	// Create the device.
	hr = D3D12CreateDevice(hardwareAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device));
	if (FAILED(hr))
	{
		return hr;
	}

	// Create the command queue.
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Priority;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.NodeMask;
	hr = m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue));
	if (FAILED(hr))
	{
		return hr;
	}

	RECT clientRect = {};
	if (!GetClientRect(m_hWnd, &clientRect))
	{
		return S_FALSE;
	}

	// Create the swap chain.
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.Width = clientRect.right - clientRect.left;
	swapChainDesc.Height = clientRect.bottom - clientRect.top;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.Stereo;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SampleDesc.Quality;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = FrameCount;
	swapChainDesc.Scaling;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.AlphaMode;
	swapChainDesc.Flags;

	Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain;
	hr = factory->CreateSwapChainForHwnd(m_commandQueue.Get(), m_hWnd, &swapChainDesc, nullptr, nullptr, &swapChain);
	if (FAILED(hr))
	{
		return hr;
	}

	hr = swapChain.As(&m_swapChain);
	if (FAILED(hr))
	{
		return hr;
	}

	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

	// Create descriptorHeaps.
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.NumDescriptors = FrameCount;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask;
	hr = m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap));
	if (FAILED(hr))
	{
		return hr;
	}

	m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	// Create frame resources.
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = {};
	rtvHandle.ptr = m_rtvHeap->GetCPUDescriptorHandleForHeapStart().ptr;

	for (UINT n = 0; n < FrameCount; n++)
	{
		hr = m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n]));
		if (FAILED(hr))
		{
			return hr;
		}
		m_device->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, rtvHandle);
		rtvHandle.ptr += m_rtvDescriptorSize;
	}

	// Create command allocator.
	hr = m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator));
	if (FAILED(hr))
	{
		return hr;
	}

	return S_OK;
}


//----------------------------------------------------------------------------------------------------
HRESULT Device::LoadAssets()
{
	HRESULT hr;

	// Create the command list.
	hr = m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_commandList));
	if (FAILED(hr))
	{
		return hr;
	}

	hr = m_commandList->Close();
	if (FAILED(hr))
	{
		return hr;
	}

	// Create synchronization objects.
	hr = m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
	if (FAILED(hr))
	{
		return hr;
	}
	m_fenceValue = 1;

	// Create an event handle to use for frame synchronization.
	m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (m_fenceEvent == nullptr)
	{
		return S_FALSE;
	}

	return S_OK;
}


//----------------------------------------------------------------------------------------------------
HRESULT Device::PopulateCommandList()
{
	HRESULT hr;

	hr = m_commandAllocator->Reset();
	if (FAILED(hr))
	{
		return hr;
	}

	hr = m_commandList->Reset(m_commandAllocator.Get(), m_pipelineState.Get());
	if (FAILED(hr))
	{
		return hr;
	}

	D3D12_RESOURCE_BARRIER resourceBarrier = {};
	resourceBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	resourceBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	resourceBarrier.Transition.pResource = m_renderTargets[m_frameIndex].Get();
	resourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	resourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	resourceBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	m_commandList->ResourceBarrier(1, &resourceBarrier);

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = {};
	rtvHandle.ptr = SIZE_T(INT64(m_rtvHeap->GetCPUDescriptorHandleForHeapStart().ptr) + INT64(m_frameIndex) * INT64(m_rtvDescriptorSize));

	// Record command.
	const float clearColor[] = { 0.0f,0.2f,0.4f,1.0f };
	m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

	resourceBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	resourceBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	resourceBarrier.Transition.pResource = m_renderTargets[m_frameIndex].Get();
	resourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	resourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	resourceBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	m_commandList->ResourceBarrier(1, &resourceBarrier);

	hr = m_commandList->Close();
	if (FAILED(hr))
	{
		return hr;
	}

	return S_OK;
}


//----------------------------------------------------------------------------------------------------
HRESULT Device::WaitForPreviousFrame()
{
	HRESULT hr;

	const UINT64 fence = m_fenceValue;
	hr = m_commandQueue->Signal(m_fence.Get(), fence);
	if (FAILED(hr))
	{
		return hr;
	}
	m_fenceValue++;

	if (m_fence->GetCompletedValue() < fence)
	{
		hr = m_fence->SetEventOnCompletion(fence, m_fenceEvent);
		if (FAILED(hr))
		{
			return hr;
		}
		WaitForSingleObject(m_fenceEvent, INFINITE);
	}

	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

	return S_OK;
}
