#include "common/stdafx.h"
#include "common/Window.h"
#include "common/Dx12helper.h"
#include <algorithm>

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
    UINT rtvDescriptorSize = 0;

    ComPtr<ID3D12CommandAllocator> commandAllocator[FrameCount];
    ComPtr<ID3D12GraphicsCommandList> commandList;

    ComPtr<ID3D12Fence> fence;
    HANDLE fenceEvent = 0;
    UINT64 fenceValues[FrameCount]{ 0 };

    UINT curFrameIndex = 0;
};
std::unique_ptr<Window> wnd;
std::unique_ptr<DxContext> ctx;

void EnableDebugLayer() {
    ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        debugController->EnableDebugLayer();
}

void WaitForGpu() {
    ThrowIfFailed(ctx->commandQueue->Signal(ctx->fence.Get(), ctx->fenceValues[ctx->curFrameIndex]));
    ThrowIfFailed(ctx->fence->SetEventOnCompletion(ctx->fenceValues[ctx->curFrameIndex], ctx->fenceEvent));
    WaitForSingleObjectEx(ctx->fenceEvent, INFINITE, FALSE);
    ctx->fenceValues[ctx->curFrameIndex]++;
}

void MoveToNextFrame() {
    const UINT64 currentFenceValue = ctx->fenceValues[ctx->curFrameIndex];
    ThrowIfFailed(ctx->commandQueue->Signal(ctx->fence.Get(), currentFenceValue));
    ctx->curFrameIndex = ctx->swapChain->GetCurrentBackBufferIndex();
    if (ctx->fence->GetCompletedValue() < ctx->fenceValues[ctx->curFrameIndex]) {
        ThrowIfFailed(ctx->fence->SetEventOnCompletion(ctx->fenceValues[ctx->curFrameIndex], ctx->fenceEvent));
        WaitForSingleObjectEx(ctx->fenceEvent, INFINITE, FALSE);
    }
    ctx->fenceValues[ctx->curFrameIndex] = currentFenceValue + 1;
}

static float clearColor[] = { 0.8f, 0.2f, 0.0f, 1.0f };

void Render() {
    ThrowIfFailed(ctx->commandAllocator[ctx->curFrameIndex]->Reset());
    ThrowIfFailed(ctx->commandList->Reset(ctx->commandAllocator[ctx->curFrameIndex].Get(), nullptr));

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(ctx->rtvHeap->GetCPUDescriptorHandleForHeapStart(), ctx->curFrameIndex, ctx->rtvDescriptorSize);
    ctx->commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(ctx->renderTargets[ctx->curFrameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    ctx->commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    ctx->commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(ctx->renderTargets[ctx->curFrameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
    ThrowIfFailed(ctx->commandList->Close());

    ID3D12CommandList* ppCommandLists[] = { ctx->commandList.Get() };
    ctx->commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
    ThrowIfFailed(ctx->swapChain->Present(0, DXGI_PRESENT_ALLOW_TEARING));
    MoveToNextFrame();
}

void Update() {
    static UINT64 lastTime = GetTickCount64();
    static float lastRad[3]{ 0 };
    UINT64 curTime = GetTickCount64();
    lastRad[0] = std::fmod(lastRad[0] + (curTime - lastTime) / 996.0f, std::acos(-1));
    lastRad[1] = std::fmod(lastRad[1] + (curTime - lastTime) / 1009.0f, std::acos(-1));
    lastRad[2] = std::fmod(lastRad[2] + (curTime - lastTime) / 666.0f, std::acos(-1));
    clearColor[0] = std::sin(lastRad[0]) * 0.4f + 0.2f;
    clearColor[1] = std::sin(lastRad[1]) * 0.4f + 0.2f;
    clearColor[2] = std::sin(lastRad[2]) * 0.4f + 0.2f;
    lastTime = curTime;
}

void Init() {
    UINT dxgiFactoryFlags = 0;
    // 启用调试层
#if defined(_DEBUG)
    EnableDebugLayer();
    dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

    // 创建设备
    ThrowIfFailed(CreateDXGIFactory2(
        dxgiFactoryFlags,
        IID_PPV_ARGS(&ctx->factory)));
    ThrowIfFailed(GetHardwareAdapter(
        ctx->factory.Get(),
        &ctx->adapter));
    ThrowIfFailed(D3D12CreateDevice(
        ctx->adapter.Get(),
        D3D_FEATURE_LEVEL_12_0,
        IID_PPV_ARGS(&ctx->device)
    ));

    // 创建命令队列
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ThrowIfFailed(ctx->device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&ctx->commandQueue)));

    // 创建交换链
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = FrameCount;
    swapChainDesc.Width = wnd->GetWidth();
    swapChainDesc.Height = wnd->GetHeight();
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
    ComPtr<IDXGISwapChain1> swapChain1;
    ThrowIfFailed(ctx->factory->CreateSwapChainForHwnd(
        ctx->commandQueue.Get(),
        wnd->GetHandle(),
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain1
    ));
    ThrowIfFailed(swapChain1.As(&ctx->swapChain));
    // 禁止全屏切换
    ThrowIfFailed(ctx->factory->MakeWindowAssociation(wnd->GetHandle(), DXGI_MWA_NO_ALT_ENTER));

    // 创建描述符堆
    {
        // RTV堆
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
        rtvHeapDesc.NumDescriptors = FrameCount;
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(ctx->device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&ctx->rtvHeap)));
    }
    ctx->rtvDescriptorSize = ctx->device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // 填充RTV堆
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(ctx->rtvHeap->GetCPUDescriptorHandleForHeapStart());
    for (UINT n = 0; n < FrameCount; n++) {
        ThrowIfFailed(ctx->swapChain->GetBuffer(n, IID_PPV_ARGS(&ctx->renderTargets[n])));
        ctx->device->CreateRenderTargetView(ctx->renderTargets[n].Get(), nullptr, rtvHandle);
        rtvHandle.Offset(1, ctx->rtvDescriptorSize);
    }

    // 为每一个RTV创建Command Allocator
    for (UINT n = 0; n < FrameCount; n++)
        ThrowIfFailed(ctx->device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&ctx->commandAllocator[n])));

    // 创建CommandList
    ctx->device->CreateCommandList1(0, D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&ctx->commandList));

    // 创建Fence
    ctx->device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&ctx->fence));
    ctx->fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (ctx->fenceEvent == nullptr)
        ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
    ctx->fenceValues[ctx->curFrameIndex]++;

    // 等待GPU同步
    WaitForGpu();
}

int main() {
    wnd = std::make_unique<Window>(500, 500, L"Note 1");
    ctx = std::make_unique<DxContext>();
    Init();
    wnd->MainLoop(Update, Render);
    WaitForGpu();
    CloseHandle(ctx->fenceEvent);
    ctx = nullptr;
    wnd = nullptr;
}