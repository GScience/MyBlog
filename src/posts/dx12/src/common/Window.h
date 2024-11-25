#pragma once
#include <iostream>
#include "stdafx.h"

using UpdateCallback = void(*)();
using RenderCallback = void(*)();

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
		}
		return 0;

		case WM_KEYDOWN:
			return 0;

		case WM_KEYUP:
			return 0;

		case WM_PAINT:
			window->_update();
			window->_render();
			return 0;

		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
		}
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	UINT _width, _height;
	HWND _hwnd;
	UpdateCallback _update;
	RenderCallback _render;
public:
	UINT GetWidth() {
		return _width;
	}
	UINT GetHeight() {
		return _height;
	}
	HWND GetHandle() {
		return _hwnd;
	}
	Window(UINT width, UINT height, LPCWSTR title) : _width(width), _height(height) {
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

	void MainLoop(UpdateCallback u, RenderCallback r) {
		_update = u;
		_render = r;
		ShowWindow(_hwnd, TRUE);
		MSG msg = {};
		while (msg.message != WM_QUIT)
		{
			// Process any messages in the queue.
			if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
	}

	~Window() {
		DestroyWindow(_hwnd);
	}
};