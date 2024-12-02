#pragma once
#include <iostream>
#include "stdafx.h"

class Window;
using InitCallback = void(*)(const Window* wnd);
using UpdateCallback = void(*)(const Window* wnd);
using RenderCallback = void(*)(const Window* wnd);

struct WindowInput {
    std::atomic_bool vKeys[0xFF];
    std::atomic_bool lMouseButton;
    std::atomic_bool rMouseButton;
    std::atomic_uint32_t mousePos;
    POINTS GetMousePos() const {
        return MAKEPOINTS(mousePos);
    }
};

class Window {
    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        Window* window = reinterpret_cast<Window*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
        switch (message)
        {
        case WM_CREATE:
        {
            LPCREATESTRUCT pCreateStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);
            SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pCreateStruct->lpCreateParams));
            return 0;
        }
        case WM_KEYDOWN:
            window->_cacheInput.vKeys[wParam] = true;
            return 0;
        case WM_KEYUP:
            window->_cacheInput.vKeys[wParam] = false;
            return 0;
        case WM_MOUSEMOVE:
            window->_cacheInput.mousePos = lParam;
            return 0;
        case WM_LBUTTONDOWN:
            window->_cacheInput.lMouseButton = true;
            return 0;
        case WM_LBUTTONUP:
            window->_cacheInput.lMouseButton = false;
            return 0;
        case WM_RBUTTONDOWN:
            window->_cacheInput.rMouseButton = true;
            return 0;
        case WM_RBUTTONUP:
            window->_cacheInput.rMouseButton = false;
            return 0;
        case WM_MOUSELEAVE:
            window->_cacheInput.lMouseButton = false;
            window->_cacheInput.rMouseButton = false;
            return 0;
        case WM_CLOSE:
            window->_shouldWindowClose = true;
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    void MessageLoop() {
        _shouldWindowClose = false;
        MSG msg = {};
        while (GetMessage(&msg, _hwnd, 0, 0) != 0 && !_shouldWindowClose) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    void GameLoop() {
        while (!_shouldWindowClose) {
            CopyMemory(&_curFrameInput, &_cacheInput, sizeof(WindowInput));
            _update(this);
            _render(this);
        }
    }
    static DWORD WINAPI GameThread(LPVOID param) {
        reinterpret_cast<Window*>(param)->_init(reinterpret_cast<Window*>(param));
        reinterpret_cast<Window*>(param)->GameLoop();
        return 0;
    }
    UINT _width, _height;
    HWND _hwnd;
    InitCallback _init;
    UpdateCallback _update;
    RenderCallback _render;
    std::atomic_bool _shouldWindowClose = false;
    WindowInput _cacheInput;
    WindowInput _curFrameInput;
public:
    UINT GetWidth() const {
        return _width;
    }
    UINT GetHeight() const {
        return _height;
    }
    HWND GetHandle() const {
        return _hwnd;
    }
    const WindowInput& GetInput() const {
        return _curFrameInput;
    }
    Window(UINT width, UINT height, LPCWSTR title) : _width(width), _height(height), _init(nullptr), _update(nullptr), _render(nullptr) {
        HINSTANCE hInstance = GetModuleHandle(nullptr);
        WNDCLASSEX windowClass = { 0 };
        windowClass.cbSize = sizeof(WNDCLASSEX);
        windowClass.style = CS_HREDRAW | CS_VREDRAW;
        windowClass.lpfnWndProc = Window::WindowProc;
        windowClass.hInstance = hInstance;
        windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
        windowClass.lpszClassName = L"DXSampleClass";
        RegisterClassEx(&windowClass);

        RECT windowRect = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
        AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);
        _hwnd = CreateWindow(
            windowClass.lpszClassName,
            title,
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            windowRect.right - windowRect.left,
            windowRect.bottom - windowRect.top,
            nullptr,
            nullptr,
            hInstance,
            this);
    }
    void Run(InitCallback i, UpdateCallback u, RenderCallback r) {
        _init = i;
        _update = u;
        _render = r;
        ShowWindow(_hwnd, TRUE);
        auto gameThread = CreateThread(nullptr, 0, GameThread, this, 0, nullptr);
        if (gameThread == 0) throw std::exception("Failed to create render thread");
        MessageLoop();
        WaitForSingleObject(gameThread, INFINITE);
        ShowWindow(_hwnd, FALSE);
        CloseHandle(gameThread);
    }

    ~Window() {
        DestroyWindow(_hwnd);
    }
};