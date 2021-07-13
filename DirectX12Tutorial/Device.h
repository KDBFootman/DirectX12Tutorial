
#pragma once

#include <windows.h>
#include <wrl/client.h>
#include <d3d12.h>
#include <dxgi1_6.h>


class Device
{
public:
	Device(HWND hWnd);
	~Device();

	void OnInit();
	void OnUpdate();
	void OnRender();
	void OnDestroy();

private:
	const static UINT FrameCount = 2;

	Microsoft::WRL::ComPtr<ID3D12Device> m_device;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_commandQueue;
	Microsoft::WRL::ComPtr<IDXGISwapChain3> m_swapChain;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_commandAllocator;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_commandList;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState;
	Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
	HWND m_hWnd;
	UINT m_frameIndex;
	UINT m_rtvDescriptorSize;

	UINT64 m_fenceValue;
	HANDLE m_fenceEvent;

	HRESULT LoadPipeline();
	HRESULT LoadAssets();
	HRESULT PopulateCommandList();
	HRESULT WaitForPreviousFrame();

};