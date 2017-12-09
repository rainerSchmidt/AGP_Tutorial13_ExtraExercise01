#include <windows.h>
#include <d3d11.h>
#include <d3dx11.h>
#include <dxerr.h>
#include <stdio.h>
#include "camera.h"
#include "Model.h"
#include "text2D.h"
#define _XM_NO_INTINSICS_
#define XM_NO_ALIGNMENT
#include <xnamath.h>
#include <dinput.h>

int (WINAPIV* __vsnprintf_s)(char*, size_t, const char*, va_list) = _vsnprintf;

//////////////////////////////////////////////////////////////////////////////////////
//	Global Variables
//////////////////////////////////////////////////////////////////////////////////////
HINSTANCE	g_hInst = NULL;
HWND		g_hWnd = NULL;

D3D_DRIVER_TYPE         g_driverType = D3D_DRIVER_TYPE_NULL;
D3D_FEATURE_LEVEL       g_featureLevel = D3D_FEATURE_LEVEL_11_0;
ID3D11Device*           g_pD3DDevice = NULL;
ID3D11DeviceContext*    g_pImmediateContext = NULL;
IDXGISwapChain*         g_pSwapChain = NULL;

ID3D11RenderTargetView* g_pBackBufferRTView = NULL;

ID3D11DepthStencilView* g_pZBuffer;
ID3D11Buffer*			g_pVertexBuffer;
ID3D11VertexShader*		g_pVertexShader;
ID3D11PixelShader*		g_pPixelShader;
ID3D11InputLayout*		g_pInputLayout;
ID3D11Buffer*			g_pConstantBuffer0;
ID3D11Buffer*			g_pConstantBuffer1;
ID3D11ShaderResourceView* g_pTexture0;
ID3D11SamplerState*		g_pSampler0;

Camera*					g_pCamera = NULL;
Model*					g_pModels[20];
//Model*					g_pModel2;
Text2D*					g_2DText;

XMVECTOR g_directional_light_shines_from;
XMVECTOR g_directional_light_colour;
XMVECTOR g_ambient_light_colour;

//Keyboard input variables
IDirectInput8*			g_direct_input;
IDirectInputDevice8*	g_keyboard_device;
unsigned char			g_keyboard_keys_state[256];

//Mouse input variables
IDirectInputDevice8*	g_pMouseDevice;
DIMOUSESTATE			g_MouseState;
float					MouseX = 0;
float					MouseY = 0;

int g_iZValue = 12;


// Const buffer structs. Pack to 16 bytes. Dont let any single element cross a 16 byte boundary
struct CONSTANT_BUFFER0
{
	XMMATRIX WorldViewProjection;  // 64 bytes (4x4 = 16 floats x 4 bytes)
	//float ThreeColorAmount; // 4 bytes
	//float Scale;			// 4 bytes
	//XMFLOAT2 packing_bytes; // 2x4 bytes = 8 bytes
	XMVECTOR directional_light_vector; // 16 Bytes
	XMVECTOR directional_light_colour; // 16 Bytes
	XMVECTOR ambient_light_colour; // 16 Bytes
	// TOTAL SIZE = 128 bytes
};

//define vertex structure
struct POS_COL_VERTEX
{
	XMFLOAT3 Pos;
	XMFLOAT4 Col;
};

struct POS_COL_TEX_VERTEX
{
	XMFLOAT3 Pos;
	XMFLOAT4 Col;
	XMFLOAT2 Tex;
};

struct POS_COL_TEX_NORM_VERTEX
{
	XMFLOAT3 Pos;
	XMFLOAT4 Col;
	XMFLOAT2 Tex;
	XMFLOAT3 Normal;
};

// Rename for each tutorial
char		g_TutorialName[100] = "Tutorial 13 Exercise 01\0";


//////////////////////////////////////////////////////////////////////////////////////
//	Forward declarations
//////////////////////////////////////////////////////////////////////////////////////
HRESULT InitialiseWindow(HINSTANCE hInstance, int nCmdShow);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

HRESULT InitialiseD3D();

HRESULT InitialiseGraphics(void);

void RenderFrame(void);
void ShutdownD3D();

HRESULT InitialiseInput();
void ReadInputStates();
bool IsKeyPressed(unsigned char DI_keycode);
void KeyLogic();
void MouseLogic();

//////////////////////////////////////////////////////////////////////////////////////
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//////////////////////////////////////////////////////////////////////////////////////
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	if (FAILED(InitialiseWindow(hInstance, nCmdShow)))
	{
		DXTRACE_MSG("Failed to create Window");
		return 0;
	}

	if (FAILED(InitialiseInput()))
	{
		DXTRACE_MSG("Failed to create Input");
		return 0;
	}

	if (FAILED(InitialiseD3D()))
	{
		DXTRACE_MSG("Failed to create Device");
		return 0;
	}

	if (FAILED(InitialiseGraphics()))
	{
		DXTRACE_MSG("Failed to initialise graphics");
		return 0;
	}


	// Main message loop
	MSG msg = { 0 };

	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			// do something
			RenderFrame();
		}
	}

	ShutdownD3D();

	return (int)msg.wParam;
}

//////////////////////////////////////////////////////////////////////////////////////
// Initialise the input mechanics
//////////////////////////////////////////////////////////////////////////////////////
HRESULT InitialiseInput()
{
	HRESULT hr;
	ZeroMemory(g_keyboard_keys_state, sizeof(g_keyboard_keys_state));
	
	hr = DirectInput8Create(g_hInst, DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)&g_direct_input, NULL);
	if (FAILED(hr)) return hr;

	//Create KeyboardInput
	hr = g_direct_input->CreateDevice(GUID_SysKeyboard, &g_keyboard_device, NULL);
	if (FAILED(hr)) return hr;

	hr = g_keyboard_device->SetDataFormat(&c_dfDIKeyboard);
	if (FAILED(hr)) return hr;

	hr = g_keyboard_device->SetCooperativeLevel(g_hWnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
	if (FAILED(hr)) return hr;

	hr = g_keyboard_device->Acquire();
	if (FAILED(hr)) return hr;

	//Create MouseInput
	hr = g_direct_input->CreateDevice(GUID_SysMouse, &g_pMouseDevice, NULL);
	if (FAILED(hr)) return hr;

	hr = g_pMouseDevice->SetDataFormat(&c_dfDIMouse);
	if (FAILED(hr)) return hr;

	hr = g_pMouseDevice->SetCooperativeLevel(g_hWnd, DISCL_FOREGROUND | DISCL_EXCLUSIVE);
	if (FAILED(hr)) return hr;

	hr = g_pMouseDevice->Acquire();
	if (FAILED(hr)) return hr;

	return S_OK;
}

void ReadInputStates()
{
	HRESULT hr;

	//Read Keyboard Input state
	hr = g_keyboard_device->GetDeviceState(sizeof(g_keyboard_keys_state), (LPVOID)&g_keyboard_keys_state);

	if (FAILED(hr))
	{
		if ((hr == DIERR_INPUTLOST) || (hr == DIERR_NOTACQUIRED))
			g_keyboard_device->Acquire();
	}

	//Read Mouse Input state
	hr = g_pMouseDevice->GetDeviceState(sizeof(g_MouseState), (LPVOID)&g_MouseState);

	if (FAILED(hr))
	{
		if ((hr == DIERR_INPUTLOST) || (hr == DIERR_NOTACQUIRED))
			g_pMouseDevice->Acquire();
	}
}

bool IsKeyPressed(unsigned char DI_keycode)
{
	return g_keyboard_keys_state[DI_keycode] & 0x80;
}




//////////////////////////////////////////////////////////////////////////////////////
// Register class and create window
//////////////////////////////////////////////////////////////////////////////////////
HRESULT InitialiseWindow(HINSTANCE hInstance, int nCmdShow)
{
	// Give your app window your own name
	char Name[100] = "RainerSchmidt";

	// Register class
	WNDCLASSEX wcex = { 0 };
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.hInstance = hInstance;
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	//   wcex.hbrBackground = (HBRUSH )( COLOR_WINDOW + 1); // Needed for non-D3D apps
	wcex.lpszClassName = Name;

	if (!RegisterClassEx(&wcex)) return E_FAIL;

	// Create window
	g_hInst = hInstance;
	RECT rc = { 0, 0, 800, 600 };
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
	g_hWnd = CreateWindow(Name, g_TutorialName, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left,
		rc.bottom - rc.top, NULL, NULL, hInstance, NULL);
	if (!g_hWnd)
		return E_FAIL;

	ShowWindow(g_hWnd, nCmdShow);

	return S_OK;
}

//////////////////////////////////////////////////////////////////////////////////////
// Called every time the application receives a message
//////////////////////////////////////////////////////////////////////////////////////
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	HDC hdc;

	switch (message)
	{
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		EndPaint(hWnd, &ps);
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	case WM_KEYDOWN:
		//if (wParam == VK_ESCAPE)
		//	DestroyWindow(g_hWnd);
		//if (wParam == VK_RIGHT)
		//	g_pCamera->Rotate(2);
		//if (wParam == VK_LEFT)
		//	g_pCamera->Rotate(-2);
		//if (wParam == VK_UP)
		//	g_pCamera->Forward(1);
		//if (wParam == VK_DOWN)
		//	g_pCamera->Forward(-1);
		//if (wParam == 0x41) // Key 'a'
		//	g_pCamera->Strafe(-0.5);
		//if (wParam == 0x44) // Key 'd'
		//	g_pCamera->Strafe(0.5);
		return 0;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////
// Create D3D device and swap chain
//////////////////////////////////////////////////////////////////////////////////////
HRESULT InitialiseD3D()
{
	HRESULT hr = S_OK;

	RECT rc;
	GetClientRect(g_hWnd, &rc);
	UINT width = rc.right - rc.left;
	UINT height = rc.bottom - rc.top;

	UINT createDeviceFlags = 0;

#ifdef _DEBUG
	createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	D3D_DRIVER_TYPE driverTypes[] =
	{
		D3D_DRIVER_TYPE_HARDWARE, // comment out this line if you need to test D3D 11.0 functionality on hardware that doesn't support it
		D3D_DRIVER_TYPE_WARP, // comment this out also to use reference device
		D3D_DRIVER_TYPE_REFERENCE,
	};
	UINT numDriverTypes = ARRAYSIZE(driverTypes);

	D3D_FEATURE_LEVEL featureLevels[] =
	{
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
	};
	UINT numFeatureLevels = ARRAYSIZE(featureLevels);

	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.BufferCount = 1;
	sd.BufferDesc.Width = width;
	sd.BufferDesc.Height = height;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = g_hWnd;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = true;

	for (UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; driverTypeIndex++)
	{
		g_driverType = driverTypes[driverTypeIndex];
		hr = D3D11CreateDeviceAndSwapChain(NULL, g_driverType, NULL,
			createDeviceFlags, featureLevels, numFeatureLevels,
			D3D11_SDK_VERSION, &sd, &g_pSwapChain,
			&g_pD3DDevice, &g_featureLevel, &g_pImmediateContext);
		if (SUCCEEDED(hr))
			break;
	}

	if (FAILED(hr))
		return hr;

	// Get pointer to back buffer texture
	ID3D11Texture2D *pBackBufferTexture;
	hr = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D),
		(LPVOID*)&pBackBufferTexture);

	if (FAILED(hr)) return hr;

	// Use the back buffer texture pointer to create the render target view
	hr = g_pD3DDevice->CreateRenderTargetView(pBackBufferTexture, NULL,
		&g_pBackBufferRTView);
	pBackBufferTexture->Release();

	if (FAILED(hr)) return hr;

	// Create a ZBuffer Texture
	D3D11_TEXTURE2D_DESC tex2dDesc;
	ZeroMemory(&tex2dDesc, sizeof(tex2dDesc));

	tex2dDesc.Width = width;
	tex2dDesc.Height = height;
	tex2dDesc.ArraySize = 1;
	tex2dDesc.MipLevels = 1;
	tex2dDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	tex2dDesc.SampleDesc.Count = sd.SampleDesc.Count;
	tex2dDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	tex2dDesc.Usage = D3D11_USAGE_DEFAULT;

	ID3D11Texture2D* pZBufferTexture;
	hr = g_pD3DDevice->CreateTexture2D(&tex2dDesc, NULL, &pZBufferTexture);

	if (FAILED(hr)) return hr;

	// Create the ZBuffer
	D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	ZeroMemory(&dsvDesc, sizeof(dsvDesc));

	dsvDesc.Format = tex2dDesc.Format;
	dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;

	g_pD3DDevice->CreateDepthStencilView(pZBufferTexture, &dsvDesc, &g_pZBuffer);
	pZBufferTexture->Release();

	// Set the render target view
	g_pImmediateContext->OMSetRenderTargets(1, &g_pBackBufferRTView, g_pZBuffer);

	// Set the viewport
	D3D11_VIEWPORT viewport;

	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.Width = width;
	viewport.Height = height;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	g_pImmediateContext->RSSetViewports(1, &viewport);

	g_2DText = new Text2D("assets/font1.bmp", g_pD3DDevice, g_pImmediateContext);


	return S_OK;
}

//////////////////////////////////////////////////////////////////////////////////////
// Clean up D3D objects
//////////////////////////////////////////////////////////////////////////////////////
void ShutdownD3D()
{
	if (g_keyboard_device)
	{
		g_keyboard_device->Unacquire();
		g_keyboard_device->Release();
	}

	if (g_pMouseDevice) g_pMouseDevice->Release();
	if (g_direct_input) g_direct_input->Release();
	if (g_pVertexBuffer) g_pVertexBuffer->Release();
	if (g_pInputLayout) g_pInputLayout->Release();
	if (g_pVertexShader) g_pVertexShader->Release();
	if (g_pPixelShader) g_pPixelShader->Release();

	if (g_pTexture0) g_pTexture0->Release();
	if (g_pSampler0)	g_pSampler0->Release();
	if (g_2DText) delete g_2DText;
	for each(Model* m in g_pModels)
	{
		if (m) delete m;
	}
	//if (g_pModel2) delete g_pModel2;


	if (g_pSwapChain) g_pSwapChain->Release();
	if (g_pImmediateContext) g_pImmediateContext->Release();
	if (g_pZBuffer) g_pZBuffer->Release();
	if (g_pD3DDevice) g_pD3DDevice->Release();
	if (g_pBackBufferRTView) g_pBackBufferRTView->Release();
	if (g_pConstantBuffer0) g_pConstantBuffer0->Release();
	if (g_pCamera) g_pCamera->~Camera();

}

//////////////////////////////////////////////////////////////////////////////////////
// Init Graphics
//////////////////////////////////////////////////////////////////////////////////////
HRESULT InitialiseGraphics()//03-01
{
	HRESULT hr = S_OK;

	/*// Define vertices of a triangle - screen coordinates -1.0 to +1.0
	POS_COL_VERTEX vertices[] =
	{
		// back face 
		{ XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f) },
		{ XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f)},
		{ XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f) },
		{ XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f) },

		// front face
		{ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f)},
		{ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },

		// left face
		{ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f)},
		{ XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },

		// right face
		{ XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f) },
		{ XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f) },
		{ XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f)},
		{ XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f) },
		{ XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f) },
		{ XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f) },

		// bottom face
		{ XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) },
		{ XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) },
		{ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f)},
		{ XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) },
		{ XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) },
		{ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) },

		// top face
		{ XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f) },
		{ XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f) },
		{ XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f)},
		{ XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f) },
		{ XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f) },
		{ XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f) }


	};*/

	/*POS_COL_TEX_VERTEX vertices[] =
	{
		// back face 
		{ XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f), XMFLOAT2(0.0f, 0.0f) },
		{ XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f), XMFLOAT2(0.0f, 1.0f) },
		{ XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 0.0f) },
		{ XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 0.0f) },
		{ XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(0.0f, 1.0f) },
		{ XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 1.0f) },

		// front face
		{ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f), XMFLOAT2(0.0f, 1.0f) },
		{ XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f), XMFLOAT2(0.0f, 0.0f) },
		{ XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f), XMFLOAT2(1.0f, 0.0f) },
		{ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f), XMFLOAT2(0.0f, 1.0f) },
		{ XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f), XMFLOAT2(1.0f, 0.0f) },
		{ XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f), XMFLOAT2(1.0f, 1.0f) },

		// left face
		{ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f), XMFLOAT2(0.0f, 1.0f) },
		{ XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f), XMFLOAT2(0.0f, 0.0f) },
		{ XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f), XMFLOAT2(1.0f, 1.0f) },
		{ XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f), XMFLOAT2(0.0f, 0.0f) },
		{ XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f), XMFLOAT2(1.0f, 0.0f) },
		{ XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f), XMFLOAT2(1.0f, 1.0f) },

		// right face
		{ XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(0.0f, 0.0f) },
		{ XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(0.0f, 1.0f) },
		{ XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 1.0f) },
		{ XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 0.0f) },
		{ XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(0.0f, 0.0f) },
		{ XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 1.0f) },

		// bottom face
		{ XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 1.0f) },
		{ XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 0.0f) },
		{ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f), XMFLOAT2(0.0f, 1.0f) },
		{ XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 0.0f) },
		{ XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f), XMFLOAT2(0.0f, 0.0f) },
		{ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f), XMFLOAT2(0.0f, 1.0f) },

		// top face
		{ XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 0.0f) },
		{ XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 1.0f) },
		{ XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(0.0f, 1.0f) },
		{ XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(0.0f, 0.0f) },
		{ XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 0.0f) },
		{ XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(0.0f, 1.0f) }


	};*/

	POS_COL_TEX_NORM_VERTEX vertices[]
	{
		// back face 
		{ XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f), XMFLOAT2(0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 1.0f)},
		{ XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f), XMFLOAT2(0.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(0.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f) },

		// front face
		{ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f), XMFLOAT2(0.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f) },
		{ XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f), XMFLOAT2(0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, -1.0f) },
		{ XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f), XMFLOAT2(1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, -1.0f) },
		{ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f), XMFLOAT2(0.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f) },
		{ XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f), XMFLOAT2(1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, -1.0f) },
		{ XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f), XMFLOAT2(1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f) },

		// left face
		{ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f), XMFLOAT2(0.0f, 1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f) },
		{ XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f), XMFLOAT2(0.0f, 0.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f) },
		{ XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f), XMFLOAT2(1.0f, 1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f) },
		{ XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f), XMFLOAT2(0.0f, 0.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f) },
		{ XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f), XMFLOAT2(1.0f, 0.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f) },
		{ XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f), XMFLOAT2(1.0f, 1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f) },

		// right face
		{ XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(0.0f, 0.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) },
		{ XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(0.0f, 1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) },
		{ XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) },
		{ XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 0.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) },
		{ XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(0.0f, 0.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) },
		{ XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) },

		// bottom face
		{ XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f) },
		{ XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 0.0f), XMFLOAT3(0.0f, -1.0f, 0.0f) },
		{ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f), XMFLOAT2(0.0f, 1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f) },
		{ XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 0.0f), XMFLOAT3(0.0f, -1.0f, 0.0f) },
		{ XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f), XMFLOAT2(0.0f, 0.0f), XMFLOAT3(0.0f, -1.0f, 0.0f) },
		{ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f), XMFLOAT2(0.0f, 1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f) },

		// top face
		{ XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 0.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) },
		{ XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) },
		{ XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(0.0f, 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) },
		{ XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(0.0f, 0.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) },
		{ XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 0.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) },
		{ XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(0.0f, 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) }
	};

	// create and set Model
	for (int i = 0; i < ARRAYSIZE(g_pModels); i++)
	{
		g_pModels[i] = new Model(g_pD3DDevice, g_pImmediateContext, (i%4) * 20.0f, 0, 20 + (i * 10));
		g_pModels[i]->LoadObjModel("assets/cube.obj");
	}
	/*g_pModel1 = new Model(g_pD3DDevice, g_pImmediateContext, -10, 0, 30);
	g_pModel1->LoadObjModel("assets/cube.obj");

	g_pModel2 = new Model(g_pD3DDevice, g_pImmediateContext, 10, 0, 30);
	g_pModel2->LoadObjModel("assets/cube.obj");*/

	// Set up and create constant buffer
	D3D11_BUFFER_DESC constant_buffer_desc;
	ZeroMemory(&constant_buffer_desc, sizeof(constant_buffer_desc));

	constant_buffer_desc.Usage = D3D11_USAGE_DEFAULT;  // Can use UpdateSubresource() to update
	constant_buffer_desc.ByteWidth = 112;  // Must be a multiple of 16, calculate from CB struct
	constant_buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER; // Use as a constant buffer

	hr = g_pD3DDevice->CreateBuffer(&constant_buffer_desc, NULL, &g_pConstantBuffer0);

	if (FAILED(hr))
	{
		return hr;
	}
	hr = g_pD3DDevice->CreateBuffer(&constant_buffer_desc, NULL, &g_pConstantBuffer1);

	if (FAILED(hr))
	{
		return hr;
	}
	// Set up and create vertex buffer
	D3D11_BUFFER_DESC bufferDesc;
	ZeroMemory(&bufferDesc, sizeof(bufferDesc));
	bufferDesc.Usage = D3D11_USAGE_DYNAMIC;			//Used by CPU and GPU
	bufferDesc.ByteWidth = sizeof(vertices);//Total size of buffer
	bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;//Use as a vertex buffer
	bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;	//Allow CPU Access
	hr = g_pD3DDevice->CreateBuffer(&bufferDesc, NULL, &g_pVertexBuffer);//Create the buffer

	if (FAILED(hr))
	{
		return hr;
	}

	// Set up sampler state
	D3D11_SAMPLER_DESC sampler_desc;
	ZeroMemory(&sampler_desc, sizeof(sampler_desc));
	sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;

	g_pD3DDevice->CreateSamplerState(&sampler_desc, &g_pSampler0);

	// Load texture from file
	D3DX11CreateShaderResourceViewFromFile(g_pD3DDevice, "assets/sword.png", NULL, NULL, &g_pTexture0, NULL);

	// Copy the vertices into the buffer
	D3D11_MAPPED_SUBRESOURCE ms;

	// Lock the buffer to allow writing
	g_pImmediateContext->Map(g_pVertexBuffer, NULL, D3D11_MAP_WRITE_DISCARD, NULL, &ms);

	// Copy the data
	memcpy(ms.pData, vertices, sizeof(vertices));

	// Unlock the buffer
	g_pImmediateContext->Unmap(g_pVertexBuffer, NULL);

	// Load and compile pixel and vertex shaders - use vs_5_0 to target DX11 hardware only
	ID3DBlob *VS, *PS, *error;
	hr = D3DX11CompileFromFile("shaders.hlsl", 0, 0, "VShader", "vs_4_0", 0, 0, 0, &VS, &error, 0);

	if (error != 0) //check for shader compilation error
	{
		OutputDebugStringA((char*)error->GetBufferPointer());
		error->Release();
		if (FAILED(hr)) // dont fail if error is just a warning
		{
			return hr;
		};
	}

	hr = D3DX11CompileFromFile("shaders.hlsl", 0, 0, "PShader", "ps_4_0", 0, 0, 0, &PS, &error, 0);

	if (error != 0)// check for shader compilation error
	{
		OutputDebugStringA((char*)error->GetBufferPointer());
		error->Release();
		if (FAILED(hr))// dont fail if error is just a warning
		{
			return hr;
		};
	}

	// Create shader objects
	hr = g_pD3DDevice->CreateVertexShader(VS->GetBufferPointer(), VS->GetBufferSize(), NULL, &g_pVertexShader);

	if (FAILED(hr))
	{
		return hr;
	}

	hr = g_pD3DDevice->CreatePixelShader(PS->GetBufferPointer(), PS->GetBufferSize(), NULL, &g_pPixelShader);

	if (FAILED(hr))
	{
		return hr;
	}

	// Set the shader objects as active
	g_pImmediateContext->VSSetShader(g_pVertexShader, 0,0);
	g_pImmediateContext->PSSetShader(g_pPixelShader, 0,0);

	//Create and set the Input layout object
	D3D11_INPUT_ELEMENT_DESC iedesc[] = 
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0}
	};

	hr = g_pD3DDevice->CreateInputLayout(iedesc, ARRAYSIZE(iedesc), VS->GetBufferPointer(), VS->GetBufferSize(), &g_pInputLayout);
	//g_pD3DDevice->CreateInputLayout(iedesc, ARRAYSIZE(iedesc), VS->GetBufferPointer(), VS->GetBufferSize(), &g_pInputLayout);

	if (FAILED(hr))
	{
		return hr;
	}

	// Create and set a camera instance
	g_pCamera = new Camera(0.0f, 0.0f, -0.5f, 0.0f);

	g_pImmediateContext->IASetInputLayout(g_pInputLayout);

	return S_OK;
}
// Render frame
void RenderFrame(void)
{
	//read input state
	ReadInputStates();

	//Go through Mouse Logic
	MouseLogic();
	//go through Key Logic
	KeyLogic();

	// Clear the back buffer - choose a colour you like
	float rgba_clear_colour[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	g_pImmediateContext->ClearRenderTargetView(g_pBackBufferRTView, rgba_clear_colour);
	g_pImmediateContext->ClearDepthStencilView(g_pZBuffer, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	// set lighting
	g_directional_light_shines_from = XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f);
	g_directional_light_colour = XMVectorSet(1.0f, 1.0f, 1.0f, 0.0f);
	g_ambient_light_colour = XMVectorSet(0.1f, 0.1f, 0.1f, 1.0f);


	//// Set vertex buffer //03-01
	//UINT stride = sizeof(POS_COL_TEX_NORM_VERTEX);
	//UINT offset = 0;
	//g_pImmediateContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);

	g_pImmediateContext->PSSetSamplers(0, 1, &g_pSampler0);
	g_pImmediateContext->PSSetShaderResources(0, 1, &g_pTexture0);
	

	// create and set WorldViewMatrix
	XMMATRIX projection, world, view;

	//world = XMMatrixRotationZ(XMConvertToRadians(45.0f));
	//world *= XMMatrixRotationX(XMConvertToRadians(45.0f));
	//world *= XMMatrixRotationY(XMConvertToRadians(45.0f));
	//world *= XMMatrixTranslation(0.0f, 0.0f, 10.0f);

	//// Set constant buffer
	//XMMATRIX transpose;
	//CONSTANT_BUFFER0 cb0_values;

	//transpose = XMMatrixTranspose(world);
	////cb0_values.ThreeColorAmount = 0.5f;
	//cb0_values.directional_light_colour = g_directional_light_colour;
	//cb0_values.ambient_light_colour = g_ambient_light_colour;
	//cb0_values.directional_light_vector = XMVector3Transform(g_directional_light_shines_from, transpose);
	//cb0_values.directional_light_vector = XMVector3Normalize(cb0_values.directional_light_vector);

	
	

	/*CONSTANT_BUFFER0 cb1_values;

	cb1_values.directional_light_colour = g_directional_light_colour;
	cb1_values.ambient_light_colour = g_ambient_light_colour;
	cb1_values.directional_light_vector = XMVector3Transform(g_directional_light_shines_from, transpose);
	cb1_values.directional_light_vector = XMVector3Normalize(cb0_values.directional_light_vector);
	
	// create and set WorldViewMatrix
	XMMATRIX newWorld;
	
	newWorld = XMMatrixRotationZ(XMConvertToRadians(120.0f));
	newWorld *= XMMatrixRotationX(XMConvertToRadians(120.0f));
	newWorld *= XMMatrixRotationY(XMConvertToRadians(240.0f));
	newWorld *= XMMatrixTranslation(0.0f,0.0f,30.0f);*/
	

	//Check SphereCollision
	for each (Model* Model in g_pModels)
	{
		Model->LookAt_XZ(0, 10);
		Model->MoveForwards(0.005f);
		for(int i=0; i<ARRAYSIZE(g_pModels); i++)
		{
			if (Model->CheckCollision(g_pModels[i]))
			{
				Model->MoveForwards(-0.005f);
				break;
			}
		}
	}
	/*g_pModel1->LookAt_XZ(g_pModel2->GetM_X(), g_pModel2->GetM_Z());
	g_pModel1->MoveForwards(0.0005f);
	if (g_pModel1->CheckCollision(g_pModel2))
		g_pModel1->MoveForwards(-0.0005f);

	g_pModel2->LookAt_XZ(g_pModel1->GetM_X(), g_pModel1->GetM_Z());
	g_pModel2->MoveForwards(0.0005f);
	if (g_pModel2->CheckCollision(g_pModel1))
		g_pModel2->MoveForwards(-0.0005f);*/

	projection = XMMatrixPerspectiveFovLH(XMConvertToRadians(45.0f), 800.0f / 600.0f, 1.0f, 100.0f);
	// set view with camera values
	//view = XMMatrixIdentity();
	view = g_pCamera->GetViewMatrix();

	// set WorldViewProjection
	//cb0_values.WorldViewProjection = world * view * projection;

	// upload the new values for the constant buffer
	//g_pImmediateContext->UpdateSubresource(g_pConstantBuffer0, 0, 0, &cb0_values, 0, 0);
	// Set constant buffer to active
	//g_pImmediateContext->VSSetConstantBuffers(0, 1, &g_pConstantBuffer0);

	// Select which primitive type to use //03-01
	g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Draw the vertex buffer to the back buffer //03-01
	//g_pImmediateContext->Draw(36,0);


	/*cb1_values.WorldViewProjection = newWorld * view * projection;
	g_pImmediateContext->UpdateSubresource(g_pConstantBuffer0, 0, 0, &cb1_values, 0, 0);
	g_pImmediateContext->VSSetConstantBuffers(0, 1, &g_pConstantBuffer0);*/
    //g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	//g_pImmediateContext->Draw(36, 0);
	
	for each (Model* Model in g_pModels)
	{
		Model->Draw(&view, &projection);
	}

	////output bounding sphere centre points
	//string s =	"SphereCentre X= " + to_string(g_pModel2->GetBoundingSphereCentreX()) +
	//			" Y= "+ to_string(g_pModel2->GetBoundingSphereCentreY()) +
	//			" Z= "+ to_string(g_pModel2->GetBoundingSphereCentreZ());
	//g_2DText->AddText(s, -1, 1, 0.04f);

	/*f = g_pModel1->GetBoundingSphereCentreY();
	s = "Bounding Sphere Centre Point Y= " + to_string(f);
	g_2DText->AddText(s, -1, 1, 0.05f);

	f = g_pModel1->GetBoundingSphereCentreZ();
	s = "Bounding Sphere Centre Point Z= " + to_string(f);
	g_2DText->AddText(s, -1, 1, 0.05f);*/

	g_2DText->RenderText();

	// RENDER HERE

	// Display what has just been rendered
	g_pSwapChain->Present(0, 0);
}

void MouseLogic()
{
	g_pCamera->Rotate(g_MouseState.lX * 0.5);
}
void KeyLogic()
{
	if (IsKeyPressed(DIK_ESCAPE))
		DestroyWindow(g_hWnd);

	if (IsKeyPressed(DIK_W))
		g_pCamera->Forward(0.05);
	if (IsKeyPressed(DIK_A))
		g_pCamera->Strafe(-0.05);
	if (IsKeyPressed(DIK_S))
		g_pCamera->Forward(-0.05);
	if (IsKeyPressed(DIK_D))
		g_pCamera->Strafe(0.05);
	if (IsKeyPressed(DIK_E))
		g_pCamera->Rotate(0.1);
	if (IsKeyPressed(DIK_Q))
		g_pCamera->Rotate(-0.1);
}


