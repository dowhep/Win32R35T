#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tchar.h>
#include <string>
#include <stdlib.h>
#include <d3d11.h>       // D3D interface
#include <dxgi.h>        // DirectX driver interface
#include <d3dcompiler.h> // shader compiler
#include <assert.h>		// assert check and crash when necessary
#include <winuser.h>

#pragma comment( lib, "user32" )          // link against the win32 library
#pragma comment( lib, "d3d11.lib" )       // direct3D library
#pragma comment( lib, "dxgi.lib" )        // directx graphics interface
#pragma comment( lib, "d3dcompiler.lib" ) // shader compiler

static TCHAR szWindowClass[] = _T("DesktopApp");
static TCHAR szTitle[] = _T("Windows Desktop Guided Tour Application");

HINSTANCE hInst;

// struct
struct float2 { float x, y; };
struct int2 { int x, y; };

// predefined functions
float2 GetMousePosition(HWND hWnd);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

// Global Variables
ID3D11Device* device_ptr = NULL;
ID3D11DeviceContext* device_context_ptr = NULL;
IDXGISwapChain* swap_chain_ptr = NULL;
ID3D11RenderTargetView* render_target_view_ptr = NULL;
int intWWidth = 384;
int intWHeight = 432;

int WINAPI WinMain(
	_In_ HINSTANCE	hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPSTR		lpCmdLine,
	_In_ int		nCmdShow)
{
	WNDCLASSEX wcex;

	wcex.cbSize			= sizeof(WNDCLASSEX);
	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon(wcex.hInstance, IDI_APPLICATION);
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName	= NULL;
	wcex.lpszClassName	= szWindowClass;
	wcex.hIconSm		= LoadIcon(wcex.hInstance, IDI_APPLICATION);

	if (!RegisterClassEx(&wcex)) {
		MessageBox(NULL,
			_T("Call to RegisterClassEx failed!"),
			_T("Windows Desktop Guided Tour"),
			NULL);

		return 1;
	}

	// store to global var
	hInst = hInstance;

	// create window handle (windows size/options)
	HWND hWnd = CreateWindowEx(
		WS_EX_OVERLAPPEDWINDOW,
		szWindowClass,
		szTitle,
		WS_OVERLAPPED | WS_SYSMENU | WS_MINIMIZEBOX,
		CW_USEDEFAULT, CW_USEDEFAULT,
		intWWidth, intWHeight,
		NULL,
		NULL,
		hInstance,
		NULL
	);
	if (!hWnd) {
		MessageBox(NULL,
			_T("Call to CreateWindowEx failed!"),
			_T("Windows Desktop Guided Tour"),
			NULL);

		return 1;
	}

	// make window visible
	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	// create basic swapchain setup
	DXGI_SWAP_CHAIN_DESC swap_chain_descr = { 0 };
	swap_chain_descr.BufferDesc.RefreshRate.Numerator = 0;
	swap_chain_descr.BufferDesc.RefreshRate.Denominator = 1;
	swap_chain_descr.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	swap_chain_descr.SampleDesc.Count = 1;
	swap_chain_descr.SampleDesc.Quality = 0;
	swap_chain_descr.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swap_chain_descr.BufferCount = 1;
	swap_chain_descr.OutputWindow = hWnd;
	swap_chain_descr.Windowed = true;

	D3D_FEATURE_LEVEL feature_level;
	UINT flagsD = D3D11_CREATE_DEVICE_SINGLETHREADED;
#if defined( DEBUG ) || defined( _DEBUG )
	flagsD |= D3D11_CREATE_DEVICE_DEBUG; // debug outputs
#endif

	HRESULT hr = D3D11CreateDeviceAndSwapChain(
		NULL,
		D3D_DRIVER_TYPE_HARDWARE,
		NULL,
		flagsD,
		NULL,
		0,
		D3D11_SDK_VERSION,
		&swap_chain_descr,
		&swap_chain_ptr,
		&device_ptr,
		&feature_level,
		&device_context_ptr);
	assert(S_OK == hr && swap_chain_ptr && device_ptr && device_context_ptr);
	
	// fetch view pointers
	ID3D11Texture2D* framebuffer;
	hr = swap_chain_ptr->GetBuffer(
		0,
		__uuidof(ID3D11Texture2D),
		(void**)&framebuffer);
	assert(SUCCEEDED(hr));

	hr = device_ptr->CreateRenderTargetView(
		framebuffer, 0, &render_target_view_ptr);
	assert(SUCCEEDED(hr));
	framebuffer->Release();

	// compile shaders
	UINT flagsS = D3DCOMPILE_ENABLE_STRICTNESS; 
#if defined( DEBUG ) || defined( _DEBUG )
	flagsS |= D3DCOMPILE_DEBUG; // add more debug output
#endif
	ID3DBlob* vs_blob_ptr = NULL, * ps_blob_ptr = NULL, * error_blob = NULL;

	// COMPILE VERTEX SHADER
	HRESULT hrC = D3DCompileFromFile(
		L"shaders.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"vs_main",
		"vs_5_0",
		flagsS,
		0,
		&vs_blob_ptr,
		&error_blob);
	if (FAILED(hrC)) {
		if (error_blob) {
			OutputDebugStringA((char*)error_blob->GetBufferPointer());
			error_blob->Release();
		}
		if (vs_blob_ptr) { vs_blob_ptr->Release(); }
		assert(false);
	}

	// COMPILE PIXEL SHADER
	hrC = D3DCompileFromFile(
		L"shaders.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"ps_main",
		"ps_5_0",
		flagsS,
		0,
		&ps_blob_ptr,
		&error_blob);
	if (FAILED(hrC)) {
		if (error_blob) {
			OutputDebugStringA((char*)error_blob->GetBufferPointer());
			error_blob->Release();
		}
		if (ps_blob_ptr) { ps_blob_ptr->Release(); }
		assert(false);
	}

	// create shaders
	ID3D11VertexShader* vertex_shader_ptr = NULL;
	ID3D11PixelShader* pixel_shader_ptr = NULL;

	hr = device_ptr->CreateVertexShader(
		vs_blob_ptr->GetBufferPointer(),
		vs_blob_ptr->GetBufferSize(),
		NULL,
		&vertex_shader_ptr);
	assert(SUCCEEDED(hr));

	hr = device_ptr->CreatePixelShader(
		ps_blob_ptr->GetBufferPointer(),
		ps_blob_ptr->GetBufferSize(),
		NULL,
		&pixel_shader_ptr);
	assert(SUCCEEDED(hr));

	// create input layout
	ID3D11InputLayout* input_layout_ptr = NULL;
	D3D11_INPUT_ELEMENT_DESC inputElementDesc[] = {
		// POS is the POS in shaders
	  { "POS", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	  /*
	  { "COL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	  { "NOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	  */
	  { "TEX", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	hr = device_ptr->CreateInputLayout(
		inputElementDesc,
		ARRAYSIZE(inputElementDesc),
		vs_blob_ptr->GetBufferPointer(),
		vs_blob_ptr->GetBufferSize(),
		&input_layout_ptr);
	assert(SUCCEEDED(hr));

	// create vertex points (clockwise)
	float vertex_data_array[] = {
  -1.0f, -1.0f,  0.0f, 0.0f, 0.0f, // point at bottom left
   -1.0f,  1.0f,  0.0f, 0.0f, 1.0f, // point at top left
   1.0f, 1.0f,  0.0f, 1.0f, 1.0f, // point at top right 
  -1.0f, -1.0f,  0.0f, 0.0f, 0.0f, // point at bottom left
   1.0f, 1.0f,  0.0f, 1.0f, 1.0f, // point at top right
   1.0f,  -1.0f,  0.0f, 1.0f, 0.0f, // point at bottom right
	};
	UINT vertex_stride = 5 * sizeof(float);
	UINT vertex_offset = 0;
	UINT vertex_count = 6;

	// load vertices
	ID3D11Buffer* vertex_buffer_ptr = NULL;
	{ /*** load mesh data into vertex buffer **/
		D3D11_BUFFER_DESC vertex_buff_descr = {};
		vertex_buff_descr.ByteWidth = sizeof(vertex_data_array);
		vertex_buff_descr.Usage = D3D11_USAGE_DEFAULT;
		vertex_buff_descr.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		D3D11_SUBRESOURCE_DATA sr_data = { 0 };
		sr_data.pSysMem = vertex_data_array;
		HRESULT hr = device_ptr->CreateBuffer(
			&vertex_buff_descr,
			&sr_data,
			&vertex_buffer_ptr);
		assert(SUCCEEDED(hr));
	}

	// create time constant
	ID3D11Buffer* constant_buffer_ptr = NULL;
	struct PS_CONSTANT_BUFFER
	{
		int fTick;
		float2 mousepos;
		int2 resolution;
	} ;


	RECT winRect;
	GetClientRect(hWnd, &winRect);
	D3D11_VIEWPORT viewport = {
	  0.0f,
	  0.0f,
	  (FLOAT)(winRect.right - winRect.left),
	  (FLOAT)(winRect.bottom - winRect.top),
	  0.0f,
	  1.0f };
	device_context_ptr->RSSetViewports(1, &viewport);
	PS_CONSTANT_BUFFER PsConstData;
	PsConstData.fTick = 0;
	PsConstData.mousepos = GetMousePosition(hWnd);
	PsConstData.resolution = { intWWidth, intWHeight };

	D3D11_BUFFER_DESC cbDesc;
	cbDesc.ByteWidth = sizeof(PS_CONSTANT_BUFFER) + 0xf & 0xfffffff0; // round constant buffer size to 16 byte boundary
	cbDesc.Usage = D3D11_USAGE_DYNAMIC;
	cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	cbDesc.MiscFlags = 0;
	cbDesc.StructureByteStride = 0;

	D3D11_SUBRESOURCE_DATA InitData;
	InitData.pSysMem = &PsConstData;
	InitData.SysMemPitch = 0;
	InitData.SysMemSlicePitch = 0;

	hr = device_ptr->CreateBuffer(&cbDesc, &InitData,
		&constant_buffer_ptr);
	assert(SUCCEEDED(hr));

	// get time
	ULONGLONG lpSystemTime;
	ULONGLONG lpOriSystemTime = GetTickCount64();

	// create a message loop
	MSG msg;
	msg.message = WM_NULL;
	PeekMessage(&msg, NULL, 0U, 0U, PM_NOREMOVE);
	bool bGotMsg;

	while (WM_QUIT != msg.message) {

		bGotMsg = (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE) != 0);

		if (bGotMsg) {
			// translate and dispatch
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else {
			// set logic
			lpSystemTime = GetTickCount64();
			int msPassed = (int)((lpSystemTime - lpOriSystemTime) % (ULONGLONG)2147483648);
			//PsConstData.fTick = (int) ((lpSystemTime - lpOriSystemTime) % (ULONGLONG) 2147483648);
			D3D11_MAPPED_SUBRESOURCE mappedSubresource;
			device_context_ptr->Map(constant_buffer_ptr, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubresource);
			PS_CONSTANT_BUFFER* PsConstBuff = reinterpret_cast<PS_CONSTANT_BUFFER*>(mappedSubresource.pData);
			PsConstBuff->fTick = msPassed;
			PsConstBuff->mousepos = GetMousePosition(hWnd);
			device_context_ptr->Unmap(constant_buffer_ptr, 0);

			// debug
			//std::wstring dbstr = std::to_wstring(msPassed);
			//OutputDebugString((dbstr+L"\n").c_str());
			
			/* clear the back buffer to cornflower blue for the new frame */
			float background_colour[4] = {
			  0x64 / 255.0f, 0x95 / 255.0f, 0xED / 255.0f, 1.0f };
			device_context_ptr->ClearRenderTargetView(
				render_target_view_ptr, background_colour);

			// set output merger
			device_context_ptr->OMSetRenderTargets(1, &render_target_view_ptr, NULL);

			// set input assembler
			device_context_ptr->IASetPrimitiveTopology(
				D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			device_context_ptr->IASetInputLayout(input_layout_ptr);
			device_context_ptr->IASetVertexBuffers(
				0,
				1,
				&vertex_buffer_ptr,
				&vertex_stride,
				&vertex_offset);
			device_context_ptr->PSSetConstantBuffers(
				0,
				1,
				&constant_buffer_ptr);

			// set the shaders
			device_context_ptr->VSSetShader(vertex_shader_ptr, NULL, 0);
			device_context_ptr->PSSetShader(pixel_shader_ptr, NULL, 0);

			// draw triangle
			device_context_ptr->Draw(vertex_count, 0);

			// present the frame by swapping buffers
			swap_chain_ptr->Present(1, 0);
		}

	}
	
	return (int) msg.wParam;
};

float2 GetMousePosition(HWND hWnd) {
	POINT ptMouse;
	bool check = GetCursorPos(&ptMouse);
	if (!check) {
		return {0.0f, 0.0f};
	}
	check = ScreenToClient(hWnd, &ptMouse);
	if (!check) {
		return { 0.0f, 0.0f };
	}

	return { (float)ptMouse.x / intWWidth, (float)ptMouse.y / intWHeight };
}

LRESULT CALLBACK WndProc(
	_In_ HWND	hWnd,
	_In_ UINT	message,
	_In_ WPARAM	wParam,
	_In_ LPARAM lParam)
{
	switch (message)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
		break;
	}
	return 0;
};