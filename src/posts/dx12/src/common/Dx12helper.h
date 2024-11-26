#pragma once
#include "stdafx.h"
#include "Exception.h"

using Microsoft::WRL::ComPtr;

inline HRESULT GetHardwareAdapter(IDXGIFactory1* pFactory, IDXGIAdapter1** ppAdapter) {
    *ppAdapter = nullptr;

    ComPtr<IDXGIAdapter1> adapter;
    ComPtr<IDXGIFactory6> factory6;
    HRESULT ret = pFactory->QueryInterface(IID_PPV_ARGS(&factory6));
    if (FAILED(ret)) return ret;
    // 按照GPU性能枚举设备，并寻找第一台满足需求的设备
    for (
        UINT adapterIndex = 0;
        SUCCEEDED(factory6->EnumAdapterByGpuPreference(
            adapterIndex,
            DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
            IID_PPV_ARGS(&adapter)));
        ++adapterIndex) {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            continue;
        if (SUCCEEDED(D3D12CreateDevice(
            adapter.Get(),
            D3D_FEATURE_LEVEL_12_0,
            _uuidof(ID3D12Device),
            nullptr)))
            break;
    }
    *ppAdapter = adapter.Detach();
    return S_OK;
}