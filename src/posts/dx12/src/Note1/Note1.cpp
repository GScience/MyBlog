#include "common/stdafx.h"
#include "common/Window.h"
#include "common/Dx12helper.h"
#include <algorithm>

constexpr UINT FrameCount = 2;

using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Details::ComPtrRef;
using DirectX::XMFLOAT3;
using DirectX::XMFLOAT4;

struct alignas(256) SceneConstantBuffer
{
    FLOAT time;
};

struct Vertex
{
    XMFLOAT3 position;
    XMFLOAT4 color;
};

struct DxContext {
    DWORD callbackCookie = 0;

    ComPtr<IDXGIFactory7> factory;
    ComPtr<IDXGIAdapter1> adapter;
    ComPtr<ID3D12Device10> device;
    ComPtr<ID3D12CommandQueue> commandQueue;
    ComPtr<IDXGISwapChain4> swapChain;

    ComPtr<ID3D12Resource> renderTargets[FrameCount];
    ComPtr<ID3D12DescriptorHeap> rtvHeap;
    ComPtr<ID3D12DescriptorHeap> cbvHeap;
    UINT rtvDescriptorSize = 0;

    ComPtr<ID3D12CommandAllocator> commandAllocator[FrameCount];
    ComPtr<ID3D12GraphicsCommandList> commandList;

    ComPtr<ID3D12Fence> fence;
    HANDLE fenceEvent = 0;
    UINT64 fenceValues[FrameCount]{ 0 };

    UINT curFrameIndex = 0;

    ComPtr<ID3D12RootSignature> rootSignature;
    ComPtr<ID3D12PipelineState> pipelineState;

    ComPtr<ID3D12Resource> vertexBuf;
    D3D12_VERTEX_BUFFER_VIEW vertexBufView;

    ComPtr<ID3D12Resource> constantBuf;
    void* cbvDataPtr;
};
std::unique_ptr<DxContext> ctx;

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
void OnD3dMessage(D3D12_MESSAGE_CATEGORY Category, D3D12_MESSAGE_SEVERITY Severity, D3D12_MESSAGE_ID ID, LPCSTR pDescription, void* pContext) {
    printf("[%d][%d]%d: %s\n", Category, Severity, ID, pDescription);
    if (Severity == D3D12_MESSAGE_SEVERITY_ERROR)
        __debugbreak();
}

static float clearColor[] = { 0.8f, 0.2f, 0.0f, 1.0f };

void SetupPipeline() {
    CD3DX12_DESCRIPTOR_RANGE1 ranges[1]{ };
    CD3DX12_ROOT_PARAMETER1 rootParameters[1]{};

    ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
    rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_VERTEX);

    D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 0, nullptr, rootSignatureFlags);

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
    ThrowIfFailed(ctx->device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&ctx->rootSignature)));

    ComPtr<ID3DBlob> vertexShader;
    ComPtr<ID3DBlob> pixelShader;
#if defined(_DEBUG)
    UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    UINT compileFlags = 0;
#endif
    ThrowIfFailed(D3DCompileFromFile(L"../shader.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, nullptr));
    ThrowIfFailed(D3DCompileFromFile(L"../shader.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, nullptr));

    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
    psoDesc.pRootSignature = ctx->rootSignature.Get();
    psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
    psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.DepthStencilState.StencilEnable = FALSE;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;
    ThrowIfFailed(ctx->device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&ctx->pipelineState)));
}

void UploadBuffer(void* dataPtr, UINT size, const ComPtr<ID3D12GraphicsCommandList>& cmdList, ComPtrRef<ComPtr<ID3D12Resource>> target, ComPtrRef<ComPtr<ID3D12Resource>> uploadBuf) {
    ThrowIfFailed(ctx->device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(size),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(uploadBuf)));
    ThrowIfFailed(ctx->device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(size),
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(target)));
    D3D12_SUBRESOURCE_DATA subResourceData = {};
    subResourceData.pData = dataPtr;
    subResourceData.RowPitch = size;
    subResourceData.SlicePitch = subResourceData.RowPitch;
    cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(*target, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));
    UpdateSubresources<1>(cmdList.Get(), *target, *uploadBuf, 0, 0, 1, &subResourceData);
    cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(*target, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ));
}

void UploadResources() {
    ctx->commandList->Reset(ctx->commandAllocator[ctx->curFrameIndex].Get(), nullptr);

    Vertex triangleVertices[] =
    {
        { { 0.0f, 0.8f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
        { { 0.8f, -0.8f, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
        { { -0.8f, -0.8f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } }
    };
    const UINT vertexBufferSize = sizeof(triangleVertices);
    ComPtr<ID3D12Resource> uploadBuf;
    UploadBuffer(triangleVertices, vertexBufferSize, ctx->commandList, &ctx->vertexBuf, &uploadBuf);
    ctx->vertexBufView.BufferLocation = ctx->vertexBuf->GetGPUVirtualAddress();
    ctx->vertexBufView.StrideInBytes = sizeof(Vertex);
    ctx->vertexBufView.SizeInBytes = vertexBufferSize;
    ThrowIfFailed(ctx->commandList->Close());
    ID3D12CommandList* ppCommandLists[] = { ctx->commandList.Get() };
    ctx->commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
    WaitForGpu();
}

void CreateConstanceBuffer() {
    const UINT constantBufferSize = sizeof(SceneConstantBuffer);

    ThrowIfFailed(ctx->device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(constantBufferSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&ctx->constantBuf)));

    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
    cbvDesc.BufferLocation = ctx->constantBuf->GetGPUVirtualAddress();
    cbvDesc.SizeInBytes = constantBufferSize;
    ctx->device->CreateConstantBufferView(&cbvDesc, ctx->cbvHeap->GetCPUDescriptorHandleForHeapStart());

    CD3DX12_RANGE readRange(0, 0);
    ThrowIfFailed(ctx->constantBuf->Map(0, &readRange, reinterpret_cast<void**>(&ctx->cbvDataPtr)));
    SceneConstantBuffer constantBufData{ 0 };
    memcpy(ctx->cbvDataPtr, &constantBufData, sizeof(constantBufData));
}

void Render(const Window* wnd) {
    ThrowIfFailed(ctx->commandAllocator[ctx->curFrameIndex]->Reset());
    ThrowIfFailed(ctx->commandList->Reset(ctx->commandAllocator[ctx->curFrameIndex].Get(), ctx->pipelineState.Get()));

    ctx->commandList->SetGraphicsRootSignature(ctx->rootSignature.Get());
    ID3D12DescriptorHeap* ppHeaps[] = { ctx->cbvHeap.Get() };
    ctx->commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
    ctx->commandList->SetGraphicsRootDescriptorTable(0, ctx->cbvHeap->GetGPUDescriptorHandleForHeapStart());
    D3D12_VIEWPORT vp{ 0, 0, static_cast<float>(wnd->GetWidth()), static_cast<float>(wnd->GetHeight())};
    ctx->commandList->RSSetViewports(1, &vp);
    D3D12_RECT sc{ 0, 0, static_cast<LONG>(wnd->GetWidth()), static_cast<LONG>(wnd->GetHeight()) };
    ctx->commandList->RSSetScissorRects(1, &sc);

    ctx->commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(ctx->renderTargets[ctx->curFrameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(ctx->rtvHeap->GetCPUDescriptorHandleForHeapStart(), ctx->curFrameIndex, ctx->rtvDescriptorSize);
    ctx->commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
    ctx->commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    ctx->commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->commandList->IASetVertexBuffers(0, 1, &ctx->vertexBufView);
    ctx->commandList->DrawInstanced(3, 1, 0, 0);

    ctx->commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(ctx->renderTargets[ctx->curFrameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
    ThrowIfFailed(ctx->commandList->Close());

    ID3D12CommandList* ppCommandLists[] = { ctx->commandList.Get() };
    ctx->commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
#if defined(ENABLE_VSYNC)
    ThrowIfFailed(ctx->swapChain->Present(1, 0));
#else
    ThrowIfFailed(ctx->swapChain->Present(0, DXGI_PRESENT_ALLOW_TEARING));
#endif
    MoveToNextFrame();
}

void Update(const Window* wnd) {
    static UINT64 startTime = GetTickCount64();
    static float lastRad[3]{ 0 };
    UINT64 curTime = GetTickCount64();
    SceneConstantBuffer constantBufData;
    lastRad[0] = std::fmod((curTime - startTime) / 996.0f, std::acos(-1));
    lastRad[1] = std::fmod((curTime - startTime) / 1009.0f, std::acos(-1));
    lastRad[2] = std::fmod((curTime - startTime) / 666.0f, std::acos(-1));
    clearColor[0] = std::sin(lastRad[0]) * 0.4f + 0.2f;
    clearColor[1] = std::sin(lastRad[1]) * 0.4f + 0.2f;
    clearColor[2] = std::sin(lastRad[2]) * 0.4f + 0.2f;
    constantBufData.time = (curTime - startTime) / 100.0f;
    memcpy(ctx->cbvDataPtr, &constantBufData, sizeof(constantBufData));
}

void Init(const Window* wnd) {
    UINT dxgiFactoryFlags = 0;
    // 启用调试层
#if defined(_DEBUG)
    ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        debugController->EnableDebugLayer();
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

    // 注册设备消息回调
#if defined(_DEBUG)
    ComPtr<ID3D12InfoQueue1> infoQueue;
    if (SUCCEEDED(ctx->device->QueryInterface(IID_PPV_ARGS(&infoQueue))))
        ThrowIfFailed(infoQueue->RegisterMessageCallback(OnD3dMessage, D3D12_MESSAGE_CALLBACK_FLAG_NONE, nullptr, &ctx->callbackCookie));
#endif

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
#if !defined(ENABLE_VSYNC)
    swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
#endif
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

        // CBV堆
        D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc = {};
        cbvHeapDesc.NumDescriptors = 1;
        cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        ThrowIfFailed(ctx->device->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&ctx->cbvHeap)));
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

    // 初始化管线
    SetupPipeline();
    // 创建CBV
    CreateConstanceBuffer();
    // 上传资源
    UploadResources();

    // 等待GPU同步
    WaitForGpu();
}

void CleanUp() {
    WaitForGpu();
    CloseHandle(ctx->fenceEvent);
}

int main() {
    auto wnd = std::make_unique<Window>(500, 500, L"Note 1");
    ctx = std::make_unique<DxContext>();
    wnd->Run(Init, Update, Render);
    CleanUp();
    ctx = nullptr;
    wnd = nullptr;
#if defined(_DEBUG)
    {
        ComPtr<IDXGIDebug1> debug;
        DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug));
        ThrowIfFailed(debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL));
    }
#endif
}