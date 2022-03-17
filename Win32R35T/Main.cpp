#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <tchar.h>
#include <string>
#include <stdlib.h>
#include <d3d11.h>       // D3D interface
#include <d2d1.h>
#include <dxgi.h>        // DirectX driver interface
#include <d3dcompiler.h> // shader compiler
#include <assert.h>		// assert check and crash when necessary
#include <winuser.h>
#include <dwrite.h>
#include <mmsystem.h>
#include "resource.h"
#include "tray.h"
#include "custommessages.h"

#pragma comment( lib, "user32" )          // link against the win32 library
#pragma comment( lib, "d3d11.lib" )       // direct3D library
#pragma comment( lib, "dxgi.lib" )        // directx graphics interface
#pragma comment( lib, "d3dcompiler.lib" ) // shader compiler
#pragma comment( lib, "d2d1.lib")
#pragma comment( lib, "dwrite.lib") 
#pragma comment( lib, "Winmm.lib")

static int intWWidth = 384;
static int intWHeight = 432;
static const WCHAR sc_txtWork[] = L"Work";
static const WCHAR sc_txtRest[] = L"Rest";
static const WCHAR sc_WorkSoundLocation[] = L"Work.wav";
static const WCHAR sc_RestSoundLocation[] = L"Rest.wav";
static const WCHAR sc_txtWorkMsg[] = L"It's time to work.";
static const WCHAR sc_txtRestMsg[] = L"It's time to rest.";
static const WCHAR sc_txtWindowRest[] = L"Resting - R35T";
static const int numWorkDefault = 45;
static const int numRestDefault = 15;
static TCHAR szWindowClass[] = _T("DesktopApp");
static TCHAR szTitle[] = _T("R35T");
static WCHAR msc_fontName[] = L"";
static float msc_fontSize = 16.0f;
static float msc_fontNumSize = 32.0f;
static float msc_fontCountdownSize = 53.0f;
static float msc_fontMsgSize = 32.0f;
static float frameTimeNum = 50;
static float frameTimeDen = 3;
static ID2D1PathGeometry* startBtnTriangle;

HINSTANCE hInst;

// struct
struct float2 { float x, y; };
struct int2 { int x, y; };

// enum
enum class clockState { STOPPED, RUNNING };
enum class textSelected { NONE, WORK, REST };
enum class mouseInteractables { EMPTY, TEXT_WORK, TEXT_REST, BTN_MAIN };

// predefined functions
bool GetMousePixelPos(HWND hWnd, POINT* pptMouse);
bool isPointInRect(D2D1_POINT_2F* ptDIP, const D2D1_RECT_F* rect);
float2 ConvertPointToScreenRelSpace(POINT ptMouse);
ID2D1PathGeometry* GenTriangleGeometry(D2D1_POINT_2F pt1, D2D1_POINT_2F pt2, D2D1_POINT_2F pt3);
BOOL FileExists(LPCTSTR szPath);
// messages
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

// Global Variables
ID3D11Device* device_ptr = NULL;
ID3D11DeviceContext* device_context_ptr = NULL;
ID3D11RenderTargetView* render_target_view_ptr = NULL;
IDXGISwapChain* swap_chain_ptr = NULL;
ID2D1RenderTarget* render2d_target_ptr = NULL;
ID2D1Factory* factory2d_ptr = NULL;
IDWriteFactory* factorywrite_ptr = NULL;

bool bHoveredMessage = true;
bool isWorking = true;
bool isWindOpen = true;

int msPassed;
int nextFrame = 0;
std::wstring tempMsg;
std::wstring tempTxt;
POINT ptMouse;
D2D1_POINT_2F ptDIPMouse;
clockState clkst = clockState::STOPPED;
textSelected txtSlt = textSelected::NONE;
mouseInteractables mouseOn = mouseInteractables::EMPTY;
mouseInteractables mouseDowned = mouseInteractables::EMPTY;
// helper class
class DPIScale
{
	static float scale;

public:
	static void Initialize()
	{
		FLOAT dpi = (FLOAT)GetDpiForWindow(GetDesktopWindow());
		scale = dpi / 96.0f;
	}

	template <typename T>
	static D2D1_POINT_2F PixelsToDips(T x, T y)
	{
		return D2D1::Point2F(static_cast<float>(x) / scale,
			static_cast<float>(y) / scale);
	}
};

float DPIScale::scale = 1.0f;

// helper class: timer
class MyTimer 
{
	const static int msInMin = 60 * 1000;
private:
	int secLeft = 0;
	int destination = 0;
	int cur = 0;
	bool isOver = false;
	std::wstring time;
	//HANDLE threadHandle;
	bool isThreadDone = true;

	std::wstring fillLeadZero(std::wstring str, int amt) {
		if (amt <= str.length()) return str;
		return std::wstring(amt - str.length(), '0').append(str);
	}

	void threadedUpdate(int ms) {
		// check if ended
		if (destination < ms) {
			secLeft = 0;
			time = L"00:00";
			isOver = true;
		}

		// check if need to update text
		else if (destination - ms < secLeft * 1000) {
			secLeft = (destination - ms) / 1000;
			time = fillLeadZero(std::to_wstring(secLeft / 60), 2)
				+ std::wstring(L":")
				+ fillLeadZero(std::to_wstring(secLeft % 60), 2);
		}

		isThreadDone = true;
	}

public:
	MyTimer() {
		time = L"00:00";
	}
	void Start(int ms, int min) {
		destination = ms + min * msInMin;
		secLeft = min * 60;
		isOver = false;
	}
	bool Update(int ms) {
		//if (threadHandle != NULL) CloseHandle(threadHandle);
		if (!isOver) {
			isThreadDone = false;
			threadedUpdate(ms);
			//threadHandle = CreateThread(
			//	0,
			//	0,
			//	threadedUpdate,
			//	&ms,
			//	0,
			//	NULL);
		}
		return isOver;
	}
	void Stop() {
		isOver = true;
	}
	const WCHAR* GetTime() {
		return time.c_str();
	}
	int GetLength() {
		return 5;
	}
};

// work and rest wrapper class
class mainControl {
	const float strokeWidth = 3.0f;
private:
	bool selected = false;
	bool edited = false;
	// minute number
	int minNum;
	int lastMinNum;
	// texts
	std::wstring txtDesc;
	std::wstring txtMin;
	D2D1_RECT_F bound;
	ID2D1PathGeometry* indicatorTriangle;

	float padding1, padding2;
public:
	mainControl(const WCHAR txtDescription[], int minNumber,
		D2D1_RECT_F Bound, const float Padding1, const float Padding2) {

		txtDesc = txtDescription;
		minNum = minNumber;
		lastMinNum = minNum;
		txtMin = std::to_wstring(minNumber);
		bound = Bound;
		padding1 = Padding1;
		padding2 = Padding2;

		float center = (bound.left + bound.right) / 2;
		indicatorTriangle = GenTriangleGeometry(
			D2D1::Point2F(center, bound.bottom + 8.0f),
			D2D1::Point2F(center - 6.0f, bound.bottom + 15.0f),
			D2D1::Point2F(center + 6.0f, bound.bottom + 15.0f));
	}
	//mainControl(const mainControl& oldCtrl) {
	//	selected = oldCtrl.selected;
	//	minNum = oldCtrl.minNum;
	//	txtDesc = oldCtrl.txtDesc;
	//	txtMin = oldCtrl.txtMin;
	//	bound = oldCtrl.bound;
	//	padding1 = oldCtrl.padding1;
	//	padding2 = oldCtrl.padding2;
	//}
	//~mainControl() {
	//	delete(&txtDesc);
	//	delete(&txtMin);
	//}
	void Draw(ID2D1RenderTarget* pt_renderTarget2d, ID2D1SolidColorBrush* ptBrush,
		IDWriteTextFormat* ptDescFormat, IDWriteTextFormat* ptNumFormat, bool drawIndicator) {
		// draw description
		pt_renderTarget2d->DrawText(
			txtDesc.c_str(),
			txtDesc.size(),
			ptDescFormat,
			D2D1::RectF(bound.left, padding1, bound.right, padding1),
			ptBrush,
			D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);

		// draw minute number
		pt_renderTarget2d->DrawText(
			txtMin.c_str(),
			txtMin.size(),
			ptNumFormat,
			D2D1::RectF(bound.left, padding2, bound.right, padding2),
			ptBrush,
			D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);

		// draw surrounding rectangle
		if (selected) {
			pt_renderTarget2d->DrawRectangle(
				&bound,
				ptBrush,
				strokeWidth
			);
		}
		if (drawIndicator) {
			pt_renderTarget2d->FillGeometry(
				indicatorTriangle,
				ptBrush);
		}
	}

	// value editing function
	void EnterNum(int num) {
		edited = true;
		if (txtMin.size() < 2) {
			minNum *= 10;
			minNum += num;
		}
		else {
			minNum = 99;
		}
		txtMin = std::to_wstring(minNum);
	}
	void EnterBackspace() {
		edited = true;
		if (txtMin.size() > 0) {
			txtMin.pop_back();
			minNum /= 10;
		}
	}


	// misc methods
	void SetSelected(bool Selected) {
		selected = Selected;
		if (!selected) {
			//OutputDebugString((edited ? L"true" : L"false"));

			if (!edited) minNum = lastMinNum;
			else lastMinNum = minNum;
			edited = false;
			if (minNum == 0) minNum = 1;
			txtMin = std::to_wstring(minNum);

			//OutputDebugString(txtMin.c_str());
		}
		else {
			lastMinNum = minNum;
			minNum = 0;
			txtMin = L"";
		}
	}
	bool IsMouseOver(D2D1_POINT_2F* mouse) {
		return isPointInRect(mouse, &bound);
	}
	int GetValue() {
		return minNum;
	}
};

MyTimer* ptrMyTimer;
mainControl* ptrCtrlWork;
mainControl* ptrCtrlRest;







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
	wcex.hIcon			= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_MAINICON));
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName	= NULL;
	wcex.lpszClassName	= szWindowClass;
	wcex.hIconSm		= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_MAINICON));

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
	UINT flagsD = D3D11_CREATE_DEVICE_SINGLETHREADED | D3D11_CREATE_DEVICE_BGRA_SUPPORT;
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

	// mouse point setup
	if (!GetMousePixelPos(hWnd, &ptMouse)) return GetLastError();

	// create constant buffer structure
	ID3D11Buffer* constant_buffer_ptr = NULL;
	struct PS_CONSTANT_BUFFER
	{
		float2 mousepos;
		int2 resolution;

		int fTick;
		float yoverx;
		float2 filler;
	} ;

	// create constant buffer for pixel shader
	RECT winRect;
	GetClientRect(hWnd, &winRect);
	intWWidth = winRect.right - winRect.left;
	intWHeight = winRect.bottom - winRect.top;
	D3D11_VIEWPORT viewport = {
	  0.0f,
	  0.0f,
	  (FLOAT)(intWWidth),
	  (FLOAT)(intWHeight),
	  0.0f,
	  1.0f };
	device_context_ptr->RSSetViewports(1, &viewport);
	PS_CONSTANT_BUFFER PsConstData;
	PsConstData.fTick = 0;
	PsConstData.mousepos = ConvertPointToScreenRelSpace(ptMouse);
	PsConstData.resolution = { intWWidth, intWHeight };
	PsConstData.yoverx = ((float)intWHeight) / ((float)intWWidth);

	D3D11_BUFFER_DESC cbDesc;
	cbDesc.ByteWidth = sizeof(PS_CONSTANT_BUFFER); // round constant buffer size to 16 byte boundary
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

	///////////////////////////////////////////////////////////////////////////////////////////////////////

	// create 2d render target
	D2D1CreateFactory(
		D2D1_FACTORY_TYPE_SINGLE_THREADED, &factory2d_ptr);

	IDXGISurface* framebuffer2d;
	hr = swap_chain_ptr->GetBuffer(
		0,
		__uuidof(IDXGISurface),
		(void**)&framebuffer2d);
	assert(SUCCEEDED(hr));

	float dpiX, dpiY;

	//factory2d_ptr->GetDesktopDpi(&dpiX, &dpiY);
	dpiX = (FLOAT)GetDpiForWindow(GetDesktopWindow());
	dpiY = dpiX;

	std::wstring dbstr = std::to_wstring(dpiX);
	OutputDebugString((dbstr + L"\n").c_str());

	const D2D1_RENDER_TARGET_PROPERTIES render2d_target_property = D2D1::RenderTargetProperties(
		D2D1_RENDER_TARGET_TYPE_DEFAULT,
		D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
		dpiX,
		dpiY
	);

	hr = factory2d_ptr->CreateDxgiSurfaceRenderTarget(
		framebuffer2d,
		&render2d_target_property,
		&render2d_target_ptr);
	assert(SUCCEEDED(hr));

	// Create a DirectWrite factory.
	hr = DWriteCreateFactory(
		DWRITE_FACTORY_TYPE_SHARED,
		__uuidof(factorywrite_ptr),
		reinterpret_cast<IUnknown**>(&factorywrite_ptr)
	);
	assert(SUCCEEDED(hr));

	// Create a DirectWrite text format object.
	IDWriteTextFormat* m_pTextFormat;
	hr = factorywrite_ptr->CreateTextFormat(
		msc_fontName,
		NULL,
		DWRITE_FONT_WEIGHT_NORMAL,
		DWRITE_FONT_STYLE_NORMAL,
		DWRITE_FONT_STRETCH_NORMAL,
		msc_fontSize,
		L"", //locale
		&m_pTextFormat
	);
	assert(SUCCEEDED(hr));

	IDWriteTextFormat* m_pNumFormat;
	hr = factorywrite_ptr->CreateTextFormat(
		msc_fontName,
		NULL,
		DWRITE_FONT_WEIGHT_NORMAL,
		DWRITE_FONT_STYLE_NORMAL,
		DWRITE_FONT_STRETCH_NORMAL,
		msc_fontNumSize,
		L"", //locale
		&m_pNumFormat
	);
	assert(SUCCEEDED(hr));

	IDWriteTextFormat* m_pCountdownFormat;
	hr = factorywrite_ptr->CreateTextFormat(
		msc_fontName,
		NULL,
		DWRITE_FONT_WEIGHT_NORMAL,
		DWRITE_FONT_STYLE_NORMAL,
		DWRITE_FONT_STRETCH_NORMAL,
		msc_fontCountdownSize,
		L"", //locale
		&m_pCountdownFormat
	);
	assert(SUCCEEDED(hr));

	IDWriteTextFormat* m_pMainMsgFormat;
	hr = factorywrite_ptr->CreateTextFormat(
		msc_fontName,
		NULL,
		DWRITE_FONT_WEIGHT_NORMAL,
		DWRITE_FONT_STYLE_NORMAL,
		DWRITE_FONT_STRETCH_NORMAL,
		msc_fontMsgSize,
		L"", //locale
		&m_pMainMsgFormat
	);
	assert(SUCCEEDED(hr));

	// Center the text horizontally and vertically.
	m_pTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
	m_pTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
	m_pNumFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
	m_pNumFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
	m_pCountdownFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
	m_pCountdownFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
	m_pMainMsgFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
	m_pMainMsgFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

	ID2D1SolidColorBrush* m_pWhiteBrush;
	hr = render2d_target_ptr->CreateSolidColorBrush(
		D2D1::ColorF(D2D1::ColorF::White),
		&m_pWhiteBrush
	);
	assert(SUCCEEDED(hr));
	ID2D1SolidColorBrush* m_pTransparentWhiteBrush;
	hr = render2d_target_ptr->CreateSolidColorBrush(
		D2D1::ColorF(D2D1::ColorF::White, 0.3f),
		&m_pTransparentWhiteBrush
	);
	assert(SUCCEEDED(hr));
	ID2D1SolidColorBrush* m_pHalfTransWhiteBrush;
	hr = render2d_target_ptr->CreateSolidColorBrush(
		D2D1::ColorF(D2D1::ColorF::White, 0.85f),
		&m_pHalfTransWhiteBrush
	);
	assert(SUCCEEDED(hr));

	const D2D1_SIZE_F renderTargetSize = render2d_target_ptr->GetSize();

	startBtnTriangle = GenTriangleGeometry(
		D2D1::Point2F(renderTargetSize.width / 2 - 35, renderTargetSize.height / 2 - 25),
		D2D1::Point2F(renderTargetSize.width / 2 - 35, renderTargetSize.height / 2 + 55),
		D2D1::Point2F(renderTargetSize.width / 2 + 45, renderTargetSize.height / 2 + 15));

	const D2D1_RECT_F mainRect = D2D1::RectF(renderTargetSize.width / 2 - 50, renderTargetSize.height / 2 - 35,
		renderTargetSize.width / 2 + 50, renderTargetSize.height / 2 + 65);

	const D2D1_RECT_F rectMainTxt = D2D1::RectF(0, mainRect.top,
		renderTargetSize.width, mainRect.bottom);

	const float btnPadding = 15.0f;

	const D2D1_RECT_F rectStopBtn = D2D1::RectF(
		mainRect.left + btnPadding, mainRect.top + btnPadding,
		mainRect.right - btnPadding, mainRect.bottom - btnPadding);

	const int txtleft = 40;
	const int txttop = 10;
	const int txtbottom = 70;
	const int txtright = 100;
	const int padding = 20;
	const int padding2 = 50;

	ptrCtrlWork = new mainControl(sc_txtWork, numWorkDefault,
		D2D1::RectF(txtleft, txttop, txtright, txtbottom),
		padding, padding2);
	ptrCtrlRest = new mainControl(sc_txtRest, numRestDefault,
		D2D1::RectF(renderTargetSize.width - txtright, txttop,
			renderTargetSize.width - txtleft, txtbottom),
		padding, padding2);

	///////////////////////////////////////////////////////////////////////////////////////////////////////

	// get time
	ULONGLONG lpSystemTime;
	ULONGLONG lpOriSystemTime = GetTickCount64();
	ptrMyTimer = new MyTimer();

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
			// update variables
			lpSystemTime = GetTickCount64();
			msPassed = (int)((lpSystemTime - lpOriSystemTime) % (ULONGLONG)2147483648);
			if (!GetMousePixelPos(hWnd, &ptMouse)) return GetLastError();
			
			mouseOn = isPointInRect(&ptDIPMouse, &mainRect) ? mouseInteractables::BTN_MAIN : 
						  ptrCtrlWork->IsMouseOver(&ptDIPMouse) ? mouseInteractables::TEXT_WORK : 
						  ptrCtrlRest->IsMouseOver(&ptDIPMouse) ? mouseInteractables::TEXT_REST :
															  mouseInteractables::EMPTY;
			if (mouseOn == mouseInteractables::BTN_MAIN) {
				bHoveredMessage = true;
			}

			if (clkst == clockState::RUNNING) {
				if (ptrMyTimer->Update(msPassed)) {
					clkst = clockState::STOPPED;
					isWorking = !isWorking;
					SetWindowText(hWnd, szTitle);
					LPCWSTR txtSoundfile = isWorking ?
						//(FileExists(sc_WorkSoundLocation) ? sc_WorkSoundLocation:
						//	MAKEINTRESOURCE(IDR_WAVE_WORK)) : 
						//(FileExists(sc_RestSoundLocation) ? sc_WorkSoundLocation :
						//	MAKEINTRESOURCE(IDR_WAVE_REST));
						MAKEINTRESOURCE(IDR_WAVE_WORK) : MAKEINTRESOURCE(IDR_WAVE_REST);
					PlaySound(
						txtSoundfile,
						hInst,
						SND_RESOURCE | SND_ASYNC | SND_LOOP | SND_SYSTEM);

					ShowWindow(hWnd, SW_SHOW);
					ShowWindow(hWnd, SW_RESTORE);
					TrayDeleteIcon(hWnd);
					isWindOpen = true;

					bHoveredMessage = false;
					SetWindowLongPtr(hWnd, GWL_STYLE,
						GetWindowLongW(hWnd, GWL_STYLE) & ~WS_MINIMIZEBOX);
					SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0,
						SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW | SWP_FRAMECHANGED);
					tempMsg = isWorking ? sc_txtWorkMsg : sc_txtRestMsg;
				}
				else if (isWorking) {
					tempTxt = L"Working: ";
					tempTxt += ptrMyTimer->GetTime();
					tempTxt += L" - R35T";
					SetWindowText(hWnd, tempTxt.c_str());
				}
			}








			///////////////////////////////////////////////////////////////////////////////////

			// draw gpu

			//PsConstData.fTick = (int) ((lpSystemTime - lpOriSystemTime) % (ULONGLONG) 2147483648);
			if (isWindOpen) {
				
				/*if (msPassed < nextFrame) {
					Sleep(nextFrame - msPassed - 1);
					while (msPassed < nextFrame) {};
				}
				int sPassed = msPassed / 1000;
				int msInSec = msPassed % 1000;
				nextFrame = (msInSec * frameTimeDen / frameTimeNum + 1) * frameTimeNum / frameTimeDen + sPassed * 1000;*/

				D3D11_MAPPED_SUBRESOURCE mappedSubresource;
				device_context_ptr->Map(constant_buffer_ptr, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubresource);
				PS_CONSTANT_BUFFER* PsConstBuff = reinterpret_cast<PS_CONSTANT_BUFFER*>(mappedSubresource.pData);
				PsConstBuff->fTick = msPassed;
				PsConstBuff->mousepos = ConvertPointToScreenRelSpace(ptMouse);
				PsConstBuff->resolution = { intWWidth, intWHeight };
				PsConstBuff->yoverx = ((float)intWHeight) / ((float)intWWidth);
				device_context_ptr->Unmap(constant_buffer_ptr, 0);


				// debug
				//float2 test = GetMousePosition(hWnd);
				//std::wstring dbstr2 = std::to_wstring(test.y);
				//std::wstring dbstr = std::to_wstring(PsConstBuff->yoverx);
				//OutputDebugString((dbstr + L"\n").c_str());
			
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

				///////////////////////////////////////////////////////////////////////////////////////////
			
				// draw 2d parts

				// Retrieve the size of the render target.

				render2d_target_ptr->BeginDraw();

				render2d_target_ptr->SetTransform(D2D1::Matrix3x2F::Identity());

				ptrCtrlWork->Draw(render2d_target_ptr, m_pWhiteBrush, m_pTextFormat, m_pNumFormat, isWorking);
				ptrCtrlRest->Draw(render2d_target_ptr, m_pWhiteBrush, m_pTextFormat, m_pNumFormat, !isWorking);


				switch (clkst)
				{
				case clockState::STOPPED:
					if (bHoveredMessage) {
						ID2D1SolidColorBrush* m_pStartBtnBrush =
							mouseOn == mouseInteractables::BTN_MAIN ?
							m_pHalfTransWhiteBrush : m_pTransparentWhiteBrush;

						render2d_target_ptr->FillGeometry(
							startBtnTriangle,
							m_pStartBtnBrush);

						m_pStartBtnBrush = NULL;
					}
					else {
						render2d_target_ptr->DrawText(
							tempMsg.c_str(),
							tempMsg.size(),
							m_pMainMsgFormat,
							&rectMainTxt,
							m_pWhiteBrush,
							D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);
					}
					break;
				case clockState::RUNNING:
					if (mouseOn == mouseInteractables::BTN_MAIN) {
						render2d_target_ptr->FillRectangle(&rectStopBtn, m_pHalfTransWhiteBrush);
					}
					else {
						render2d_target_ptr->DrawText(
							ptrMyTimer->GetTime(),
							ptrMyTimer->GetLength(),
							m_pCountdownFormat,
							&rectMainTxt,
							m_pWhiteBrush,
							D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);
					}
					break;
				default:
					break;
				}

				//std::wstring dbstr = std::to_wstring(renderTargetSize.width) + L", " + std::to_wstring(renderTargetSize.height);
				//OutputDebugString((dbstr + L"\n").c_str());

				hr = render2d_target_ptr->EndDraw();

				if (hr == D2DERR_RECREATE_TARGET)
				{
					hr = S_OK;
				}
				assert(SUCCEEDED(hr));

				// present the frame by swapping buffers
				swap_chain_ptr->Present(1, 0);
			}
			else {
				Sleep(5);
			}

		}

	}
	
	return (int) msg.wParam;
};

bool GetMousePixelPos(HWND hWnd, POINT* pptMouse) {
	bool check = GetCursorPos(pptMouse);
	if (!check) {
		return false;
	}
	check = ScreenToClient(hWnd, pptMouse);
	if (!check) {
		return false;
	}
	ptDIPMouse = DPIScale::PixelsToDips(pptMouse->x, pptMouse->y);
	return true;
}

float2 ConvertPointToScreenRelSpace(POINT ptMouse) {
	return { (float)ptMouse.x / intWWidth, 1.0f - (float)ptMouse.y / intWHeight };
}

ID2D1PathGeometry* GenTriangleGeometry(D2D1_POINT_2F pt1, D2D1_POINT_2F pt2, D2D1_POINT_2F pt3)
{
	ID2D1GeometrySink* pSink = NULL;
	HRESULT hr = S_OK;
	ID2D1PathGeometry* m_pPathGeometry;
	// Create a path geometry.
	assert(SUCCEEDED(hr));
	hr = factory2d_ptr->CreatePathGeometry
	(&m_pPathGeometry);

	assert(SUCCEEDED(hr));
	// Write to the path geometry using the geometry sink.
	hr = m_pPathGeometry->Open(&pSink);

	assert(SUCCEEDED(hr));
	pSink->BeginFigure(
		pt1,
		D2D1_FIGURE_BEGIN_FILLED
	);

	pSink->AddLine(pt2);


	pSink->AddLine(pt3);

	pSink->EndFigure(D2D1_FIGURE_END_CLOSED);

	hr = pSink->Close();

	if (pSink != NULL) {
		pSink->Release();
		pSink = NULL;
	};

	return m_pPathGeometry;
}


bool isPointInRect(D2D1_POINT_2F* ptDIP, const D2D1_RECT_F* rect) {
	return (rect->top < ptDIP->y) && (ptDIP->y < rect->bottom) &&
		(rect->left < ptDIP->x) && (ptDIP->x < rect->right);
}

void SelectText(textSelected txt) {
	switch (txt) {
	case textSelected::WORK:
		ptrCtrlWork->SetSelected(true);
		ptrCtrlRest->SetSelected(false);
		break;
	case textSelected::REST:
		ptrCtrlWork->SetSelected(false);
		ptrCtrlRest->SetSelected(true);
		break;
	default:
		ptrCtrlWork->SetSelected(false);
		ptrCtrlRest->SetSelected(false);
		break;
	}
}

BOOL FileExists(LPCTSTR szPath)
{
	DWORD dwAttrib = GetFileAttributes(szPath);

	return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
		!(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

LRESULT CALLBACK WndProc(
	_In_ HWND	hWnd,
	_In_ UINT	message,
	_In_ WPARAM	wParam,
	_In_ LPARAM lParam)
{
	switch (message)
	{
	case WM_CREATE:
		if (FAILED(D2D1CreateFactory(
			D2D1_FACTORY_TYPE_SINGLE_THREADED, &factory2d_ptr)))
		{
			return -1;  // Fail CreateWindowEx.
		}
		DPIScale::Initialize();

		//{
		//	LPARAM hIcon = (LPARAM)LoadIcon(hInst, MAKEINTRESOURCE(IDI_MAINICON));

		//	//Change both icons to the same icon handle.
		//	SendMessage(hWnd, WM_SETICON, ICON_SMALL, hIcon);
		//	SendMessage(hWnd, WM_SETICON, ICON_BIG, hIcon);

		//	//This will ensure that the application icon gets changed too.
		//	SendMessage(GetWindow(hWnd, GW_OWNER), WM_SETICON, ICON_SMALL, hIcon);
		//	SendMessage(GetWindow(hWnd, GW_OWNER), WM_SETICON, ICON_BIG, hIcon);
		//}
		return 0;
	case WM_DESTROY:
		// debug
		// #region
		WCHAR buf[100];
		SYSTEMTIME st;
		GetLocalTime(&st);
		wsprintfW(buf, L"%.2u:%.2u:%.2u", st.wHour, st.wMinute, st.wSecond);
		MessageBox(NULL, buf, L"Ended Unexpectedly", 0x00000000L);
		// #region
		
		factory2d_ptr->Release();
		delete ptrCtrlWork;
		delete ptrCtrlRest;
		delete ptrMyTimer;
		PostQuitMessage(0);
		break;
	case WM_LBUTTONDOWN:
		// stop all sounds
		float2 tempMsPos = ConvertPointToScreenRelSpace(ptMouse);
		if (tempMsPos.x > 0.0f && tempMsPos.x < 1.0f &&
			tempMsPos.y > 0.0f && tempMsPos.y < 1.0f) {
			SetWindowLongPtr(hWnd, GWL_STYLE,
				GetWindowLongW(hWnd, GWL_STYLE) | WS_MINIMIZEBOX);
			SetWindowPos(hWnd, HWND_NOTOPMOST, 0, 0, 0, 0,
				SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW | SWP_FRAMECHANGED);
			PlaySound(NULL, 0, 0);
		}
		mouseDowned = mouseOn;
		break;
	case WM_LBUTTONUP:
		if (mouseDowned == mouseOn) {
			switch (mouseOn) {
			case mouseInteractables::BTN_MAIN:
				// convenient to add more states
				clkst = clkst == clockState::RUNNING ? 
					clockState::STOPPED : clockState::RUNNING;
				switch (clkst)
				{
				case clockState::STOPPED:
					isWorking = true;
					SetWindowText(hWnd, szTitle);
					ptrMyTimer->Stop();
					break;
				case clockState::RUNNING:
					ptrMyTimer->Start(msPassed,
						isWorking ? ptrCtrlWork->GetValue() : ptrCtrlRest->GetValue());
					SetWindowText(hWnd, sc_txtWindowRest);
					break;
				default:
					break;
				}
				txtSlt = textSelected::NONE;
				break;
			case mouseInteractables::TEXT_WORK:
				txtSlt = textSelected::WORK;
				break;
			case mouseInteractables::TEXT_REST:
				txtSlt = textSelected::REST;
				break;
			default:
				txtSlt = textSelected::NONE;
				break;
			}
			// convenient to add more states
			SelectText(txtSlt);
		}
		break;
	
	case WM_KEYDOWN:
		if (wParam == VK_RETURN || wParam == VK_ESCAPE) {
			txtSlt = textSelected::NONE;
			SelectText(txtSlt);
		} 
		else if (wParam == VK_TAB) {
			txtSlt = (txtSlt == textSelected::WORK) ? textSelected::REST : textSelected::WORK;
			SelectText(txtSlt);
		}
		// not the most efficient coding but serves the purpose
		else if (wParam >= 0x30 && wParam <= 0x39) {
			// entered a number key
			int numInput = wParam - 0x30;
			if (txtSlt == textSelected::WORK) {
				ptrCtrlWork->EnterNum(numInput);
			}
			else if (txtSlt == textSelected::REST) {
				ptrCtrlRest->EnterNum(numInput);
			}
		}
		else if (wParam == VK_BACK) {
			if (txtSlt == textSelected::WORK) {
				ptrCtrlWork->EnterBackspace();
			}
			else if (txtSlt == textSelected::REST) {
				ptrCtrlRest->EnterBackspace();
			}
			// entered a back key
		}
		break;

	case WM_KILLFOCUS:
		txtSlt = textSelected::NONE;
		SelectText(txtSlt);
		break;
	case WM_SIZE:
		if (wParam == SIZE_MINIMIZED) {
			TrayDrawIcon(hWnd);
			isWindOpen = false;
			ShowWindow(hWnd, SW_HIDE);
		}
		break;
	case WM_TRAYMESSAGE:
		switch (lParam) {
		case WM_LBUTTONUP:
			ShowWindow(hWnd, SW_SHOW);
			ShowWindow(hWnd, SW_RESTORE);
			TrayDeleteIcon(hWnd);
			isWindOpen = true;
			break;
		case WM_RBUTTONUP:
			TrayLoadPopupMenu(hWnd);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		break;

	case WM_COMMAND:
	{
		WORD wmId = LOWORD(wParam);
		WORD wmEvent = HIWORD(wParam);
		// Parse the menu selections:
		switch (wmId)
		{
		case ID_TRAY_SHOW:
			ShowWindow(hWnd, SW_SHOW);
			ShowWindow(hWnd, SW_RESTORE);
			isWindOpen = true;
			TrayDeleteIcon(hWnd);
			break;
		case ID_TRAY_QUIT:
			TrayDeleteIcon(hWnd);
			DestroyWindow(hWnd);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
	}
		break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
		break;
	}
	return 0;
};