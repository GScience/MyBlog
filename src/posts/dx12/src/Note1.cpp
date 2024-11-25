#include "common/stdafx.h"
#include "common/Window.h"
#include "common/Dx12helper.h"

constexpr UINT FrameCount = 2;

using Microsoft::WRL::ComPtr;

struct DxContext {
	ComPtr<IDXGIFactory7> factory;
	ComPtr<IDXGIAdapter1> adapter;
	ComPtr<ID3D12Device10> device;
	ComPtr<ID3D12CommandQueue> commandQueue;
	ComPtr<IDXGISwapChain4> swapChain;
	ComPtr<ID3D12Resource> renderTargets[FrameCount];
	ComPtr<ID3D12DescriptorHeap> rtvHeap;
	ComPtr<ID3D12CommandAllocator> commandAllocator;
};
DxContext ctx{ 0 };

void EnableDebugLayer() {
	ComPtr<ID3D12Debug> debugController;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
		debugController->EnableDebugLayer();
}

void Render() {

}

void Update() {

}

int main() {
	UINT dxgiFactoryFlags = 0;

	// 创建窗体
	Window window(500, 500, L"Note 1");

	// 启用调试层
#if defined(_DEBUG)
	EnableDebugLayer();
	dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

	// 创建设备
	ThrowIfFailed(CreateDXGIFactory2(
		dxgiFactoryFlags, 
		IID_PPV_ARGS(&ctx.factory)));
	ThrowIfFailed(GetHardwareAdapter(
		ctx.factory.Get(), 
		&ctx.adapter));
	ThrowIfFailed(D3D12CreateDevice(
		ctx.adapter.Get(),
		D3D_FEATURE_LEVEL_12_0,
		IID_PPV_ARGS(&ctx.device)
	));

	// 创建命令队列
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	ThrowIfFailed(ctx.device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&ctx.commandQueue)));

	// 创建交换链
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.BufferCount = FrameCount;
	swapChainDesc.Width = window.GetWidth();
	swapChainDesc.Height = window.GetHeight();
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.SampleDesc.Count = 1;
	ComPtr<IDXGISwapChain1> swapChain1;
	ThrowIfFailed(ctx.factory->CreateSwapChainForHwnd(
		ctx.commandQueue.Get(),
		window.GetHandle(),
		&swapChainDesc,
		nullptr,
		nullptr,
		&swapChain1
	));
	ThrowIfFailed(swapChain1.As(&ctx.swapChain));
	// 禁止全屏切换
	ThrowIfFailed(ctx.factory->MakeWindowAssociation(window.GetHandle(), DXGI_MWA_NO_ALT_ENTER));

	// 创建描述符堆
	{
		// RTV堆
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
		rtvHeapDesc.NumDescriptors = FrameCount;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		ThrowIfFailed(ctx.device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&ctx.rtvHeap)));
	}
	UINT rtvDescriptorSize = ctx.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	// 创建RTV
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(ctx.rtvHeap->GetCPUDescriptorHandleForHeapStart());

		// 为每一帧创建RTV
		for (UINT n = 0; n < FrameCount; n++)
		{
			ThrowIfFailed(ctx.swapChain->GetBuffer(n, IID_PPV_ARGS(&ctx.renderTargets[n])));
			ctx.device->CreateRenderTargetView(ctx.renderTargets[n].Get(), nullptr, rtvHandle);
			rtvHandle.Offset(1, rtvDescriptorSize);
		}
	}
	ThrowIfFailed(ctx.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&ctx.commandAllocator)));
	window.MainLoop(Update, Render);
}