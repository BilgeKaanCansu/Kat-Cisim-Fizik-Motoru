#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <DirectXMath.h>
#include <windows.h>
#include <map>
#include <set>
#include <WindowsX.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <D3Dcompiler.h>
#include <sstream>
#include <wrl.h>
#include "dinput.h"
#include <fstream>
#include "d3dx12.h"
#include "resource.h"
#include "DDSTextureLoader.h"
using namespace DirectX;
using namespace std;
namespace fs = filesystem;
using Microsoft::WRL::ComPtr;
//VERİ YAPILARI
struct Vertex
{
	XMFLOAT3 position;
	XMFLOAT3 normal;
	XMFLOAT2 texture;
};
struct Triangle {
	XMFLOAT3 v0, v1, v2;
	int index;
};
struct Object
{
	Vertex* vertices;
	XMFLOAT3 momentum;
	XMVECTOR angular;
	XMFLOAT3 kuvvet;
	XMVECTOR tork;
	float density;
	int vertexCount;
	float statikSurt;
	float dinamikSurt;
	XMFLOAT3 position;
	float volume;
	float mass;
	XMMATRIX inertiaTensor;
	XMMATRIX inertiaInverse;

	Object()
		: vertices(nullptr),
		momentum(0.0f, 0.0f, 0.0f),
		angular(XMVectorZero()),
		kuvvet(0.0f, 0.0f, 0.0f),
		tork(XMVectorZero()),
		density(0.0f),
		vertexCount(0),
		statikSurt(0.0f),
		dinamikSurt(0.0f),
		position(0.0f, 0.0f, 0.0f),
		volume(0.0f),
		mass(0.0f),
		inertiaTensor(XMMatrixIdentity()),
		inertiaInverse(XMMatrixIdentity())

	{
	}
	Object(Vertex* verticesModel, XMFLOAT3 momentum, XMVECTOR angularInput, float densityInput, int temp_vertexCount, float ss, float ds)
		: vertices(verticesModel),
		momentum(momentum),
		angular(angularInput),
		kuvvet(0.0f, 0.0f, 0.0f),
		tork(XMVectorZero()),
		density(densityInput),
		vertexCount(temp_vertexCount),
		position(0.0f, 0.0f, 0.0f),
		volume(0.0f),
		mass(0.0f),
		inertiaTensor(XMMatrixIdentity()),
		inertiaInverse(XMMatrixIdentity()),
		statikSurt(ss),
		dinamikSurt(ds)
	{
	}

	Object& operator=(const Object& other)
	{
		if (this != &other)
		{
			// Eğer mevcut vertices doluysa belleği temizle
			if (vertices)
			{
				delete[] vertices;
				vertices = nullptr;
			}
			vertexCount = other.vertexCount;
			if (vertexCount > 0)
			{
				vertices = new Vertex[vertexCount];
				for (int i = 0; i < vertexCount; ++i)
					vertices[i] = other.vertices[i];
			}
			else
				vertices = nullptr;

			density = other.density;
			momentum = other.momentum;
			angular = other.angular;
			kuvvet = other.kuvvet;
			tork = other.tork;
			position = other.position;
			volume = other.volume;
			mass = other.mass;
			inertiaTensor = other.inertiaTensor;
			statikSurt = other.statikSurt;
			dinamikSurt = other.dinamikSurt;
		}
		return *this;
	}
};
struct OOBB
{
	XMFLOAT3 center;
	XMFLOAT3 extents;
	XMFLOAT3 axisX;
	XMFLOAT3 axisY;
	XMFLOAT3 axisZ;
};
struct BVHNode {
	XMFLOAT3 min;
	XMFLOAT3 max;
	vector<Triangle> triangles;
	BVHNode* left = nullptr;
	BVHNode* right = nullptr;

	bool isLeaf() const { return !left && !right; }
};
struct intersect
{
	Object* a;
	Object* b;
	XMVECTOR normal;
	vector<XMFLOAT3> temas;
	float vRel;
	float Impulse = 0.0f;
	string aName;
	string bName;

	intersect(Object* objA, Object* objB, XMVECTOR normalVector, vector<XMFLOAT3> contactPoint, float relativeVelocity, float impulse,string aname, string bname)
		: a(objA), b(objB), normal(normalVector), temas(contactPoint), vRel(relativeVelocity), Impulse(impulse), aName(aname), bName(bname)
	{
	}
	intersect(Object* objA, Object* objB, string aname, string bname)
		:a(objA), b(objB), normal(XMVectorSet(0, 0, 0, 0)), temas(vector<XMFLOAT3>()), vRel(0.0f), Impulse(0.0f), aName(aname), bName(bname)
	{
	}
};
typedef struct
{
	int vIndex1, vIndex2, vIndex3;
	int tIndex1, tIndex2, tIndex3;
	int nIndex1, nIndex2, nIndex3;
} FaceType;
struct SceneConstantBuffer
{
	XMMATRIX mWorld;
	XMMATRIX mView;
	XMMATRIX mProjection;
	XMFLOAT4 mLightPos;
	XMFLOAT4 mLightColor;
	XMFLOAT4 mEyePos;
	XMFLOAT4 mMeshColor;
};
//fizik dizileri
map<string, Object> nesneler;
vector<intersect> intersections;
map<string, OOBB> oobbMap;
//çizim dizileri
map<string, ComPtr<ID3D12Resource>>m_vertexBuffer_of;
map<string, D3D12_VERTEX_BUFFER_VIEW>m_vertexBufferView_of;
map<string, ComPtr<ID3D12Resource>>textureBuffer_of;
map<string, ComPtr<ID3D12Resource>>textureBufferUploadHeap_of;
map<string, XMMATRIX>g_World_of;
map<string, string>texture;
map<string, bool> render;
map<string, int> sira;
map<string, bool> moved;
map<string, BVHNode*> bvh;
map<string, vector<Triangle>> worldTriangles;
vector<pair<string, string>> genis;
///Oyun Değişkenleri
string temp_obj_path = "./Media";
string temp_tex_path = "./Media";

bool FireTankMissile = true;
bool FireEnemyMissile = true;

bool PlayTankFireSoundOnce = true;
bool PlayTankHitSoundOnce = true;
bool PlayEnemyHitSoundOnce = true;
bool PlayEnemyFireSoundOnce = true;

bool once = true;
long long int dongu = 0;
////DIRECTX DEĞİŞKENLERİ//////////////////////////////
XMMATRIX g_World;
XMMATRIX g_View;
XMMATRIX g_Projection;

HINSTANCE m_hinst = NULL;
HWND m_hwnd = NULL;
UINT m_width = 1280;
UINT m_height = 720;
UINT m_rtvDescriptorSize = 0;
bool m_useWarpDevice = false;	// Adapter info.
float rotation = 0.0;
float speed = 0.0;
const UINT FrameCount = 2;
// Pipeline objects.
D3D12_VIEWPORT						m_viewport;
D3D12_RECT							m_scissorRect;
ComPtr<IDXGISwapChain3>				m_swapChain;
ComPtr<ID3D12Device>				m_device;
ComPtr<ID3D12Resource>				m_renderTargets[FrameCount];
ComPtr<ID3D12CommandAllocator>		m_commandAllocator;
ComPtr<ID3D12CommandQueue>			m_commandQueue;
ComPtr<ID3D12RootSignature>			m_rootSignature;
ComPtr<ID3D12DescriptorHeap>		m_rtvHeap;
ComPtr<ID3D12DescriptorHeap>		m_descriptorHeap;
ComPtr<ID3D12PipelineState>			m_pipelineState_Textured;
ComPtr<ID3D12PipelineState>			m_pipelineState_Phong;
ComPtr<ID3D12PipelineState>			m_pipelineState_Solid;
ComPtr<ID3D12GraphicsCommandList>	m_commandList;
ComPtr<ID3D12DescriptorHeap>		m_dsvHeap;
ComPtr<ID3D12Resource>				m_depthStencil;
ComPtr<ID3D12Resource>				m_constantBuffer;
SceneConstantBuffer					m_constantBufferData;
UINT8* m_pCbvDataBegin = NULL;
// Synchronization objects.
UINT								m_frameIndex;
HANDLE								m_fenceEvent;
ComPtr<ID3D12Fence>					m_fence;
UINT64								m_fenceValue;

POINT mLastMousePos;

XMVECTOR Eye;
XMVECTOR At;
XMVECTOR Up;

XMVECTOR DefaultForward = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
XMVECTOR DefaultRight = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
XMVECTOR camForward = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
XMVECTOR camRight = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);

XMMATRIX camRotationMatrix;
float moveLeftRight = 0.0f;
float moveBackForward = 0.0f;
float camYaw = 0.0f;
float camPitch = 0.0f;
float g_mouseDeltaX = 0.0f;

//DIRECTX FONKSİYONLARI
void ThrowIfFailed(HRESULT hr)
{
	if (FAILED(hr))
	{
		throw exception();
	}
}
void GetHardwareAdapter(IDXGIFactory2* pFactory, IDXGIAdapter1** ppAdapter)
{
	ComPtr<IDXGIAdapter1> adapter;
	*ppAdapter = nullptr;

	for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != pFactory->EnumAdapters1(adapterIndex, &adapter); ++adapterIndex)
	{
		DXGI_ADAPTER_DESC1 desc;
		adapter->GetDesc1(&desc);

		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
		{
			// Don't select the Basic Render Driver adapter.
			// If you want a software adapter, pass in "/warp" on the command line.
			continue;
		}

		// Check to see if the adapter supports Direct3D 12, but don't create the actual device yet.
		if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
		{
			break;
		}
	}

	*ppAdapter = adapter.Detach();
}
void WaitForPreviousFrame()
{
	// WAITING FOR THE FRAME TO COMPLETE BEFORE CONTINUING IS NOT BEST PRACTICE.
	// This is code implemented as such for simplicity. The D3D12HelloFrameBuffering
	// sample illustrates how to use fences for efficient resource usage and to maximize GPU utilization.

	// Signal and increment the fence value.
	const UINT64 fence = m_fenceValue;
	ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), fence));
	m_fenceValue++;

	// Wait until the previous frame is finished.
	if (m_fence->GetCompletedValue() < fence)
	{
		ThrowIfFailed(m_fence->SetEventOnCompletion(fence, m_fenceEvent));
		WaitForSingleObject(m_fenceEvent, INFINITE);
	}

	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}
void OnDestroy()
{
	// Ensure that the GPU is no longer referencing resources that are about to be cleaned up by the destructor.
	WaitForPreviousFrame();

	CloseHandle(m_fenceEvent);
}
void OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;

	SetCapture(m_hwnd);
}
void OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}
void OnMouseMove(WPARAM btnState, int x, int y)
{
	if ((btnState & MK_LBUTTON) != 0)
	{
		// Make each pixel correspond to a quarter of a degree.
		float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
		g_mouseDeltaX = dx;
		float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));

		camYaw += dx;
		camPitch += dy;
	}

	mLastMousePos.x = x;
	mLastMousePos.y = y;
}
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

	case WM_LBUTTONDOWN:

	case WM_MBUTTONDOWN:

	case WM_RBUTTONDOWN:
		OnMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;

	case WM_LBUTTONUP:

	case WM_MBUTTONUP:

	case WM_RBUTTONUP:
		OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;

	case WM_MOUSEMOVE:
		OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}
HRESULT InitWindow(HINSTANCE hInstance, int nCmdShow)
{
	// Register class
	WNDCLASSEX wcex;
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, (LPCTSTR)IDI_TUTORIAL1);
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = L"DirectX12TankOyunu";
	wcex.hIconSm = LoadIcon(wcex.hInstance, (LPCTSTR)IDI_TUTORIAL1);

	if (!RegisterClassEx(&wcex)) return E_FAIL;

	// Create window
	m_hinst = hInstance;
	RECT rc = { 0, 0, 1280, 720 };
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

	m_hwnd = CreateWindow(
		L"DirectX12TankOyunu", L"DirectX12 > Tank Oyunu",
		WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
		rc.right - rc.left, rc.bottom - rc.top,
		NULL, NULL, hInstance, NULL);

	if (!m_hwnd) return E_FAIL;

	ShowWindow(m_hwnd, nCmdShow);
	return S_OK;
}
//YARDIMCI FONKSİYONLAR
XMFLOAT3 DivideFloat3(const XMFLOAT3& vec, float scalar)
{
	return XMFLOAT3(vec.x / scalar, vec.y / scalar, vec.z / scalar);
}
XMFLOAT3 MultiplyFloat3(const XMFLOAT3& vec, float scalar)
{
	return XMFLOAT3(vec.x * scalar, vec.y * scalar, vec.z * scalar);
}
XMFLOAT3 AddFloat3(const XMFLOAT3& vec1, const XMFLOAT3& vec2) {
	return XMFLOAT3(vec1.x + vec2.x, vec1.y + vec2.y, vec1.z + vec2.z);
}
XMFLOAT3 SubtractFloat3(const XMFLOAT3& vec1, const XMFLOAT3& vec2) {
	return XMFLOAT3(vec1.x - vec2.x, vec1.y - vec2.y, vec1.z - vec2.z);
}
bool IsZeroMatrix(const XMMATRIX& m)
{
	XMFLOAT4X4 f;
	XMStoreFloat4x4(&f, m);

	return (f._11 == 0.0f && f._12 == 0.0f && f._13 == 0.0f && f._14 == 0.0f &&
		f._21 == 0.0f && f._22 == 0.0f && f._23 == 0.0f && f._24 == 0.0f &&
		f._31 == 0.0f && f._32 == 0.0f && f._33 == 0.0f && f._34 == 0.0f &&
		f._41 == 0.0f && f._42 == 0.0f && f._43 == 0.0f && f._44 == 0.0f);
}
bool PointInTriangle(XMVECTOR p, XMVECTOR a, XMVECTOR b, XMVECTOR c) {
	XMVECTOR v0 = b - a;
	XMVECTOR v1 = c - a;
	XMVECTOR v2 = p - a;

	float d00 = XMVectorGetX(XMVector3Dot(v0, v0));
	float d01 = XMVectorGetX(XMVector3Dot(v0, v1));
	float d11 = XMVectorGetX(XMVector3Dot(v1, v1));
	float d20 = XMVectorGetX(XMVector3Dot(v2, v0));
	float d21 = XMVectorGetX(XMVector3Dot(v2, v1));

	float denom = d00 * d11 - d01 * d01;
	if (fabs(denom) < 1e-6f) return false;

	float v = (d11 * d20 - d01 * d21) / denom;
	float w = (d00 * d21 - d01 * d20) / denom;
	float u = 1.0f - v - w;

	return (u >= -1e-4f && v >= -1e-4f && w >= -1e-4f);
}
float SignedTetrahedronVolume(const XMFLOAT3& p0, const XMFLOAT3& p1, const XMFLOAT3& p2)
{
	XMVECTOR v0 = XMLoadFloat3(&p0);
	XMVECTOR v1 = XMLoadFloat3(&p1);
	XMVECTOR v2 = XMLoadFloat3(&p2);

	XMVECTOR cross = XMVector3Cross(v1, v2);
	float volume = XMVectorGetX(XMVector3Dot(v0, cross)) / 6.0f;
	return volume;
}
void CalculateProperties(Object& obj)
{
	obj.volume = 0.0f;
	XMVECTOR weightedCenter = XMVectorZero();
	float totalVolume = 0.0f;

	float Ixx = 0, Iyy = 0, Izz = 0;
	float Ixy = 0, Ixz = 0, Iyz = 0;

	for (int i = 0; i < obj.vertexCount; i += 3)
	{
		XMFLOAT3 p0 = obj.vertices[i].position;
		XMFLOAT3 p1 = obj.vertices[i + 1].position;
		XMFLOAT3 p2 = obj.vertices[i + 2].position;

		float vol = SignedTetrahedronVolume(p0, p1, p2);

		totalVolume += vol;

		// Tetrahedron centroid (ortalama)
		XMVECTOR centroid = (XMLoadFloat3(&p0) + XMLoadFloat3(&p1) + XMLoadFloat3(&p2)) / 4.0f;

		weightedCenter += centroid * vol;

		// Eylemsizlik katkısı (yaklaşık formül)
		float temp0 = p0.x * p0.x + p1.x * p1.x + p2.x * p2.x + p0.x * p1.x + p1.x * p2.x + p2.x * p0.x;
		float temp1 = p0.y * p0.y + p1.y * p1.y + p2.y * p2.y + p0.y * p1.y + p1.y * p2.y + p2.y * p0.y;
		float temp2 = p0.z * p0.z + p1.z * p1.z + p2.z * p2.z + p0.z * p1.z + p1.z * p2.z + p2.z * p0.z;

		Ixx += vol * (temp1 + temp2) / 10.0f;
		Iyy += vol * (temp0 + temp2) / 10.0f;
		Izz += vol * (temp0 + temp1) / 10.0f;

		float tempXY = (2.0f * (p0.x * p0.y + p1.x * p1.y + p2.x * p2.y) + (p0.x * p1.y + p1.x * p2.y + p2.x * p0.y)) / 10.0f;
		float tempXZ = (2.0f * (p0.x * p0.z + p1.x * p1.z + p2.x * p2.z) + (p0.x * p1.z + p1.x * p2.z + p2.x * p0.z)) / 10.0f;
		float tempYZ = (2.0f * (p0.y * p0.z + p1.y * p1.z + p2.y * p2.z) + (p0.y * p1.z + p1.y * p2.z + p2.y * p0.z)) / 10.0f;

		Ixy -= vol * tempXY;
		Ixz -= vol * tempXZ;
		Iyz -= vol * tempYZ;
	}

	obj.volume = fabsf(totalVolume);
	if (obj.density != FLT_MAX)
	{
		obj.mass = obj.density * obj.volume;
	}
	else
	{
		obj.mass = FLT_MAX;
	}

	if (totalVolume != 0.0f)
	{
		weightedCenter /= totalVolume;
	}

	XMStoreFloat3(&obj.position, weightedCenter);

	// Kütle merkezine göre düzeltme (Parallel Axis Theorem)
	XMFLOAT3 c = obj.position;
	float dx = c.x;
	float dy = c.y;
	float dz = c.z;

	float mass = obj.mass;

	Ixx = Ixx * obj.density - mass * (dy * dy + dz * dz);
	Iyy = Iyy * obj.density - mass * (dx * dx + dz * dz);
	Izz = Izz * obj.density - mass * (dx * dx + dy * dy);

	Ixy = Ixy * obj.density + mass * (dx * dy);
	Ixz = Ixz * obj.density + mass * (dx * dz);
	Iyz = Iyz * obj.density + mass * (dy * dz);

	// XMMATRIX'e doldur
	obj.inertiaTensor = XMMatrixSet(
		Ixx, Ixy, Ixz, 0.0f,
		Ixy, Iyy, Iyz, 0.0f,
		Ixz, Iyz, Izz, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	);
	obj.inertiaInverse = XMMatrixInverse(nullptr, obj.inertiaTensor);
}
string dosyaAdiAl(const string& yol) {
	size_t slashPos = yol.find_last_of("/\\");  // Hem Linux hem Windows için
	size_t dotPos = yol.find_last_of('.');

	if (dotPos == string::npos || slashPos == string::npos || dotPos <= slashPos) {
		return "";  // Geçersiz durum
	}

	return yol.substr(slashPos + 1, dotPos - slashPos - 1);
}
vector<string> Obj_Finder(const string& folderPath) {
	vector<string> filePaths;

	try {
		for (const auto& entry : fs::directory_iterator(folderPath)) {
			if (entry.is_regular_file() && entry.path().extension().string() == ".obj") {
				filePaths.push_back(entry.path().string()); // Tam dosya yolu
			}
		}
	}
	catch (const fs::filesystem_error& e) {
		cerr << "Hata: " << e.what() << endl;
	}

	return filePaths;
}
map<string, string> Texture_Finder(const string& folderPath) {
	map<string, string>texPaths;
	try {
		for (const auto& entry : fs::directory_iterator(folderPath)) {
			if (entry.is_regular_file() && entry.path().extension().string() == ".dds") {
				//texPaths[dosyaAdiAl(folderPath)]=(entry.path().string()); // Tam dosya yolu
				texPaths[dosyaAdiAl(entry.path().string())] = entry.path().string();
			}
		}
	}
	catch (const fs::filesystem_error& e) {
		cerr << "Hata: " << e.what() << endl;
	}

	return texPaths;
}
Object Obj_Loader(string filename)
{
	ifstream fin;
	char input;

	// Initialize the counts.
	int vertexCount = 0;
	int textureCount = 0;
	int normalCount = 0;
	int faceCount = 0;

	// Open the file.
	fin.open(filename);
	if (!fin.is_open()) {
		cerr << "Dosya açılamadı: " << filename << endl;
		return Object();
	}

	// Read from the file and continue to read until the end of the file is reached.
	
	while (fin.get(input))
	{
		// If the line starts with 'v' then count either the vertex, the texture coordinates, or the normal vector.
		if (input == 'v')
		{
			if(fin.get(input))
			{
				if (input == ' ') { vertexCount++; }
				if (input == 't') { textureCount++; }
				if (input == 'n') { normalCount++; }
			}
		}

		// If the line starts with 'f' then increment the face count.
		else if (input == 'f')
		{
			if (fin.get(input) && input == ' ') { faceCount++; }
		}

		// Otherwise read in the remainder of the line.
		while (input != '\n' && fin.get(input));

	}

	// Close the file.
	fin.close();

	XMFLOAT3* vertices, * texcoords, * normals;
	XMFLOAT3 momentum = XMFLOAT3(0.0f, 0.0f, 0.0f);
	XMFLOAT4 angular_temp = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
	FaceType* faces;
	int vertexIndex, texcoordIndex, normalIndex, faceIndex, vIndex, tIndex, nIndex;
	char input2;
	float density = 0.0f, ss = 0.0f, ds = 0.0f;

	// Initialize the four data structures.
	vertices = new XMFLOAT3[vertexCount];
	texcoords = new XMFLOAT3[textureCount];
	normals = new XMFLOAT3[normalCount];
	faces = new FaceType[faceCount];

	// Initialize the indexes.
	vertexIndex = 0;
	texcoordIndex = 0;
	normalIndex = 0;
	faceIndex = 0;

	// Open the file.
	fin.open(filename);

	// Read in the vertices, texture coordinates, and normals into the data structures.
	// Important: Also convert to left hand coordinate system since Maya uses right hand coordinate system.
	fin.get(input);
	while (fin.get(input)) // güvenli döngü
	{
		if (input == 'v')
		{
			if (fin.get(input))
			{
				if (input == ' ')
				{
					fin >> vertices[vertexIndex].x >> vertices[vertexIndex].y >> vertices[vertexIndex].z;
					vertices[vertexIndex].z *= -1.0f; // left-hand system
					vertexIndex++;
				}
				else if (input == 't')
				{
					fin >> texcoords[texcoordIndex].x >> texcoords[texcoordIndex].y;
					texcoords[texcoordIndex].y = 1.0f - texcoords[texcoordIndex].y; // left-hand
					texcoordIndex++;
				}
				else if (input == 'n')
				{
					fin >> normals[normalIndex].x >> normals[normalIndex].y >> normals[normalIndex].z;
					normals[normalIndex].z *= -1.0f;
					normalIndex++;
				}
			}
		}
		else if (input == 'f')
		{
			if (fin.get(input) && input == ' ')
			{
				// Face verisini ters sırayla oku (left-hand için)
				fin >> faces[faceIndex].vIndex3 >> input2 >> faces[faceIndex].tIndex3 >> input2 >> faces[faceIndex].nIndex3
					>> faces[faceIndex].vIndex2 >> input2 >> faces[faceIndex].tIndex2 >> input2 >> faces[faceIndex].nIndex2
					>> faces[faceIndex].vIndex1 >> input2 >> faces[faceIndex].tIndex1 >> input2 >> faces[faceIndex].nIndex1;
				faceIndex++;
			}
		}
		else if (input == 'd')
		{
			if (fin.get(input))
			{
				if (input == ' ')
				{
					fin >> density;
				}
				else if (input == 'i')
				{
					density = FLT_MAX;
				}
			}
		}
		else if (input == 'h')
		{
			if (fin.get(input) && input == ' ')
			{
				fin >> momentum.x >> momentum.y >> momentum.z;
			}
		}
		else if (input == 'a')
		{
			if (fin.get(input) && input == ' ')
			{
				fin >> angular_temp.x >> angular_temp.y >> angular_temp.z >> angular_temp.w;
			}
		}
		else if (input == 's')
		{
			if (fin.get(input))
			{
				if (input == 's')
				{
					fin >> ss;
				}
				else if (input == 'd')
				{
					fin >> ds;
				}
			}
		}

		// Satır sonuna kadar atla
		while (input != '\n' && fin.get(input));
	}

	// Close the file.
	fin.close();

	int temp_verticesCount = faceCount * 3;
	Vertex* verticesModel = new Vertex[temp_verticesCount];

	// Now loop through all the faces and output the three vertices for each face.
	int k = 0;
	for (int i = 0; i < faceIndex; i++)
	{
		vIndex = faces[i].vIndex1 - 1;
		tIndex = faces[i].tIndex1 - 1;
		nIndex = faces[i].nIndex1 - 1;

		verticesModel[k].position.x = vertices[vIndex].x;
		verticesModel[k].position.y = vertices[vIndex].y;
		verticesModel[k].position.z = vertices[vIndex].z;

		verticesModel[k].texture.x = texcoords[tIndex].x;
		verticesModel[k].texture.y = texcoords[tIndex].y;

		verticesModel[k].normal.x = normals[nIndex].x;
		verticesModel[k].normal.y = normals[nIndex].y;
		verticesModel[k].normal.z = normals[nIndex].z;

		vIndex = faces[i].vIndex2 - 1;
		tIndex = faces[i].tIndex2 - 1;
		nIndex = faces[i].nIndex2 - 1;

		k++;

		verticesModel[k].position.x = vertices[vIndex].x;
		verticesModel[k].position.y = vertices[vIndex].y;
		verticesModel[k].position.z = vertices[vIndex].z;

		verticesModel[k].texture.x = texcoords[tIndex].x;
		verticesModel[k].texture.y = texcoords[tIndex].y;

		verticesModel[k].normal.x = normals[nIndex].x;
		verticesModel[k].normal.y = normals[nIndex].y;
		verticesModel[k].normal.z = normals[nIndex].z;

		vIndex = faces[i].vIndex3 - 1;
		tIndex = faces[i].tIndex3 - 1;
		nIndex = faces[i].nIndex3 - 1;

		k++;
		verticesModel[k].position.x = vertices[vIndex].x;
		verticesModel[k].position.y = vertices[vIndex].y;
		verticesModel[k].position.z = vertices[vIndex].z;

		verticesModel[k].texture.x = texcoords[tIndex].x;
		verticesModel[k].texture.y = texcoords[tIndex].y;

		verticesModel[k].normal.x = normals[nIndex].x;
		verticesModel[k].normal.y = normals[nIndex].y;
		verticesModel[k].normal.z = normals[nIndex].z;

		k++;
	}

	XMVECTOR angular = XMVectorSet(angular_temp.x, angular_temp.y, angular_temp.z, angular_temp.w);

	Object ObjectModel = Object(verticesModel, momentum, angular, density, temp_verticesCount, ss, ds);
	CalculateProperties(ObjectModel);
	// Release the four data structures.
	//delete[] vertices;
	/*delete[] texcoords;
	delete[] normals;
	delete[] faces;
	delete[] verticesModel;*/

	return ObjectModel;
}
map<string, Object> Obj_Lister(string path)
{
	vector<string> list = Obj_Finder(path);
	map<string, Object> Dict;
	render.clear();
	sira.clear();
	for (size_t i = 0; i < list.size(); i++)
	{
		Dict[dosyaAdiAl(list[i])] = Obj_Loader(list[i]);
	}
	int k = 0;

	for (const auto& [key, deger] : Dict)
	{
		render[key] = true;
		sira[key] = k++;
		moved[key] = true;
	}
	return Dict;
}
void DeleteBVH(BVHNode* node) {
	if (!node) return;
	DeleteBVH(node->left);
	DeleteBVH(node->right);
	delete node;
}
float CalculateDeltaTime()
{
	static LARGE_INTEGER frequency;
	static LARGE_INTEGER lastTime;
	static bool initialized = false;

	if (!initialized)
	{
		QueryPerformanceFrequency(&frequency);
		QueryPerformanceCounter(&lastTime);
		initialized = true;
		return 0.016f; // Başlangıçta yaklaşık 60 FPS kabul edelim
	}

	LARGE_INTEGER currentTime;
	QueryPerformanceCounter(&currentTime);

	float deltaTime = static_cast<float>(currentTime.QuadPart - lastTime.QuadPart) / frequency.QuadPart;
	lastTime = currentTime;
	return deltaTime;
}
//GENİŞ FAZ
bool OBBIntersect(const OOBB& a, const OOBB& b)
{
	// OBB1 eksenleri
	XMVECTOR A0 = XMLoadFloat3(&a.axisX);
	XMVECTOR A1 = XMLoadFloat3(&a.axisY);
	XMVECTOR A2 = XMLoadFloat3(&a.axisZ);

	// OBB2 eksenleri
	XMVECTOR B0 = XMLoadFloat3(&b.axisX);
	XMVECTOR B1 = XMLoadFloat3(&b.axisY);
	XMVECTOR B2 = XMLoadFloat3(&b.axisZ);

	// Merkez farkı (B - A)
	XMVECTOR centerA = XMLoadFloat3(&a.center);
	XMVECTOR centerB = XMLoadFloat3(&b.center);
	XMVECTOR D = centerB - centerA;

	// A ve B kutularının yarı uzunlukları
	float aExt[3] = { a.extents.x, a.extents.y, a.extents.z };
	float bExt[3] = { b.extents.x, b.extents.y, b.extents.z };

	// Eksenler listesi (toplam 15)
	XMVECTOR axes[15] = {
		A0, A1, A2,
		B0, B1, B2,
		XMVector3Cross(A0, B0),
		XMVector3Cross(A0, B1),
		XMVector3Cross(A0, B2),
		XMVector3Cross(A1, B0),
		XMVector3Cross(A1, B1),
		XMVector3Cross(A1, B2),
		XMVector3Cross(A2, B0),
		XMVector3Cross(A2, B1),
		XMVector3Cross(A2, B2)
	};

	for (int i = 0; i < 15; ++i)
	{
		XMVECTOR axis = XMVector3Normalize(axes[i]);

		// Skip null vectors (when axes are parallel and cross product is zero)
		if (XMVector3LengthSq(axis).m128_f32[0] < 1e-6f)
			continue;

		// Projeksiyon merkez farkı (D) bu eksende ne kadar?
		float projCenter = fabsf(XMVectorGetX(XMVector3Dot(D, axis)));

		// A ve B kutularının projeksiyon yarıçapları
		float projA =
			aExt[0] * fabsf(XMVectorGetX(XMVector3Dot(A0, axis))) +
			aExt[1] * fabsf(XMVectorGetX(XMVector3Dot(A1, axis))) +
			aExt[2] * fabsf(XMVectorGetX(XMVector3Dot(A2, axis)));

		float projB =
			bExt[0] * fabsf(XMVectorGetX(XMVector3Dot(B0, axis))) +
			bExt[1] * fabsf(XMVectorGetX(XMVector3Dot(B1, axis))) +
			bExt[2] * fabsf(XMVectorGetX(XMVector3Dot(B2, axis)));

		// Ayrım varsa kesişme yok
		if (projCenter > projA + projB)
			return false;
	}

	// Hiçbir eksende ayrım bulunamadıysa kutular kesişiyor
	return true;
}
OOBB ComputeOOBB(const Object& obj, const XMMATRIX& worldMatrix)
{
	XMVECTOR objectPosition = XMLoadFloat3(&obj.position);

	// Local eksenleri al (worldMatrix’ten)
	XMVECTOR axisX = XMVector3Normalize(worldMatrix.r[0]);
	XMVECTOR axisY = XMVector3Normalize(worldMatrix.r[1]);
	XMVECTOR axisZ = XMVector3Normalize(worldMatrix.r[2]);

	float minX = FLT_MAX, maxX = -FLT_MAX;
	float minY = FLT_MAX, maxY = -FLT_MAX;
	float minZ = FLT_MAX, maxZ = -FLT_MAX;

	for (int i = 0; i < obj.vertexCount; ++i)
	{
		XMVECTOR localPos = XMLoadFloat3(&obj.vertices[i].position);
		XMVECTOR worldPos = XMVector3Transform(localPos, worldMatrix);

		float x = XMVectorGetX(XMVector3Dot(worldPos, axisX));
		float y = XMVectorGetX(XMVector3Dot(worldPos, axisY));
		float z = XMVectorGetX(XMVector3Dot(worldPos, axisZ));

		minX = min(minX, x); maxX = max(maxX, x);
		minY = min(minY, y); maxY = max(maxY, y);
		minZ = min(minZ, z); maxZ = max(maxZ, z);
	}

	float centerX = (minX + maxX) * 0.5f;
	float centerY = (minY + maxY) * 0.5f;
	float centerZ = (minZ + maxZ) * 0.5f;

	XMVECTOR translation = worldMatrix.r[3];
	XMVECTOR centerWorld = translation + axisX * centerX + axisY * centerY + axisZ * centerZ;

	OOBB box;
	XMStoreFloat3(&box.center, centerWorld);
	box.extents = XMFLOAT3((maxX - minX) * 0.5f, (maxY - minY) * 0.5f, (maxZ - minZ) * 0.5f);
	if (box.extents.x == 0.0f) box.extents.x = 0.01f; 
	if (box.extents.y == 0.0f) box.extents.y = 0.01f; 
	if (box.extents.z == 0.0f) box.extents.z = 0.01f; 
	XMStoreFloat3(&box.axisX, axisX);
	XMStoreFloat3(&box.axisY, axisY);
	XMStoreFloat3(&box.axisZ, axisZ);

	return box;
}
vector<intersect> TestIntersection(map<string, OOBB> boxes)					//Geniş FAZ İÇİN
{
	vector<intersect> result;                                      //BURADA BİR BAŞLANGIÇ YAP
	genis.clear();
	for (auto& [key, deger] : oobbMap)
	{
		for (auto& [key1, deger1] : oobbMap)
		{
			if (key == key1)
				continue;
			string ilk = min(key, key1);
			string ikinci = max(key, key1);
			pair<string, string> ikili = { ilk, ikinci };
			if(find(genis.begin(),genis.end(),ikili)!=genis.end())
			{
				continue;
			}
			genis.push_back(ikili);
			if (OBBIntersect(deger, deger1))											//KESİŞİM VARSA
			{
				intersect inter(&nesneler[key], &nesneler[key1], key, key1);
				result.push_back(inter);
			}

		}
	}

	return result;
}
vector<intersect> BroadPhase()
{
	for (auto& [key, deger] : nesneler) {
		if (moved[key])
		{
			oobbMap[key] = ComputeOOBB(deger, g_World_of[key]);
		}
	}														// Cisimleri kutuya al
	vector<intersect> result = TestIntersection(oobbMap);
	return result;
}
//DAR FAZ
void ComputeBounds(const Triangle& tri, XMFLOAT3& min, XMFLOAT3& max) {
	min.x = min(min(tri.v0.x, tri.v1.x), tri.v2.x);
	min.y = min(min(tri.v0.y, tri.v1.y), tri.v2.y);
	min.z = min(min(tri.v0.z, tri.v1.z), tri.v2.z);
	max.x = max(max(tri.v0.x, tri.v1.x), tri.v2.x);
	max.y = max(max(tri.v0.y, tri.v1.y), tri.v2.y);
	max.z = max(max(tri.v0.z, tri.v1.z), tri.v2.z);
}
BVHNode* BuildBVH(vector<Triangle>& tris, int depth = 0) {
	if (tris.empty()) return nullptr;

	BVHNode* node = new BVHNode();

	XMFLOAT3 minBound = tris[0].v0, maxBound = tris[0].v0;
	for (const auto& tri : tris) {
		XMFLOAT3 triMin, triMax;
		ComputeBounds(tri, triMin, triMax);
		minBound.x = min(minBound.x, triMin.x); maxBound.x = max(maxBound.x, triMax.x);
		minBound.y = min(minBound.y, triMin.y); maxBound.y = max(maxBound.y, triMax.y);
		minBound.z = min(minBound.z, triMin.z); maxBound.z = max(maxBound.z, triMax.z);
	}
	node->min = minBound;
	node->max = maxBound;

	if (tris.size() <= 2 || depth > 10) {
		node->triangles = tris;
		return node;
	}

	XMFLOAT3 extent = {
		maxBound.x - minBound.x,
		maxBound.y - minBound.y,
		maxBound.z - minBound.z
	};

	int axis = 0;
	if (extent.y > extent.x && extent.y > extent.z) axis = 1;
	else if (extent.z > extent.x && extent.z > extent.y) axis = 2;

	float center;
	if (axis == 0) center = (minBound.x + maxBound.x) * 0.5f;
	else if (axis == 1) center = (minBound.y + maxBound.y) * 0.5f;
	else center = (minBound.z + maxBound.z) * 0.5f;

	vector<Triangle> left, right;
	for (auto& tri : tris) {
		float cx = (tri.v0.x + tri.v1.x + tri.v2.x) / 3.0f;
		if (axis == 1) cx = (tri.v0.y + tri.v1.y + tri.v2.y) / 3.0f;
		else if (axis == 2) cx = (tri.v0.z + tri.v1.z + tri.v2.z) / 3.0f;

		(cx < center ? left : right).push_back(tri);
	}

	if (left.empty() || right.empty()) {
		node->triangles = tris;
		return node;
	}

	node->left = BuildBVH(left, depth + 1);
	node->right = BuildBVH(right, depth + 1);
	return node;
}
bool TriangleTriangleIntersection(const Triangle& t1, const Triangle& t2,vector<XMFLOAT3>& outPoints, XMVECTOR& outNormalA, XMVECTOR& outNormalB, int& pointCount)
{
	using namespace std;
	auto Load = [](const XMFLOAT3& f) { return XMLoadFloat3(&f); };
	auto Store = [](XMVECTOR v) { XMFLOAT3 o; XMStoreFloat3(&o, v); return o; };

	XMVECTOR A0 = Load(t1.v0), A1 = Load(t1.v1), A2 = Load(t1.v2);
	XMVECTOR B0 = Load(t2.v0), B1 = Load(t2.v1), B2 = Load(t2.v2);

	XMVECTOR N1 = XMVector3Normalize(XMVector3Cross(A1 - A0, A2 - A0));
	XMVECTOR N2 = XMVector3Normalize(XMVector3Cross(B1 - B0, B2 - B0));
	const float EPS = 1e-6f;

	vector<XMVECTOR> rawContacts;

	auto ClipEdge = [&](XMVECTOR p1, XMVECTOR p2, XMVECTOR a, XMVECTOR b, XMVECTOR c) {
		XMVECTOR N = XMVector3Cross(b - a, c - a);
		XMVECTOR dir = p2 - p1;
		float denom = XMVectorGetX(XMVector3Dot(N, dir));
		if (fabs(denom) < EPS) return;

		float t = XMVectorGetX(XMVector3Dot(N, a - p1)) / denom;
		if (t < -EPS || t > 1.0f + EPS) return;

		XMVECTOR P = p1 + t * dir;
		if (PointInTriangle(P, a, b, c)) {
			rawContacts.push_back(P);
		}
		};

	// Kenar testleri
	ClipEdge(A0, A1, B0, B1, B2);
	ClipEdge(A1, A2, B0, B1, B2);
	ClipEdge(A2, A0, B0, B1, B2);
	ClipEdge(B0, B1, A0, A1, A2);
	ClipEdge(B1, B2, A0, A1, A2);
	ClipEdge(B2, B0, A0, A1, A2);

	// Köşe-yüz içeride kalma testleri
	if (PointInTriangle(A0, B0, B1, B2)) rawContacts.push_back(A0);
	if (PointInTriangle(A1, B0, B1, B2)) rawContacts.push_back(A1);
	if (PointInTriangle(A2, B0, B1, B2)) rawContacts.push_back(A2);
	if (PointInTriangle(B0, A0, A1, A2)) rawContacts.push_back(B0);
	if (PointInTriangle(B1, A0, A1, A2)) rawContacts.push_back(B1);
	if (PointInTriangle(B2, A0, A1, A2)) rawContacts.push_back(B2);

	if (rawContacts.empty()) return false;

	// Tekrarlı noktaları filtrele
	set<tuple<int, int, int>> unique;
	vector<XMVECTOR> filtered;
	for (auto& v : rawContacts) {
		XMFLOAT3 p;
		XMStoreFloat3(&p, v);
		auto key = make_tuple((int)(p.x * 1000), (int)(p.y * 1000), (int)(p.z * 1000));
		if (unique.insert(key).second) {
			filtered.push_back(v);
		}
	}

	if (filtered.empty()) return false;

	// Nokta azaltma stratejisi
	if (filtered.size() > 5) {
		// Bu bir kenar-kenar kesişimi gibi görünüyor
		XMVECTOR p1 = filtered.front();
		XMVECTOR p2 = filtered.back();
		outPoints.push_back(Store(p1));
		outPoints.push_back(Store(p2));
		pointCount += 2;
	}
	else if (filtered.size() > 1) {
		// Orta düzey temas: ortalama noktayı al
		XMVECTOR sum = XMVectorZero();
		for (auto& v : filtered) sum += v;
		XMVECTOR avg = sum / static_cast<float>(filtered.size());
		outPoints.push_back(Store(avg));
		pointCount += 1;
	}
	else {
		// Zaten tek benzersiz temas var
		outPoints.push_back(Store(filtered[0]));
		pointCount += 1;
	}
	outNormalA += N1;
	outNormalB += N2;
	//if (pointCount > 0) outNormal = XMVector3Normalize(outNormal/float(pointCount));
	return true;
}
bool TraverseBVH(BVHNode* a, BVHNode* b, vector<XMFLOAT3>& points, XMVECTOR& normalA, XMVECTOR& normalB, int& pointscount) {
	if (!a || !b) return false;
	bool overlap =
		(a->min.x <= b->max.x && a->max.x >= b->min.x) &&
		(a->min.y <= b->max.y && a->max.y >= b->min.y) &&
		(a->min.z <= b->max.z && a->max.z >= b->min.z);
	if (!overlap) return false;

	if (a->isLeaf() && b->isLeaf()) {
		bool found = false;
		for (auto& ta : a->triangles)
			for (auto& tb : b->triangles)
				if (TriangleTriangleIntersection(ta, tb, points, normalA,normalB, pointscount)) found = true;
		if (found) return true;
		else return false;
	}
	if (a->isLeaf()) return TraverseBVH(a, b->left, points, normalA, normalB, pointscount) || TraverseBVH(a, b->right, points, normalA, normalB, pointscount);
	if (b->isLeaf()) return TraverseBVH(a->left, b, points, normalA, normalB, pointscount) || TraverseBVH(a->right, b, points, normalA, normalB, pointscount);
	return TraverseBVH(a->left, b->left, points, normalA, normalB, pointscount) || TraverseBVH(a->left, b->right, points, normalA, normalB, pointscount) ||
		TraverseBVH(a->right, b->left, points, normalA, normalB, pointscount) || TraverseBVH(a->right, b->right, points, normalA, normalB, pointscount);
}
vector<intersect> NarrowPhase(vector<intersect> Intersects)
{
	vector<intersect> result;
	for (const auto& pair : Intersects) {
		Object* a = pair.a;
		Object* b = pair.b;
		string aName = pair.aName;
		string bName = pair.bName;
		vector<Triangle> trisA, trisB;
		if(moved[aName])
		{
			trisA.clear();
			for (int i = 0; i < a->vertexCount; i += 3) {
				Triangle t;
				XMStoreFloat3(&t.v0, XMVector3Transform(XMLoadFloat3(&a->vertices[i].position), g_World_of[aName]));
				XMStoreFloat3(&t.v1, XMVector3Transform(XMLoadFloat3(&a->vertices[i + 1].position), g_World_of[aName]));
				XMStoreFloat3(&t.v2, XMVector3Transform(XMLoadFloat3(&a->vertices[i + 2].position), g_World_of[aName]));
				t.index = i;
				trisA.push_back(t);
			}
			worldTriangles[aName] = trisA;
			if (bvh[aName]) DeleteBVH(bvh[aName]);
			bvh[aName] = BuildBVH(trisA);
		}
		if(moved[bName])
		{
			trisB.clear();
			for (int i = 0; i < b->vertexCount; i += 3) {
				Triangle t;
				XMStoreFloat3(&t.v0, XMVector3Transform(XMLoadFloat3(&b->vertices[i].position), g_World_of[bName]));
				XMStoreFloat3(&t.v1, XMVector3Transform(XMLoadFloat3(&b->vertices[i + 1].position), g_World_of[bName]));
				XMStoreFloat3(&t.v2, XMVector3Transform(XMLoadFloat3(&b->vertices[i + 2].position), g_World_of[bName]));
				t.index = i;
				trisB.push_back(t);
			}
			worldTriangles[bName] = trisB;
			if (bvh[bName]) DeleteBVH(bvh[bName]);
			bvh[bName] = BuildBVH(trisB);
		}

		vector<XMFLOAT3> points;
		XMVECTOR normalA = XMVectorSet(0, 0, 0, 0);
		XMVECTOR normalB = XMVectorSet(0, 0, 0, 0);
		XMVECTOR normal = XMVectorSet(0, 0, 0, 0);
		int pointsCount = 0;
		if (TraverseBVH(bvh[aName], bvh[bName], points, normalA,normalB, pointsCount))
		{
			if (pointsCount > 0)
			{
				if(a->mass==FLT_MAX)
					normal = XMVector3Normalize(normalA);
				else if(b->mass==FLT_MAX)
					normal = XMVector3Normalize(normalB);
				else
					normal = XMVector3Normalize(normalA + normalB);
			}
			else continue;
			normal = XMVector3Normalize(normal);
			XMVECTOR va = XMLoadFloat3(&a->momentum) / a->mass;
			XMVECTOR vb = XMLoadFloat3(&b->momentum) / b->mass;
			float vrel = XMVectorGetX(XMVector3Dot(vb - va, normal));
			result.emplace_back(a, b, normal, points, vrel, 0.0f,aName,bName);
		}
	}
	return result;
}
//FİZİKSEL HAREKETLER
void Action()
{
	XMFLOAT3 gravity = XMFLOAT3(0.0f, -9.81f, 0.0f);
	//Yerçekimi
	for (auto& [key, deger] : nesneler)
	{
		if (deger.density != FLT_MAX && deger.density!=0.001f)  //Cisim sabit ya da ağırlıksız değilse düşsün
		{
			//deger.momentum = AddFloat3(deger.momentum, XMFLOAT3(0.0f, -0.0005f*deger.mass, 0.0f));
			XMFLOAT3 gravityForce = MultiplyFloat3(gravity, deger.mass);
			deger.kuvvet = AddFloat3(deger.kuvvet, gravityForce);
		}
	}
	//butonlar
	if (GetAsyncKeyState('W') & 0x8000)
	{
		//nesneler["Tank"].kuvvet = AddFloat3(nesneler["Tank"].kuvvet, XMFLOAT3(0.0f, 0.0f, 10.0f));
		XMVECTOR forward = XMVector3Normalize(g_World_of["Tank"].r[2]);
		XMVECTOR forceVec = XMVectorScale(forward, 10.0f);
		XMFLOAT3 force;
		XMStoreFloat3(&force, forceVec);
		nesneler["Tank"].kuvvet = AddFloat3(nesneler["Tank"].kuvvet, force);
		if (nesneler["Tank"].kuvvet.z >= 10.0f) nesneler["Tank"].kuvvet.z = 10.0f;
	}
	if (GetAsyncKeyState('S') & 0x8000)
	{
		nesneler["Tank"].kuvvet = AddFloat3(nesneler["Tank"].kuvvet, XMFLOAT3(0.0f, 0.0f, -10.0f));
		if (nesneler["Tank"].kuvvet.z <= -10.0f) nesneler["Tank"].kuvvet.z = -10.0f;
	}
	if (GetAsyncKeyState('A') & 0x8000)
	{
		nesneler["Tank"].kuvvet = AddFloat3(nesneler["Tank"].kuvvet, XMFLOAT3(-10.0f, 0.0f, 0.0f));
		if (nesneler["Tank"].kuvvet.x <= -10.0f) nesneler["Tank"].kuvvet.x = -10.0f;
	}
	if (GetAsyncKeyState('D') & 0x8000)
	{
		nesneler["Tank"].kuvvet = AddFloat3(nesneler["Tank"].kuvvet, XMFLOAT3(10.0f, 0.0f, 0.0f));
		if (nesneler["Tank"].kuvvet.x >= 10.0f) nesneler["Tank"].kuvvet.x = 10.0f;
	}
	if (GetAsyncKeyState('F') & 0x8000)					// Animate Tank
	{
		FireEnemyMissile = false;
	}
	if (!(GetAsyncKeyState(VK_SPACE) & 0x8000))					// SPACE is UP
	{

	}
	if (GetAsyncKeyState(VK_SPACE) & 0x8000) //if (keyboardState[DIK_SPACE] & 0x80)					// SPACE is PRESSED
	{
		if (PlayTankFireSoundOnce)
			PlaySound(TEXT("fire.wav"), NULL, SND_FILENAME | SND_ASYNC);
		PlayTankFireSoundOnce = false;
		FireTankMissile = false;
		XMVECTOR yon = XMVector3Normalize((At - (Eye-XMVectorSet(0,0,0,1.0f))));
        XMVECTOR moment = XMVectorScale(yon, 0.00001f);
		XMStoreFloat3(&nesneler["Missile"].momentum, moment);	// Set Missile's momentum
	}
	if (!(GetAsyncKeyState(VK_LBUTTON) & 0x8000))				// Left Button is UP
	{
		nesneler["Tank"].angular = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
	}
	if (GetAsyncKeyState('R') & 0x8000)							// 'R' is PRESSED (for Reloading)
	{
		FireTankMissile = true;

		render["Enemy"] = true;
		render["Tank"] = true;

		render["Missile"] = true;
		render["Enemy_Missile"] = true;

		FireEnemyMissile = true;

		PlayTankFireSoundOnce = true;
		PlayTankHitSoundOnce = true;
	}
	if (!(GetAsyncKeyState(VK_RBUTTON) & 0x8000))		// Right Button is UP (for Zoom out)
	{

	}
	if (GetAsyncKeyState(VK_RBUTTON) & 0x8000)		// Right Button is PRESSED (for Zoom in)
	{

	}
	if (fabs(g_mouseDeltaX) > 1e-4f) // küçük farkları yok say
	{
		// Tankın açısal momentumuna Y ekseninde katkı
		XMVECTOR yawRotation = XMVectorSet(0.0f, g_mouseDeltaX * 5.0f, 0.0f, 0.0f); // çarpanla hız ayarı yap
		nesneler["Tank"].angular = yawRotation;
	}
	else
	{
		nesneler["Tank"].angular = XMVectorZero(); // fare hareket etmiyorsa dönmeyi durdur
	}
	if (FireTankMissile)
	{
		XMFLOAT3 vTank = DivideFloat3(nesneler["Tank"].momentum, nesneler["Tank"].mass);
		XMVECTOR wTank = XMVector3Transform(nesneler["Tank"].angular, nesneler["Tank"].inertiaInverse);
		XMFLOAT3 tempF = SubtractFloat3(nesneler["Missile"].position, nesneler["Tank"].position);
		XMVECTOR MissileToTank = XMLoadFloat3(&tempF);
		XMVECTOR tempVec = XMVector3Cross(wTank, MissileToTank);
		XMStoreFloat3(&tempF, tempVec);
		tempF = AddFloat3(vTank, tempF);
		tempVec = XMLoadFloat3(&tempF);
		XMVECTOR tempMomentum = XMVectorScale(tempVec, nesneler["Missile"].mass);
		XMStoreFloat3(&tempF, tempMomentum);
		nesneler["Missile"].momentum = tempF;
		nesneler["Missile"].angular = XMVector3Cross(MissileToTank, tempMomentum);
	}
	if (FireEnemyMissile)
	{
		XMFLOAT3 vTank = DivideFloat3(nesneler["Enemy"].momentum, nesneler["Enemy"].mass);
		XMVECTOR wTank = XMVector3Transform(nesneler["Enemy"].angular, nesneler["Enemy"].inertiaInverse);
		XMFLOAT3 tempF = SubtractFloat3(nesneler["Enemy_Missile"].position, nesneler["Enemy"].position);
		XMVECTOR MissileToTank = XMLoadFloat3(&tempF);
		XMVECTOR tempVec = XMVector3Cross(wTank, MissileToTank);
		XMStoreFloat3(&tempF, tempVec);
		tempF = AddFloat3(vTank, tempF);
		tempVec = XMLoadFloat3(&tempF);
		XMVECTOR tempMomentum = XMVectorScale(tempVec, nesneler["Enemy_Missile"].mass);
		XMStoreFloat3(&tempF, tempMomentum);
		nesneler["Enemy_Missile"].momentum = tempF;
		nesneler["Enemy_Missile"].angular = XMVector3Cross(MissileToTank, tempMomentum);
	}
}
void Collision(intersect& contact)
{
	Object* A = contact.a;
	Object* B = contact.b;
	XMVECTOR n = XMVector3Normalize(contact.normal);

	float totalImpulse = 0.0f;
	float minPenetration = FLT_MAX;
	bool aImmovable = (A->mass == FLT_MAX);
	bool bImmovable = (B->mass == FLT_MAX);
	if (aImmovable && bImmovable) return;
	/*if (contact.aName == "RedDot" || contact.bName == "RedDot") return;*/
	if (contact.aName == "Missile" || contact.bName == "Missile")
	{
		render["Missile"] = false;
		/*if (contact.bName == "Enemy" || contact.aName == "Enemy")
		{
			render["Enemy"] = false;
			if (FireEnemyMissile) render["Enemy_Missile"] = false;
			if (PlayTankHitSoundOnce)
			{
				PlaySound(TEXT("hit.wav"), NULL, SND_FILENAME | SND_ASYNC); PlayTankHitSoundOnce = false;
			}
			return;
		}
		if (contact.bName == "Enemy_Missile" || contact.aName == "Enemy_Missile")
		{
			render["Enemy_Missile"] = false;
			if (PlayTankHitSoundOnce)
			{
				PlaySound(TEXT("hit.wav"), NULL, SND_FILENAME | SND_ASYNC); PlayTankHitSoundOnce = false;
			}
			return;
		}*/
		if (aImmovable || bImmovable || contact.aName == "Tank" || contact.bName == "Tank") return;
	}
	/*if (contact.aName == "Enemy_Missile" || contact.bName == "Enemy_Missile")
	{
		render["Enemy_Missile"] = false;
		if (contact.aName == "Tank" || contact.bName == "Tank")
		{
			render["Tank"] = false;
			if (FireTankMissile) render["Missile"] = false;
			if (PlayTankHitSoundOnce)
			{
				PlaySound(TEXT("hit.wav"), NULL, SND_FILENAME | SND_ASYNC); PlayTankHitSoundOnce = false;
			}
			return;
		}
		if (contact.bName == "Missile" || contact.aName == "Missile")
		{
			render["Missile"] = false;
			if (PlayTankHitSoundOnce)
			{
				PlaySound(TEXT("hit.wav"), NULL, SND_FILENAME | SND_ASYNC); PlayTankHitSoundOnce = false;
			}
			return;
		}
		if (aImmovable || bImmovable || contact.bName == "Enemy" || contact.aName == "Enemy") return;
	}*/
	//if cisimlerden biri mermi ise tüm momentumunu karşıya aktar

	if (contact.temas.empty()) return;

	XMVECTOR posA = XMLoadFloat3(&A->position);
	XMVECTOR posB = XMLoadFloat3(&B->position);

	float restitution = 0.0f;
	float yonA;
	float yonB;

	XMVECTOR contactPoint = XMVectorZero();
	for (const XMFLOAT3& pt : contact.temas)
		contactPoint += XMLoadFloat3(&pt);
	contactPoint /= static_cast<float>(contact.temas.size());

	
		XMVECTOR vA = XMLoadFloat3(&A->momentum) / A->mass;
		XMVECTOR vB = XMLoadFloat3(&B->momentum) / B->mass;

		if (!XMVector3Equal(vA, XMVectorZero())) {
			yonA = XMVectorGetX(XMVector3Dot(vA, n));
			yonA = -yonA / abs(yonA);
		}
		if (!XMVector3Equal(vB, XMVectorZero())) {
			yonB = XMVectorGetX(XMVector3Dot(vB, n));
			yonB = -yonB / abs(yonB);
		}
		else if (!XMVector3Equal(vA, XMVectorZero())) yonB = -yonA;
		if (XMVector3Equal(vA, XMVectorZero()) && !XMVector3Equal(vB, XMVectorZero())) yonA = -yonB;

		XMVECTOR rA = contactPoint - posA;
		XMVECTOR rB = contactPoint - posB;

		XMVECTOR vAngularA = XMVector3Cross(A->angular, rA);
		XMVECTOR vAngularB = XMVector3Cross(B->angular, rB);

		XMVECTOR vTotalA = vA + vAngularA;
		XMVECTOR vTotalB = vB + vAngularB;

		XMVECTOR vRelVec = vTotalB - vTotalA;
		float vRelN = XMVectorGetX(XMVector3Dot(vRelVec, n));
		if (vRelN >= 0) return;//continue;

		float denom = 0.0f;

		if (!aImmovable) 
		{

			XMVECTOR rA_cross_n = XMVector3Cross(rA, n);
			XMVECTOR termA = XMVector3Transform(XMVector3Cross(rA_cross_n, rA), A->inertiaInverse);
			denom += (1.0f / A->mass) + XMVectorGetX(XMVector3Dot(rA_cross_n, termA));
		}
		if (!bImmovable) 
		{

			XMVECTOR rB_cross_n = XMVector3Cross(rB, n);
			XMVECTOR termB = XMVector3Transform(XMVector3Cross(rB_cross_n, rB), B->inertiaInverse);
			denom += (1.0f / B->mass) + XMVectorGetX(XMVector3Dot(rB_cross_n, termB));
		}

		// Güvenlik için minimum sınır koy
		if (denom < 1e-3f) denom = 1e-3f;
		float j = -(1.0f + restitution) * vRelN / denom;
		XMVECTOR impulseN = j * n;
		ostringstream os;
		if(!isnan(j)) os<<"Collision : "<<contact.aName<<"<->"<<contact.bName<<"önceki vrel:" <<contact.vRel<<"şimdiki vrel"<<vRelN << " impulse: " << j << ", normal x: " << XMVectorGetX(n) << ", y: " << XMVectorGetY(n) << ", z: " << XMVectorGetZ(n) << endl << endl;
		
		totalImpulse += j;
		XMVECTOR deltaL_A = XMVectorZero();
		XMVECTOR deltaL_B = XMVectorZero();

		if (!aImmovable && XMVectorGetX(XMVector3LengthSq(XMVector3Cross(rA, impulseN))) > 1e-6f)
			deltaL_A = XMVector3Cross(rA, yonA * impulseN);

		if (!bImmovable && XMVectorGetX(XMVector3LengthSq(XMVector3Cross(rB, impulseN))) > 1e-6f)
			deltaL_B = XMVector3Cross(rB, yonB * impulseN);
		// Momentum güncelle

		if( !aImmovable)
		{
			os << "A eski momentum: " << A->momentum.x << ", " << A->momentum.y << ", " << A->momentum.z << ", angular x: " << XMVectorGetX(A->angular) << ", y: " << XMVectorGetY(A->angular) << ", z: " << XMVectorGetZ(A->angular) << endl;
			XMStoreFloat3(&A->momentum, XMLoadFloat3(&A->momentum) + yonA*impulseN);
			if(contact.aName!="Tank") A->angular = A->angular + deltaL_A;
			float angularMax = 5.0f;
			if (XMVectorGetX(XMVector3Length(A->angular)) > angularMax)
				A->angular = XMVector3Normalize(A->angular) * angularMax;
			os << "A momentum: " << A->momentum.x<<", "<< A->momentum.y <<", "<< A->momentum.z << ", angular x: " << XMVectorGetX(A->angular) << ", y: " << XMVectorGetY(A->angular) << ", z: " << XMVectorGetZ(A->angular) << endl;
		}
		if( !bImmovable)
		{
			os << "B eski momentum: " << B->momentum.x << ", " << B->momentum.y << ", " << B->momentum.z << ", angular x: " << XMVectorGetX(B->angular) << ", y: " << XMVectorGetY(B->angular) << ", z: " << XMVectorGetZ(B->angular) << endl<<endl;
			XMStoreFloat3(&B->momentum, XMLoadFloat3(&B->momentum) + yonB*impulseN);
			if(contact.bName!="Tank")B->angular = B->angular + deltaL_B;
			float angularMax = 5.0f;
			if (XMVectorGetX(XMVector3Length(B->angular)) > angularMax)
				B->angular = XMVector3Normalize(B->angular) * angularMax;
			os << "B momentum: "<< B->momentum.x << ", " << B->momentum.y << ", " << B->momentum.z << ", angular x: " << XMVectorGetX(B->angular) << ", y: " << XMVectorGetY(B->angular) << ", z: " << XMVectorGetZ(B->angular) << endl<<endl;
		}
		//OutputDebugStringA(os.str().c_str());

		// Dinamik sürtünme
		XMVECTOR vRelTangent = vRelVec - vRelN * n;
		float vRelTangentLen = XMVectorGetX(XMVector3Length(vRelTangent));
		if (vRelTangentLen > 1e-4f)
		{
			XMVECTOR tangent = XMVector3Normalize(vRelTangent);
			float mu = sqrt(A->dinamikSurt * B->dinamikSurt);
			XMVECTOR frictionImpulse = -mu * j * tangent;
			os << "frictionImpulse x: " << XMVectorGetX(frictionImpulse) << ", y: " << XMVectorGetY(frictionImpulse) << ", z: " << XMVectorGetZ(frictionImpulse) << endl;

			if (!aImmovable)
			{
				XMStoreFloat3(&A->momentum, XMLoadFloat3(&A->momentum) + frictionImpulse);
				if(contact.aName!="Tank")
				A->angular += XMVector3Transform(XMVector3Cross(rA, frictionImpulse), A->inertiaInverse);
			}
			if (!bImmovable) 
			{
				XMStoreFloat3(&B->momentum, XMLoadFloat3(&B->momentum) - frictionImpulse);
				if(contact.bName!="Tank")
				B->angular -= XMVector3Transform(XMVector3Cross(rB, frictionImpulse), B->inertiaInverse);
			}
		}
		float distance = XMVectorGetX(XMVector3Dot(/*XMLoadFloat3(&pt)*/contactPoint - XMLoadFloat3(&A->position), contact.normal));
		minPenetration = min(minPenetration, distance);
	//}

	contact.Impulse = totalImpulse / max(contact.temas.size(), size_t(1));
	//float penetration = max(0.0f, -minPenetration + 0.0001f);
	//float penetration = max(0.0f, -minPenetration) * 0.5f;
	float penetration = 0.0f;
	for (const XMFLOAT3& pt : contact.temas)
	{
		XMVECTOR p = XMLoadFloat3(&pt);
		XMVECTOR aToP = p - XMLoadFloat3(&A->position);
		float proj = XMVectorGetX(XMVector3Dot(aToP, contact.normal));
		if (proj < 0) // içeri girmişse
			penetration += -proj;
	}
	penetration /= max(contact.temas.size(), size_t(1));
	XMVECTOR correction = contact.normal * penetration * 1.0f; // veya 1.0f tam düzeltme

	//ostringstream os;
	os <<"penetrasyon: " << penetration << ", correction x: " << XMVectorGetX(correction) << ", y: " << XMVectorGetY(correction) << ", z: " << XMVectorGetZ(correction) << endl<<endl;
	OutputDebugStringA(os.str().c_str());

	//XMVECTOR correction = penetration * contact.normal;
	if (!aImmovable && !bImmovable)
	{
		XMStoreFloat3(&A->position, XMLoadFloat3(&A->position) + yonA*correction * (B->mass / (A->mass + B->mass)));
		XMStoreFloat3(&B->position, XMLoadFloat3(&B->position) + yonB*correction * (A->mass / (A->mass + B->mass)));
		g_World_of[contact.aName] = XMMatrixTranslationFromVector(XMLoadFloat3(&A->position));
		g_World_of[contact.bName] = XMMatrixTranslationFromVector(XMLoadFloat3(&B->position));
	}
	else if (!aImmovable)
	{
		XMStoreFloat3(&A->position, XMLoadFloat3(&A->position) + yonA*correction);
		g_World_of[contact.aName] = XMMatrixTranslationFromVector(XMLoadFloat3(&A->position));
	}
	else if (!bImmovable)
	{
		XMStoreFloat3(&B->position, XMLoadFloat3(&B->position) +yonB*correction);
		g_World_of[contact.bName] = XMMatrixTranslationFromVector(XMLoadFloat3(&B->position));
	}
}

void Contact(intersect& contact)
{
	Object* A = contact.a;
	Object* B = contact.b;
	
	if (A->mass == FLT_MAX && B->mass == FLT_MAX) return;
	bool aImmovable = (A->mass == FLT_MAX);
	bool bImmovable = (B->mass == FLT_MAX);
	if (aImmovable && bImmovable) return;
	/*if (contact.aName == "RedDot" || contact.bName == "RedDot") return;*/
	if (contact.aName == "Missile" || contact.bName == "Missile")
	{
		render["Missile"] = false;
		/*if (contact.bName == "Enemy" || contact.aName == "Enemy")
		{
			render["Enemy"] = false;
			if (FireEnemyMissile) render["Enemy_Missile"] = false;
			if (PlayTankHitSoundOnce)
			{
				PlaySound(TEXT("hit.wav"), NULL, SND_FILENAME | SND_ASYNC); PlayTankHitSoundOnce = false;
			}
			return;
		}
		if (contact.bName == "Enemy_Missile" || contact.aName == "Enemy_Missile")
		{
			render["Enemy_Missile"] = false;
			if (PlayTankHitSoundOnce)
			{
				PlaySound(TEXT("hit.wav"), NULL, SND_FILENAME | SND_ASYNC); PlayTankHitSoundOnce = false;
			}
			return;
		}*/
		if (aImmovable || bImmovable || contact.aName == "Tank" || contact.bName == "Tank") return;
	}
	/*if (contact.aName == "Enemy_Missile" || contact.bName == "Enemy_Missile")
	{
		render["Enemy_Missile"] = false;
		if (contact.aName == "Tank" || contact.bName == "Tank")
		{
			render["Tank"] = false;
			if (FireTankMissile) render["Missile"] = false;
			if (PlayTankHitSoundOnce)
			{
				PlaySound(TEXT("hit.wav"), NULL, SND_FILENAME | SND_ASYNC); PlayTankHitSoundOnce = false;
			}
			return;
		}
		if (contact.bName == "Missile" || contact.aName == "Missile")
		{
			render["Missile"] = false;
			if (PlayTankHitSoundOnce)
			{
				PlaySound(TEXT("hit.wav"), NULL, SND_FILENAME | SND_ASYNC); PlayTankHitSoundOnce = false;
			}
			return;
		}
		if (aImmovable || bImmovable || contact.bName == "Enemy" || contact.aName == "Enemy") return;
	}*/
	if (contact.temas.empty()) return;
	
	XMVECTOR n = XMVector3Normalize(contact.normal);

	// 1. Net kuvvetleri al
	XMVECTOR FA = XMLoadFloat3(&A->kuvvet);
	XMVECTOR FB = XMLoadFloat3(&B->kuvvet);
	XMVECTOR relForce = FB - FA;

	// 2. Kuvvetlerin normal bileşeni (dış kuvvetlerin temas doğrultusundaki bileşeni)
	float forceRelN = XMVectorGetX(XMVector3Dot(relForce, n));
	if (forceRelN == 0.0f || forceRelN >= 0.0f) return;

	const float stiffness = 10000.0f;

	//contact.Impulse = 0.0f; // Toplam temas kuvveti (isteğe bağlı)

	XMVECTOR contactPoint = XMVectorZero();
	for (const XMFLOAT3& pt : contact.temas)
		contactPoint += XMLoadFloat3(&pt);
	contactPoint /= static_cast<float>(contact.temas.size());
	//for (const auto& pt : contact.temas)
	//{
		//XMVECTOR contactPoint = XMLoadFloat3(&pt);
		XMVECTOR posA = XMLoadFloat3(&A->position);
		XMVECTOR posB = XMLoadFloat3(&B->position);

		// Penetrasyon
		float penetration = -XMVectorGetX(XMVector3Dot(contactPoint - posA, n));
		if (penetration <= 0.0f) return;//continue;

		// Penetrasyon kuvveti
		float forcePenetration = stiffness * penetration;

		// Temas için normal kuvvet, penetrasyon ve dış kuvvetin normal bileşeninin maksimumu
		float normalForceMag = max(forcePenetration, forceRelN);
		if (normalForceMag <= 0.0f) return;//continue;

		contact.Impulse = normalForceMag;

		XMVECTOR rA = contactPoint - posA;
		XMVECTOR rB = contactPoint - posB;

		// Hızlar (momentum/mass + açısal hız x r)
		XMVECTOR vA = XMLoadFloat3(&A->kuvvet) / A->mass + XMVector3Cross(A->tork, rA);
		XMVECTOR vB = XMLoadFloat3(&B->kuvvet) / B->mass + XMVector3Cross(B->tork, rB);

		float yonA;
		float yonB;
		if (!XMVector3Equal(vA, XMVectorZero())) {
			yonA = XMVectorGetX(XMVector3Dot(vA, n));
			yonA = -yonA / abs(yonA);
		}
		if (!XMVector3Equal(vB, XMVectorZero())) {
			yonB = XMVectorGetX(XMVector3Dot(vB, n));
			yonB = -yonB / abs(yonB);
		}
		else if (!XMVector3Equal(vA, XMVectorZero())) yonB = -yonA;
		if (XMVector3Equal(vA, XMVectorZero()) && !XMVector3Equal(vB, XMVectorZero())) yonA = -yonB;

		// Göreli hız
		XMVECTOR relVel = vB - vA;

		// Göreli hızın temas normaline dik bileşeni (kayma hızı)
		XMVECTOR velNormalComp = XMVector3Dot(relVel, n) * n;
		XMVECTOR velTangent = relVel - velNormalComp;
		float velTangentLen = XMVectorGetX(XMVector3Length(velTangent));

		// Sürtünme kuvveti
		XMVECTOR frictionForce = XMVectorZero();

		if (velTangentLen < 1e-4f)
		{
			float maxStaticFriction = normalForceMag * min(A->statikSurt, B->statikSurt);
			if (XMVector3Length(relVel).m128_f32[0] > 1e-8f) // Sıfıra yakın hızsa normalize etme
			{
				frictionForce = -relVel;
				frictionForce = XMVector3Normalize(frictionForce) * maxStaticFriction;
			}
			else
			{
				frictionForce = XMVectorZero();
			}
		}
		else
		{
			float dynamicFrictionMag = normalForceMag * min(A->dinamikSurt, B->dinamikSurt);
			frictionForce = -velTangent / velTangentLen * dynamicFrictionMag;
		}

		XMVECTOR normalForceVec = normalForceMag * n;
		XMVECTOR totalForce = normalForceVec + frictionForce;

		if (!aImmovable)
		{
			XMFLOAT3 f;
			XMStoreFloat3(&f, totalForce*yonA);
			A->kuvvet = AddFloat3(A->kuvvet, f);
			A->tork += XMVector3Cross(totalForce*yonA, rA);
		}
		if (!bImmovable)
		{
			XMFLOAT3 f;
			XMStoreFloat3(&f, totalForce*yonB);
			B->kuvvet = AddFloat3(B->kuvvet, f);
			B->tork += XMVector3Cross(totalForce*yonB, rB);
		}
	//}
	ostringstream os;
	os <<"contact: " << contact.aName << "<->" << contact.bName 
		<< ", normal x: " << XMVectorGetX(contact.normal) << ", y: " << XMVectorGetY(contact.normal) << ", z: " << XMVectorGetZ(contact.normal) 
		<< ", Impulse: " << contact.Impulse << ", normal x: " << XMVectorGetX(n) << ", y: " << XMVectorGetY(n) << ", z: " << XMVectorGetZ(n)
		<<" total friction:"<<XMVectorGetX(totalForce)<<", " << XMVectorGetY(totalForce) << ", " << XMVectorGetZ(totalForce) << ", " << endl;
	OutputDebugStringA(os.str().c_str());
}

void Reaction(vector<intersect> Intersects)
{
	//for(int j=0;j<6;j++)
	//{
		for (int i = 0; i < Intersects.size(); i++)
		{
			if (Intersects[i].vRel >= 1e-4f)
			{
				ostringstream uz;
				uz << "ayrılıyor: " << Intersects[i].aName << "<->" << Intersects[i].bName << ", vRel: " << Intersects[i].vRel << endl;
				OutputDebugStringA(uz.str().c_str());
				continue;
			}
			else if (Intersects[i].vRel <= -1e-4f)
				Collision(Intersects[i]);
			else /*if (Intersects[i].vRel < 1e-4f)*/
				Contact(Intersects[i]);
		}
	//}
}

void Effect(float deltaTime)
{
	rotation += 0.01f;
	XMMATRIX mRotate = XMMatrixRotationY(rotation);
	XMMATRIX mTranslate = XMMatrixTranslation(-50.0f, 100.0f, 0.0f);
	XMVECTOR xmvLightPos = XMVectorSet(0, 0, 0, 0);
	xmvLightPos = XMVector3Transform(xmvLightPos, mTranslate);
	xmvLightPos = XMVector3Transform(xmvLightPos, mRotate);
	XMStoreFloat4(&m_constantBufferData.mLightPos, xmvLightPos);

	for (auto& [key, deger] : nesneler)
	{
		if (!render[key]) continue;

		// Fırlatma fiziksel başlangıç
		if (key == "Missile" && FireTankMissile)
		{
			XMFLOAT3 vTank = DivideFloat3(nesneler["Tank"].momentum, nesneler["Tank"].mass);
			XMVECTOR wTank = XMVector3Transform(nesneler["Tank"].angular, nesneler["Tank"].inertiaInverse);
			XMFLOAT3 tempF = SubtractFloat3(deger.position, nesneler["Tank"].position);
			XMVECTOR MissileToTank = XMLoadFloat3(&tempF);
			XMVECTOR tempVec = XMVector3Cross(wTank, MissileToTank);
			XMStoreFloat3(&tempF, tempVec);
			tempF = AddFloat3(vTank, tempF);
			tempVec = XMLoadFloat3(&tempF);
			XMVECTOR tempMomentum = XMVectorScale(tempVec, deger.mass);
			XMStoreFloat3(&tempF, tempMomentum);
			deger.momentum = tempF;
			deger.angular = XMVector3Cross(MissileToTank, tempMomentum);
		}
		else if (key == "Enemy_Missile" && FireEnemyMissile)
		{
			XMFLOAT3 vEnemy = DivideFloat3(nesneler["Enemy"].momentum, nesneler["Enemy"].mass);
			XMVECTOR wEnemy = XMVector3Transform(nesneler["Enemy"].angular, nesneler["Enemy"].inertiaInverse);
			XMFLOAT3 tempF = SubtractFloat3(deger.position, nesneler["Enemy"].position);
			XMVECTOR MissileToEnemy = XMLoadFloat3(&tempF);
			XMVECTOR tempVec = XMVector3Cross(wEnemy, MissileToEnemy);
			XMStoreFloat3(&tempF, tempVec);
			tempF = AddFloat3(vEnemy, tempF);
			tempVec = XMLoadFloat3(&tempF);
			XMVECTOR tempMomentum = XMVectorScale(tempVec, deger.mass);
			XMStoreFloat3(&tempF, tempMomentum);
			deger.momentum = tempF;
			deger.angular = XMVector3Cross(MissileToEnemy, tempMomentum);
		}


		if(deger.mass!=FLT_MAX)
		{
			// Kuvvet ve tork güncelle
			deger.momentum = AddFloat3(deger.momentum, MultiplyFloat3(deger.kuvvet, deltaTime));
			deger.angular = XMVectorAdd(deger.angular, XMVectorScale(deger.tork, deltaTime));
			deger.kuvvet = XMFLOAT3(0.0f, 0.0f, 0.0f);
			deger.tork = XMVectorZero();
		}
		XMVECTOR v = XMLoadFloat3(&deger.momentum);
		if (XMVectorGetX(XMVector3Length(v)) <= 1e-6f && XMVectorGetX(XMVector3Length(deger.angular)) <= 1e-6f)
		{
			moved[key] = false;
		}
		else
		{
			moved[key] = true;
		}

		XMVECTOR position = XMLoadFloat3(&deger.position);

		if (deger.mass != FLT_MAX && moved[key])
		{
			// Konum güncelle
			XMVECTOR linearVelocity = XMVectorScale(XMLoadFloat3(&deger.momentum), 1.0f / deger.mass);
			position = XMVectorAdd(position, XMVectorScale(linearVelocity, deltaTime));
			XMStoreFloat3(&deger.position, position);
			XMMATRIX mTranslate = XMMatrixTranslationFromVector(position);

			// Dönüş var mı?
			XMVECTOR angularVelocity = XMVector3Transform(deger.angular, deger.inertiaInverse);
			float angle = XMVectorGetX(XMVector3Length(angularVelocity));
			if (angle > 1e-6f)
			{
				XMVECTOR axis = XMVector3Normalize(angularVelocity);
				XMVECTOR qRotation = XMQuaternionRotationAxis(axis, angle);
				XMMATRIX mRotate = XMMatrixRotationQuaternion(qRotation);
				g_World_of[key] = mRotate * mTranslate;
			}
			else
			{
				g_World_of[key] = mTranslate;
			}
			/*ostringstream ek;
			ek << "deltatime: " << deltaTime<<", " << key << " pozisyon: "<<nesneler[key].position.x<<", " << nesneler[key].position.y <<", " << nesneler[key].position.z <<", momentum: "<<nesneler[key].momentum.x <<", " << nesneler[key].momentum.y << ", " << nesneler[key].momentum.z<<endl;
			OutputDebugStringA(ek.str().c_str());*/
		}
		// Kamera takibi varsa burada (tank için)
		if (key == "Tank")
		{
			XMVECTOR tankPos = position;
			XMVECTOR cameraOffset = XMVectorSet(0.0f, 2.0f, -3.0f, 0.0f);
			XMVECTOR cameraPos = XMVectorAdd(tankPos, cameraOffset);
			Eye = cameraPos;
			At = tankPos+XMVectorSet(0,2,0,0);
			Up = XMVectorSet(0, 1, 0, 0);
			g_View = XMMatrixLookAtLH(Eye, At, Up);
		}

		// World matrix buffer'a yaz
		
		m_constantBufferData.mWorld = XMMatrixTranspose(g_World_of[key]);
		m_constantBufferData.mView = XMMatrixTranspose(g_View);
		m_constantBufferData.mProjection = XMMatrixTranspose(g_Projection);
		m_constantBufferData.mMeshColor = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

		memcpy(m_pCbvDataBegin + sira[key] * 256, &m_constantBufferData, sizeof(m_constantBufferData));
	}
}

void OnInit(string objPath, string texPath)
{
#if defined(_DEBUG)
	// Enable the D3D12 debug layer.
	{
		ComPtr<ID3D12Debug> debugController;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
		{
			debugController->EnableDebugLayer();
		}
	}
#endif

ComPtr<IDXGIFactory4> factory;
ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));

if (m_useWarpDevice)
{
	ComPtr<IDXGIAdapter> warpAdapter;
	ThrowIfFailed(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));

	ThrowIfFailed(D3D12CreateDevice(
		warpAdapter.Get(),
		D3D_FEATURE_LEVEL_11_0,
		IID_PPV_ARGS(&m_device)
	));
}
else
{
	ComPtr<IDXGIAdapter1> hardwareAdapter;
	GetHardwareAdapter(factory.Get(), &hardwareAdapter);

	ThrowIfFailed(D3D12CreateDevice(
		hardwareAdapter.Get(),
		D3D_FEATURE_LEVEL_11_0,
		IID_PPV_ARGS(&m_device)
	));
}

nesneler = Obj_Lister(objPath);
texture = Texture_Finder(texPath);
for (auto& [key, path] : texture)
{
	textureBuffer_of[key] = nullptr;
	textureBufferUploadHeap_of[key] = nullptr;
}

// Describe and create the command queue.
D3D12_COMMAND_QUEUE_DESC queueDesc = {};
queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));

// Describe and create the swap chain.
DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
swapChainDesc.BufferCount = FrameCount;
swapChainDesc.Width = m_width;
swapChainDesc.Height = m_height;
swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
swapChainDesc.SampleDesc.Count = 1;

ComPtr<IDXGISwapChain1> swapChain;
ThrowIfFailed(factory->CreateSwapChainForHwnd(
	m_commandQueue.Get(),		// Swap chain needs the queue so that it can force a flush on it.
	m_hwnd,
	&swapChainDesc,
	nullptr,
	nullptr,
	&swapChain
));

// This sample does not support fullscreen transitions.
ThrowIfFailed(factory->MakeWindowAssociation(m_hwnd, DXGI_MWA_NO_ALT_ENTER));

ThrowIfFailed(swapChain.As(&m_swapChain));
m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

// Create descriptor heaps.
{
	// Describe and create a render target view (RTV) descriptor heap.
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
	rtvHeapDesc.NumDescriptors = FrameCount;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));

	m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	// Describe and create a depth stencil view (DSV) descriptor heap.
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	ThrowIfFailed(m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap)));

	// Create the SRV heap.
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = texture.size();	// Önemli! : Doku sayısı değişince bunu da güncelle
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	ThrowIfFailed(m_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_descriptorHeap)));
}

// Create frame resources.
{
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

	// Create a RTV for each frame.
	for (UINT n = 0; n < FrameCount; n++)
	{
		ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));
		m_device->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, rtvHandle);
		rtvHandle.Offset(1, m_rtvDescriptorSize);
	}
}

ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));

// Create the command list.
ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), m_pipelineState_Textured.Get(), IID_PPV_ARGS(&m_commandList)));

// Create synchronization objects and wait until assets have been uploaded to the GPU.
{
	ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
	m_fenceValue = 1;

	// Create an event handle to use for frame synchronization.
	m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (m_fenceEvent == nullptr)
	{
		ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
	}

	// Wait for the command list to execute; we are reusing the same command 
	// list in our main loop but for now, we just want to wait for setup to 
	// complete before continuing.
	WaitForPreviousFrame();
}

// Graphics root signature.
{
	D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

	// This is the highest version the sample supports. If CheckFeatureSupport succeeds, the HighestVersion returned will not be greater than this.
	featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

	if (FAILED(m_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
	{
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
	}

	CD3DX12_DESCRIPTOR_RANGE1 ranges[1];
	ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);

	CD3DX12_ROOT_PARAMETER1 rootParameters[2];
	rootParameters[0].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC, D3D12_SHADER_VISIBILITY_ALL);
	rootParameters[1].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);

	// create a static sampler
	D3D12_STATIC_SAMPLER_DESC sampler = {};
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.MipLODBias = 0;
	sampler.MaxAnisotropy = 0;
	sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	sampler.MinLOD = 0.0f;
	sampler.MaxLOD = D3D12_FLOAT32_MAX;
	sampler.ShaderRegister = 0;
	sampler.RegisterSpace = 0;
	sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
	rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> signature;
	ComPtr<ID3DBlob> error;
	ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error));
	ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
}

// Create the pipeline state, which includes compiling and loading shaders.
{
	ComPtr<ID3DBlob> vertexShader;
	ComPtr<ID3DBlob> pixelShader_Textured;
	ComPtr<ID3DBlob> pixelShader_Phong;
	ComPtr<ID3DBlob> pixelShader_Solid;

#if defined(_DEBUG)
	// Enable better shader debugging with the graphics debugging tools.
	UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	UINT compileFlags = 0;
#endif

	ThrowIfFailed(D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, nullptr));
	ThrowIfFailed(D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr, "PS_Textured", "ps_5_0", compileFlags, 0, &pixelShader_Textured, nullptr));
	ThrowIfFailed(D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr, "PS_Phong", "ps_5_0", compileFlags, 0, &pixelShader_Phong, nullptr));
	ThrowIfFailed(D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr, "PS_Solid", "ps_5_0", compileFlags, 0, &pixelShader_Solid, nullptr));

	// Define the vertex input layout.
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	// Describe and create the graphics pipeline state object (PSO).
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDescDefault = {};
	psoDescDefault.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
	psoDescDefault.pRootSignature = m_rootSignature.Get();
	psoDescDefault.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
	psoDescDefault.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDescDefault.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDescDefault.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDescDefault.SampleMask = UINT_MAX;
	psoDescDefault.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDescDefault.NumRenderTargets = 1;
	psoDescDefault.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDescDefault.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	psoDescDefault.SampleDesc.Count = 1;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDescTextured = psoDescDefault;
	psoDescTextured.PS = CD3DX12_SHADER_BYTECODE(pixelShader_Textured.Get());
	ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDescTextured, IID_PPV_ARGS(&m_pipelineState_Textured)));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDescPhong = psoDescDefault;
	psoDescPhong.PS = CD3DX12_SHADER_BYTECODE(pixelShader_Phong.Get());
	ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDescPhong, IID_PPV_ARGS(&m_pipelineState_Phong)));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDescSolid = psoDescDefault;
	psoDescSolid.PS = CD3DX12_SHADER_BYTECODE(pixelShader_Solid.Get());
	ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDescSolid, IID_PPV_ARGS(&m_pipelineState_Solid)));
}
// Create the depth stencil view.
{
	D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilDesc = {};
	depthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;
	depthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	depthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

	D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
	depthOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
	depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
	depthOptimizedClearValue.DepthStencil.Stencil = 0;

	ThrowIfFailed(m_device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, m_width, m_height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&depthOptimizedClearValue,
		IID_PPV_ARGS(&m_depthStencil)
	));

	m_device->CreateDepthStencilView(m_depthStencil.Get(), &depthStencilDesc, m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
}

const UINT cbvSrvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
// Get a handle to the start of the descriptor heap.
CD3DX12_CPU_DESCRIPTOR_HANDLE m_descriptorHeapHandle(m_descriptorHeap->GetCPUDescriptorHandleForHeapStart());

for ( auto& [key,deger] : nesneler)
{
	g_World_of[key] = XMMatrixIdentity();
	render[key] = true;
	const UINT vertexBufferSize = deger.vertexCount * sizeof(Vertex); 
	ThrowIfFailed(
		m_device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		IID_PPV_ARGS(&m_vertexBuffer_of[key])));

	// Copy the triangle data to the vertex buffer.
	UINT8* pVertexDataBegin;
	CD3DX12_RANGE readRange(0, 0);		// We do not intend to read from this resource on the CPU.
	ThrowIfFailed(m_vertexBuffer_of[key]->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
	memcpy(pVertexDataBegin, &deger.vertices[0], vertexBufferSize);   //&deger.vertices[0]
	m_vertexBuffer_of[key]->Unmap(0, nullptr);

	// Initialize the vertex buffer view.
	m_vertexBufferView_of[key].BufferLocation = m_vertexBuffer_of[key]->GetGPUVirtualAddress();
	m_vertexBufferView_of[key].StrideInBytes = sizeof(Vertex);
	m_vertexBufferView_of[key].SizeInBytes = vertexBufferSize;

	if (textureBuffer_of.find(key) != textureBuffer_of.end() && textureBuffer_of[key] == nullptr)
	{
		wstring texPathW(texture[key].begin(), texture[key].end());

		ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(
			m_device.Get(), m_commandList.Get(), texPathW.c_str(),
			textureBuffer_of[key], textureBufferUploadHeap_of[key]));

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = textureBuffer_of[key]->GetDesc().Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = textureBuffer_of[key]->GetDesc().MipLevels;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

		m_device->CreateShaderResourceView(textureBuffer_of[key].Get(), &srvDesc, m_descriptorHeapHandle);
		m_descriptorHeapHandle.Offset(1, cbvSrvDescriptorSize);
	}
}

// Now we execute the command list to upload the initial assets
m_commandList->Close();
ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

// Create the constant buffer.
{
	ThrowIfFailed(m_device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(1024 * 64),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_constantBuffer)));

	// Initialize and map the constant buffers. We don't unmap this until the
	// app closes. Keeping things mapped for the lifetime of the resource is okay.
	ZeroMemory(&m_constantBufferData, sizeof(m_constantBufferData));

	CD3DX12_RANGE readRange(0, 0);		// We do not intend to read from this resource on the CPU.
	ThrowIfFailed(m_constantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_pCbvDataBegin)));
	memcpy(m_pCbvDataBegin, &m_constantBufferData, sizeof(m_constantBufferData));

}

m_viewport.Width = static_cast<float>(m_width);
m_viewport.Height = static_cast<float>(m_height);
m_viewport.MaxDepth = 1.0f;

m_scissorRect.right = static_cast<float>(m_width);
m_scissorRect.bottom = static_cast<float>(m_height);

// Initialize the world matrixs
g_World = XMMatrixIdentity();

// Initialize the view matrix
Eye = XMVectorSet(0.0f, 2.0, -30.0, 0.0);
At = XMVectorSet(0.0f, 0.0, 1.0, 0.0);
Up = XMVectorSet(0.0f, 1.0, 0.0, 0.0);
g_View = XMMatrixLookAtLH(Eye, At, Up);

// Initialize the projection matrix
g_Projection = XMMatrixPerspectiveFovLH(XM_PIDIV4, 1280 / (FLOAT)720, 0.01f, 1000.0f);

////Tank ve mermi başlangıç setlemesi
camRotationMatrix = XMMatrixRotationRollPitchYaw(0,0,0);
XMVECTOR TankStart = Eye + XMVectorSet(0, 0, 3, 0);
XMVECTOR MissileStart = TankStart +XMVectorSet(0, 1, 2.5f, 0);
//XMVECTOR EnemyMissileStart = XMVectorSet(0, 2, 2.5f, 0);
//XMVECTOR EnemyStart = XMVectorSet(0, 1, 0, 0);
//XMStoreFloat3(&nesneler["Enemy_Missile"].position, EnemyMissileStart);
XMStoreFloat3(&nesneler["Tank"].position, TankStart);
XMStoreFloat3(&nesneler["Missile"].position, MissileStart);
//XMStoreFloat3(&nesneler["Enemy"].position, EnemyStart);

XMMATRIX mTranslate_Tank = XMMatrixTranslation(TankStart.m128_f32[0], TankStart.m128_f32[1], TankStart.m128_f32[2]);
g_World_of["Tank"] = camRotationMatrix * mTranslate_Tank;
XMMATRIX mTranslate_Missile = XMMatrixTranslation(MissileStart.m128_f32[0], MissileStart.m128_f32[1], MissileStart.m128_f32[2]);
g_World_of["Missile"] = camRotationMatrix * mTranslate_Missile;
//XMMATRIX mTranslate_Enemy_Missile = XMMatrixTranslation(EnemyMissileStart.m128_f32[0], EnemyMissileStart.m128_f32[1], EnemyMissileStart.m128_f32[2]);
//g_World_of["Enemy_Missile"] = mTranslate_Enemy_Missile;
//XMMATRIX mTranslate_Enemy = XMMatrixTranslation(EnemyStart.m128_f32[0], EnemyStart.m128_f32[1], EnemyStart.m128_f32[2]);
//g_World_of["Enemy"] = mTranslate_Enemy;

m_constantBufferData.mWorld = XMMatrixTranspose(g_World);
m_constantBufferData.mView = XMMatrixTranspose(g_View);
m_constantBufferData.mProjection = XMMatrixTranspose(g_Projection);

m_constantBufferData.mLightPos = XMFLOAT4(15, 10, 5, 0);
m_constantBufferData.mLightColor = XMFLOAT4(1, 1, 1, 1);
m_constantBufferData.mEyePos = XMFLOAT4(0, 2, -30, 0);

}
void Physics()	
{
	WaitForPreviousFrame();
	Action();									//-> yerçekimi,rüzgar,oyuncu kontrolü için momentum hesabı.												
	intersections = BroadPhase();
	ostringstream oss;
	oss << "Geniş faz eşleşme sayısı: " << intersections.size() << "\n";
	intersections = NarrowPhase(intersections);
	oss << "Dar faz eşleşme sayısı: " << intersections.size() <<", döngü: "<<dongu++ << "\n";
	if(intersections.size()!=0)
	{
		for (size_t i = 0; i < intersections.size(); i++)
		{
			XMFLOAT3 temp;
			XMStoreFloat3(&temp, intersections[i].normal);
			oss << intersections[i].aName << "<->" << intersections[i].bName <<" normal;"<<temp.x<<", "<<temp.y<<", "<<temp.z <<" temas sayısı: "<< intersections[i].temas.size()<< endl;
		}
	}
	OutputDebugStringA(oss.str().c_str());
	Reaction(intersections);					//->kesişime bağlı momentum güncellenmesi
	float deltaTime = CalculateDeltaTime();	//->deltaTime hesaplanması
	Effect(deltaTime);									//->konum güncellenmesi ve constant buffer setlenmesi		
	wostringstream outs;
	if (!isnan(nesneler["Tank"].position.y))
	{
		outs << " Tank konumu = ( " << nesneler["Tank"].position.x << ", " << nesneler["Tank"].position.y << ", " << nesneler["Tank"].position.z << " )";
		SetWindowText(m_hwnd, outs.str().c_str()); 
	}
}
void OnRender()
{
	WaitForPreviousFrame();
	ThrowIfFailed(m_commandAllocator->Reset());
	ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), m_pipelineState_Textured.Get()));
	m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());

	ID3D12DescriptorHeap* ppHeaps[] = { m_descriptorHeap.Get() };
	m_commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	m_commandList->RSSetViewports(1, &m_viewport);
	m_commandList->RSSetScissorRects(1, &m_scissorRect);

	// Indicate that the back buffer will be used as a render target.
	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
	CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
	m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

	const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
	m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	m_commandList->ClearDepthStencilView(m_dsvHeap->GetCPUDescriptorHandleForHeapStart(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	CD3DX12_GPU_DESCRIPTOR_HANDLE Texture(m_descriptorHeap->GetGPUDescriptorHandleForHeapStart());
	const UINT cbvSrvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	string key;
	Object* deger;
	for (const auto& [key,deger] : nesneler)
	{
		if (textureBuffer_of.find(key) != textureBuffer_of.end() && render[key])
		{
			m_commandList->SetPipelineState(m_pipelineState_Textured.Get());

			m_commandList->SetGraphicsRootDescriptorTable(1, Texture);
			m_commandList->SetGraphicsRootConstantBufferView(0, m_constantBuffer->GetGPUVirtualAddress() + sira[key] * 256);
			m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView_of[key]);
			m_commandList->DrawInstanced(nesneler[key].vertexCount, 1, 0, 0);

			Texture.Offset(1, cbvSrvDescriptorSize);
		}
		else if(textureBuffer_of.find(key) != textureBuffer_of.end()&& !render[key]) 
			Texture.Offset(1, cbvSrvDescriptorSize);

		else if(key=="RedDot" && render[key]) 
		{
			m_commandList->SetPipelineState(m_pipelineState_Solid.Get());

			// Render RedDot
			m_commandList->SetGraphicsRootConstantBufferView(0, m_constantBuffer->GetGPUVirtualAddress() + sira[key] * 256);
			m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView_of[key]);
			m_commandList->DrawInstanced(nesneler[key].vertexCount, 1, 0, 0);

		}
		else if (render[key])
		{
			m_commandList->SetPipelineState(m_pipelineState_Phong.Get());
			m_commandList->SetGraphicsRootConstantBufferView(0, m_constantBuffer->GetGPUVirtualAddress() + sira[key] * 256);
			m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView_of[key]);
			m_commandList->DrawInstanced(nesneler[key].vertexCount, 1, 0, 0);
		}
	}
	// Indicate that the back buffer will now be used to present.
	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	ThrowIfFailed(m_commandList->Close());

	// Execute the command list.
	ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	// Present the frame.
	ThrowIfFailed(m_swapChain->Present(1, 0));

	WaitForPreviousFrame();
}
_Use_decl_annotations_
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, int nCmdShow)   //,string objpath,string texpath  parametrelerini sonra ekle
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	InitWindow(hInstance, nCmdShow);

	OnInit(temp_obj_path, temp_tex_path);

	// Main message loop
	MSG msg = { 0 };
	while (WM_QUIT != msg.message)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			Physics();
			OnRender();
		}
	}

	OnDestroy();

	return (int)msg.wParam;
}
	
//void Collision(intersect& contact)
//{
//	Object* A = contact.a;
//	Object* B = contact.b;
//	bool aImmovable = (A->mass == FLT_MAX);
//	bool bImmovable = (B->mass == FLT_MAX);
//	if (aImmovable && bImmovable) return;
//	if (contact.temas.empty()) return;
//
//	constexpr int ITERATION_COUNT = 15;
//	const size_t N = contact.temas.size();
//
//	XMVECTOR n = XMVector3Normalize(contact.normal);
//	XMVECTOR posA = XMLoadFloat3(&A->position);
//	XMVECTOR posB = XMLoadFloat3(&B->position);
//
//	// Temas noktalarının r vektörleri
//	std::vector<XMVECTOR> rA(N), rB(N);
//
//	// İmpuls vektörleri: Normal + 2 Tangent bileşen
//	vector<float> jn(N, 0.0f);
//	vector<XMFLOAT2> jt(N, XMFLOAT2{ 0,0 }); // tangent impulslar (2D)
//
//	// Ön hesaplamalar
//	vector<float> invDenomN(N, 0.0f);
//	vector<float> invDenomT(N, 0.0f);
//	vector<XMVECTOR> tangent1(N);
//	vector<XMVECTOR> tangent2(N);
//
//	// Ortalama sürtünme katsayısı
//	float mu = sqrt(A->dinamikSurt * B->dinamikSurt);
//
//	for (size_t i = 0; i < N; ++i)
//	{
//		XMVECTOR p = XMLoadFloat3(&contact.temas[i]);
//		rA[i] = p - posA;
//		rB[i] = p - posB;
//
//		// Tangent vektörlerini bul
//		// n ile lineer bağımsız iki vektör seç
//		XMVECTOR t1;
//		if (fabs(XMVectorGetX(n)) < 0.577f)
//			t1 = XMVectorSet(1, 0, 0, 0);
//		else
//			t1 = XMVectorSet(0, 1, 0, 0);
//		tangent1[i] = XMVector3Normalize(XMVector3Cross(n, t1));
//		tangent2[i] = XMVector3Normalize(XMVector3Cross(n, tangent1[i]));
//
//		// Ön hesaplama için denominator (normal)
//		float denomN = 0.0f;
//		if (!aImmovable)
//		{
//			XMVECTOR rA_cross_n = XMVector3Cross(rA[i], n);
//			XMVECTOR inertia_term = XMVector3Transform(XMVector3Cross(rA_cross_n, rA[i]), A->inertiaInverse);
//			denomN += 1.0f / A->mass + XMVectorGetX(XMVector3Dot(rA_cross_n, inertia_term));
//		}
//		if (!bImmovable)
//		{
//			XMVECTOR rB_cross_n = XMVector3Cross(rB[i], n);
//			XMVECTOR inertia_term = XMVector3Transform(XMVector3Cross(rB_cross_n, rB[i]), B->inertiaInverse);
//			denomN += 1.0f / B->mass + XMVectorGetX(XMVector3Dot(rB_cross_n, inertia_term));
//		}
//		if (denomN < 1e-5f) denomN = 1e-5f;
//		invDenomN[i] = 1.0f / denomN;
//
//		// Tangent 1 için denominator
//		float denomT1 = 0.0f;
//		if (!aImmovable)
//		{
//			XMVECTOR rA_cross_t1 = XMVector3Cross(rA[i], tangent1[i]);
//			XMVECTOR inertia_term = XMVector3Transform(XMVector3Cross(rA_cross_t1, rA[i]), A->inertiaInverse);
//			denomT1 += 1.0f / A->mass + XMVectorGetX(XMVector3Dot(rA_cross_t1, inertia_term));
//		}
//		if (!bImmovable)
//		{
//			XMVECTOR rB_cross_t1 = XMVector3Cross(rB[i], tangent1[i]);
//			XMVECTOR inertia_term = XMVector3Transform(XMVector3Cross(rB_cross_t1, rB[i]), B->inertiaInverse);
//			denomT1 += 1.0f / B->mass + XMVectorGetX(XMVector3Dot(rB_cross_t1, inertia_term));
//		}
//		if (denomT1 < 1e-5f) denomT1 = 1e-5f;
//
//		// Tangent 2 için denominator
//		float denomT2 = 0.0f;
//		if (!aImmovable)
//		{
//			XMVECTOR rA_cross_t2 = XMVector3Cross(rA[i], tangent2[i]);
//			XMVECTOR inertia_term = XMVector3Transform(XMVector3Cross(rA_cross_t2, rA[i]), A->inertiaInverse);
//			denomT2 += 1.0f / A->mass + XMVectorGetX(XMVector3Dot(rA_cross_t2, inertia_term));
//		}
//		if (!bImmovable)
//		{
//			XMVECTOR rB_cross_t2 = XMVector3Cross(rB[i], tangent2[i]);
//			XMVECTOR inertia_term = XMVector3Transform(XMVector3Cross(rB_cross_t2, rB[i]), B->inertiaInverse);
//			denomT2 += 1.0f / B->mass + XMVectorGetX(XMVector3Dot(rB_cross_t2, inertia_term));
//		}
//		if (denomT2 < 1e-5f) denomT2 = 1e-5f;
//
//		invDenomT[i] = 1.0f / ((denomT1 + denomT2) * 0.5f); // Ortalama alındı (tahmini)
//	}
//
//	// PGS iterasyonu
//	for (int iter = 0; iter < ITERATION_COUNT; ++iter)
//	{
//		for (size_t i = 0; i < N; ++i)
//		{
//			// Güncel hızları hesapla
//			XMVECTOR velA = XMLoadFloat3(&A->momentum) / A->mass;
//			XMVECTOR velB = XMLoadFloat3(&B->momentum) / B->mass;
//
//			XMVECTOR angVelA = XMVector3Cross(A->angular, rA[i]);
//			XMVECTOR angVelB = XMVector3Cross(B->angular, rB[i]);
//
//			XMVECTOR totalVA = velA + angVelA;
//			XMVECTOR totalVB = velB + angVelB;
//
//			XMVECTOR relVel = totalVB - totalVA;
//
//			// Normal impuls hesapla
//			float vRelN = XMVectorGetX(XMVector3Dot(relVel, n));
//			float restitution = 1.0f;
//
//			float jn_old = jn[i];
//			float jn_new = -(1.0f + restitution) * vRelN * invDenomN[i];
//
//			// Pozitif projeksiyon (yalnızca iteratif artış)
//			jn[i] = max(0.0f, jn_old + jn_new);
//			float deltaJn = jn[i] - jn_old;
//
//			XMVECTOR impulseN = deltaJn * n;
//
//			// Momentum güncelle
//			if (!aImmovable)
//			{
//				XMStoreFloat3(&A->momentum, XMLoadFloat3(&A->momentum) + impulseN);
//				A->angular += XMVector3Transform(XMVector3Cross(rA[i], impulseN), A->inertiaInverse);
//			}
//			if (!bImmovable)
//			{
//				XMStoreFloat3(&B->momentum, XMLoadFloat3(&B->momentum) - impulseN);
//				B->angular -= XMVector3Transform(XMVector3Cross(rB[i], impulseN), B->inertiaInverse);
//			}
//
//			// Tangent impulslar
//			// İki tangent ekseni üzerinden sürtünme kuvvetini uygula
//
//			XMVECTOR vRelT = relVel - vRelN * n;
//			float vRelT_len = XMVectorGetX(XMVector3Length(vRelT));
//			if (vRelT_len > 1e-6f)
//			{
//				XMVECTOR t1 = tangent1[i];
//				XMVECTOR t2 = tangent2[i];
//
//				float jt1_old = jt[i].x;
//				float jt2_old = jt[i].y;
//
//				float vRelT_t1 = XMVectorGetX(XMVector3Dot(vRelT, t1));
//				float vRelT_t2 = XMVectorGetX(XMVector3Dot(vRelT, t2));
//
//				// Yeni tangent impulsları (yaklaşık viscous sürtünme tarzı)
//				float jt1_new = -vRelT_t1 * invDenomT[i];
//				float jt2_new = -vRelT_t2 * invDenomT[i];
//
//				jt[i].x = jt1_old + jt1_new;
//				jt[i].y = jt2_old + jt2_new;
//
//				// Coulomb sınırı (|jt| <= mu * jn)
//				float jt_len = sqrt(jt[i].x * jt[i].x + jt[i].y * jt[i].y);
//				float maxFriction = mu * jn[i];
//				if (jt_len > maxFriction)
//				{
//					float scale = maxFriction / jt_len;
//					jt[i].x *= scale;
//					jt[i].y *= scale;
//				}
//
//				float deltaJt1 = jt[i].x - jt1_old;
//				float deltaJt2 = jt[i].y - jt2_old;
//
//				XMVECTOR frictionImpulse = deltaJt1 * t1 + deltaJt2 * t2;
//
//				if (!aImmovable)
//				{
//					XMStoreFloat3(&A->momentum, XMLoadFloat3(&A->momentum) + frictionImpulse);
//					A->angular += XMVector3Transform(XMVector3Cross(rA[i], frictionImpulse), A->inertiaInverse);
//				}
//				if (!bImmovable)
//				{
//					XMStoreFloat3(&B->momentum, XMLoadFloat3(&B->momentum) - frictionImpulse);
//					B->angular -= XMVector3Transform(XMVector3Cross(rB[i], frictionImpulse), B->inertiaInverse);
//				}
//			}
//		}
//	}
//
//	// Penetrasyon düzeltme (ortalama penetrasyon mesafesi üzerinden)
//	float minPenetration = FLT_MAX;
//	for (const XMFLOAT3& pt : contact.temas)
//	{
//		float dist = XMVectorGetX(XMVector3Dot(XMLoadFloat3(&pt) - posA, n));
//		minPenetration = min(minPenetration, dist);
//	}
//	float penetration = max(0.0f, -minPenetration) * 0.2f;
//	XMVECTOR correction = penetration * n;
//
//	XMStoreFloat3(&A->position, posA + correction * (B->mass / (A->mass + B->mass)));
//	XMStoreFloat3(&B->position, posB - correction * (A->mass / (A->mass + B->mass)));
//
//	// Ortalama normal impuls
//	float totalJn = 0.0f;
//	for (float val : jn) totalJn += val;
//	contact.Impulse = totalJn / std::max<size_t>(1, N);
//}
//void Contact(intersect& contact)
//{
//	Object* A = contact.a;
//	Object* B = contact.b;
//
//	// Temas yüzeyi normalini normalize et
//	XMVECTOR n = XMVector3Normalize(contact.normal);
//
//	float minPenetration = FLT_MAX;
//
//	bool aImmovable = (A->mass == FLT_MAX);
//	bool bImmovable = (B->mass == FLT_MAX);
//	if (aImmovable && bImmovable) return;
//
//	// Özel isim bazlı durumlar (oyun mantığı)
//	if (contact.aName == "RedDot" || contact.bName == "RedDot") return;
//
//	// Burada oyun içi özel durumlar var, onları orijinal gibi bırakıyorum
//	if (contact.aName == "Missile" || contact.bName == "Missile")
//	{
//		render["Missile"] = false;
//		if (contact.bName == "Enemy" || contact.aName == "Enemy")
//		{
//			render["Enemy"] = false;
//			if (FireEnemyMissile) render["Enemy_Missile"] = false;
//			if (PlayTankHitSoundOnce)
//			{
//				PlaySound(TEXT("hit.wav"), NULL, SND_FILENAME | SND_ASYNC);
//				PlayTankHitSoundOnce = false;
//			}
//			return;
//		}
//		if (contact.bName == "Enemy_Missile" || contact.aName == "Enemy_Missile")
//		{
//			render["Enemy_Missile"] = false;
//			if (PlayTankHitSoundOnce)
//			{
//				PlaySound(TEXT("hit.wav"), NULL, SND_FILENAME | SND_ASYNC);
//				PlayTankHitSoundOnce = false;
//			}
//			return;
//		}
//		if (aImmovable || bImmovable || contact.aName == "Tank" || contact.bName == "Tank") return;
//	}
//	if (contact.aName == "Enemy_Missile" || contact.bName == "Enemy_Missile")
//	{
//		render["Enemy_Missile"] = false;
//		if (contact.aName == "Tank" || contact.bName == "Tank")
//		{
//			render["Tank"] = false;
//			if (FireTankMissile) render["Missile"] = false;
//			if (PlayTankHitSoundOnce)
//			{
//				PlaySound(TEXT("hit.wav"), NULL, SND_FILENAME | SND_ASYNC);
//				PlayTankHitSoundOnce = false;
//			}
//			return;
//		}
//		if (contact.bName == "Missile" || contact.aName == "Missile")
//		{
//			render["Missile"] = false;
//			if (PlayTankHitSoundOnce)
//			{
//				PlaySound(TEXT("hit.wav"), NULL, SND_FILENAME | SND_ASYNC);
//				PlayTankHitSoundOnce = false;
//			}
//			return;
//		}
//		if (aImmovable || bImmovable || contact.bName == "Enemy" || contact.aName == "Enemy") return;
//	}
//
//	// Temas noktaları başına işlemler
//	for (const XMFLOAT3& pt : contact.temas)
//	{
//		XMVECTOR contactPoint = XMLoadFloat3(&pt);
//
//		// Temas noktasına göre pozisyonlar
//		XMVECTOR rA = contactPoint - XMLoadFloat3(&A->position);
//		XMVECTOR rB = contactPoint - XMLoadFloat3(&B->position);
//
//		// Hızlar (momentum / kütle)
//		XMVECTOR vA = XMLoadFloat3(&A->momentum) / A->mass;
//		XMVECTOR vB = XMLoadFloat3(&B->momentum) / B->mass;
//
//		// Açısal hızdan temas noktasındaki hız
//		XMVECTOR angularA = A->angular;
//		XMVECTOR angularB = B->angular;
//		XMVECTOR vAngularA = XMVector3Cross(angularA, rA);
//		XMVECTOR vAngularB = XMVector3Cross(angularB, rB);
//
//		XMVECTOR vTotalA = vA + vAngularA;
//		XMVECTOR vTotalB = vB + vAngularB;
//
//		// Göreceli hız
//		XMVECTOR vRel = vTotalB - vTotalA;
//
//		float vRelN = XMVectorGetX(XMVector3Dot(vRel, n));
//		if (vRelN >= 0) continue; // Ayrılma hareketi varsa atla
//
//		// Normal impuls için katsayılar
//		XMVECTOR rA_cross_n = XMVector3Cross(rA, n);
//		XMVECTOR rB_cross_n = XMVector3Cross(rB, n);
//
//		XMVECTOR termA = XMVector3Transform(XMVector3Cross(rA_cross_n, rA), A->inertiaInverse);
//		XMVECTOR termB = XMVector3Transform(XMVector3Cross(rB_cross_n, rB), B->inertiaInverse);
//
//		float denom = (1.0f / A->mass) + (1.0f / B->mass)
//			+ XMVectorGetX(XMVector3Dot(rA_cross_n, termA))
//			+ XMVectorGetX(XMVector3Dot(rB_cross_n, termB));
//
//		if (denom < 1e-6f) denom = 1e-6f;
//
//		float restitution = 0.5f; // İstersen objeden de çekebilirsin
//
//		// Normal impuls büyüklüğü
//		float j = -(1.0f + restitution) * vRelN / denom;
//
//		// Normal impuls vektörü
//		XMVECTOR impulseN = j * n;
//
//		// Normal impulsu uygula
//		XMStoreFloat3(&A->momentum, XMLoadFloat3(&A->momentum) + impulseN);
//		XMStoreFloat3(&B->momentum, XMLoadFloat3(&B->momentum) - impulseN);
//
//		angularA = *(&A->angular) + XMVector3Transform(XMVector3Cross(rA, impulseN), A->inertiaInverse);
//		angularB = *(&B->angular)- XMVector3Transform(XMVector3Cross(rB, impulseN), B->inertiaInverse);
//
//		A->angular=angularA;
//		B->angular=angularB;
//
//		// --- Sürtünme hesaplama ---
//		XMVECTOR vRelTangent = vRel - vRelN * n;
//		float vRelTangentLen = XMVectorGetX(XMVector3Length(vRelTangent));
//
//		if (vRelTangentLen > 1e-6f)
//		{
//			XMVECTOR tangent = XMVector3Normalize(vRelTangent);
//
//			float mu_static = sqrtf(A->statikSurt * B->statikSurt);
//			float mu_dynamic = sqrtf(A->dinamikSurt * B->dinamikSurt);
//
//			// Statik sürtünme limit impulsu
//			float maxStaticImpulse = mu_static * j;
//
//			XMVECTOR frictionImpulse;
//
//			// Küçük hızsa statik sürtünme dene
//			if (vRelTangentLen < 0.01f)
//			{
//				frictionImpulse = -vRelTangent / denom; // Göreceli hareketi sıfırla
//
//				float frictionImpulseLen = XMVectorGetX(XMVector3Length(frictionImpulse));
//				if (frictionImpulseLen > maxStaticImpulse)
//				{
//					// Statik sürtünme sınırı aşıldı, dinamik sürtünmeye geç
//					frictionImpulse = -mu_dynamic * j * tangent;
//				}
//			}
//			else
//			{
//				// Dinamik sürtünme
//				frictionImpulse = -mu_dynamic * j * tangent;
//			}
//
//			// Sürtünme impulsu uygula
//			XMStoreFloat3(&A->momentum, XMLoadFloat3(&A->momentum) + frictionImpulse);
//			XMStoreFloat3(&B->momentum, XMLoadFloat3(&B->momentum) - frictionImpulse);
//
//			angularA = A->angular + XMVector3Transform(XMVector3Cross(rA, frictionImpulse), A->inertiaInverse);
//			angularB = B->angular - XMVector3Transform(XMVector3Cross(rB, frictionImpulse), B->inertiaInverse);
//
//			A->angular=angularA;
//			B->angular=angularB;
//		}
//
//		// Penetrasyon mesafesi (min arama)
//		float distance = XMVectorGetX(XMVector3Dot(contactPoint - XMLoadFloat3(&A->position), n));
//		if (distance < minPenetration)
//			minPenetration = distance;
//	}
//
//	// Penetrasyon düzeltmesi
//	float penetration = max(0.0f, -minPenetration + 0.001f);
//	XMVECTOR correction = penetration * n;
//
//	XMStoreFloat3(&A->position, XMLoadFloat3(&A->position) + correction * (B->mass / (A->mass + B->mass)));
//	XMStoreFloat3(&B->position, XMLoadFloat3(&B->position) - correction * (A->mass / (A->mass + B->mass)));
//}


//void Contact(intersect& contact)
//{
//	Object* A = contact.a;
//	Object* B = contact.b;
//	XMVECTOR n = XMVector3Normalize(contact.normal);
//	float minPenetration = FLT_MAX;
//	bool aImmovable = (A->mass == FLT_MAX);
//	bool bImmovable = (B->mass == FLT_MAX);
//	if (aImmovable && bImmovable) return;
//	if (contact.aName == "RedDot" || contact.bName == "RedDot") return;
//	if (contact.aName == "Missile" || contact.bName == "Missile")
//	{
//		render["Missile"] = false;
//		if (contact.bName == "Enemy" || contact.aName == "Enemy")
//		{
//			render["Enemy"] = false;
//			if (FireEnemyMissile) render["Enemy_Missile"] = false;
//			if (PlayTankHitSoundOnce)
//			{
//				PlaySound(TEXT("hit.wav"), NULL, SND_FILENAME | SND_ASYNC); PlayTankHitSoundOnce = false;
//			}
//			return;
//		}
//		if (contact.bName == "Enemy_Missile" || contact.aName == "Enemy_Missile")
//		{
//			render["Enemy_Missile"] = false;
//			if (PlayTankHitSoundOnce)
//			{
//				PlaySound(TEXT("hit.wav"), NULL, SND_FILENAME | SND_ASYNC); PlayTankHitSoundOnce = false;
//			}
//			return;
//		}
//		if (aImmovable || bImmovable || contact.aName == "Tank" || contact.bName == "Tank") return;
//	}
//	if (contact.aName == "Enemy_Missile" || contact.bName == "Enemy_Missile")
//	{
//		render["Enemy_Missile"] = false;
//		if (contact.aName == "Tank" || contact.bName == "Tank")
//		{
//			render["Tank"] = false;
//			if (FireTankMissile) render["Missile"] = false;
//			if (PlayTankHitSoundOnce)
//			{
//				PlaySound(TEXT("hit.wav"), NULL, SND_FILENAME | SND_ASYNC); PlayTankHitSoundOnce = false;
//			}
//			return;
//		}
//		if (contact.bName == "Missile" || contact.aName == "Missile")
//		{
//			render["Missile"] = false;
//			if (PlayTankHitSoundOnce)
//			{
//				PlaySound(TEXT("hit.wav"), NULL, SND_FILENAME | SND_ASYNC); PlayTankHitSoundOnce = false;
//			}
//			return;
//		}
//		if (aImmovable || bImmovable || contact.bName == "Enemy" || contact.aName == "Enemy") return;
//	}
//
//	if (contact.temas.empty()) return;
//
//	for (const XMFLOAT3& pt : contact.temas)
//	{
//		XMVECTOR contactPoint = XMLoadFloat3(&pt);
//
//		XMVECTOR rA = contactPoint - XMLoadFloat3(&A->position);
//		XMVECTOR rB = contactPoint - XMLoadFloat3(&B->position);
//
//		XMVECTOR vA = XMLoadFloat3(&A->momentum) / A->mass;
//		XMVECTOR vB = XMLoadFloat3(&B->momentum) / B->mass;
//
//		XMVECTOR vAngularA = XMVector3Cross(A->angular, rA);
//		XMVECTOR vAngularB = XMVector3Cross(B->angular, rB);
//
//		XMVECTOR vTotalA = vA + vAngularA;
//		XMVECTOR vTotalB = vB + vAngularB;
//
//		XMVECTOR vRel = vTotalB - vTotalA;
//		XMVECTOR vRelTangent = vRel - XMVector3Dot(vRel, n) * n;
//
//		float vRelTangentLen = XMVectorGetX(XMVector3Length(vRelTangent));
//		if (vRelTangentLen < 1e-6f)
//			continue;
//
//		XMVECTOR tangent = XMVector3Normalize(vRelTangent);
//
//		float mu_static = sqrtf(A->statikSurt * B->statikSurt);
//		float mu_dynamic = sqrtf(A->dinamikSurt * B->dinamikSurt);
//
//		float normalImpulseMag = contact.Impulse;
//		float frictionThreshold = mu_static * normalImpulseMag;
//
//		if (vRelTangentLen < frictionThreshold)
//		{
//			XMVECTOR impulse = -vRelTangent * A->mass;
//
//			XMStoreFloat3(&A->momentum, XMLoadFloat3(&A->momentum) + impulse);
//			XMStoreFloat3(&B->momentum, XMLoadFloat3(&B->momentum) - impulse);
//
//			A->angular += XMVector3Transform(XMVector3Cross(rA, impulse), A->inertiaInverse);
//			B->angular -= XMVector3Transform(XMVector3Cross(rB, impulse), B->inertiaInverse);
//		}
//		else
//		{
//			XMVECTOR frictionImpulse = -mu_dynamic * normalImpulseMag * tangent;
//
//			XMStoreFloat3(&A->momentum, XMLoadFloat3(&A->momentum) + frictionImpulse);
//			XMStoreFloat3(&B->momentum, XMLoadFloat3(&B->momentum) - frictionImpulse);
//
//			A->angular += XMVector3Transform(XMVector3Cross(rA, frictionImpulse), A->inertiaInverse);
//			B->angular -= XMVector3Transform(XMVector3Cross(rB, frictionImpulse), B->inertiaInverse);
//		}
//		float distance = XMVectorGetX(XMVector3Dot(XMLoadFloat3(&pt) - XMLoadFloat3(&A->position), contact.normal));
//		minPenetration = min(minPenetration, distance);
//	}
//	float penetration = max(0.0f, -minPenetration + 0.001f);
//	XMVECTOR correction = penetration * contact.normal;
//	if (!aImmovable&&!bImmovable)
//	{
//		XMStoreFloat3(&A->position, XMLoadFloat3(&A->position) + correction * (B->mass / (A->mass + B->mass)));
//		XMStoreFloat3(&B->position, XMLoadFloat3(&B->position) - correction * (A->mass / (A->mass + B->mass)));
//	}
//	else if (!aImmovable)
//	{
//		XMStoreFloat3(&A->position, XMLoadFloat3(&A->position) + correction);
//	}
//	else if (!bImmovable)
//	{
//		XMStoreFloat3(&B->position, XMLoadFloat3(&B->position) - correction);
//	}
//}

//void Effect(float deltaTime)
//{
//	rotation += 0.01;
//	XMMATRIX mRotate = XMMatrixRotationY(rotation);
//	XMMATRIX mTranslate = XMMatrixTranslation(-50.0f, 100.0f, 0.0f);
//	XMVECTOR xmvLightPos = XMVectorSet(0, 0, 0, 0);
//	xmvLightPos = XMVector3Transform(xmvLightPos, mTranslate);
//	xmvLightPos = XMVector3Transform(xmvLightPos, mRotate);
//	XMStoreFloat4(&m_constantBufferData.mLightPos, xmvLightPos);
//
//	for (auto& [key, deger] : nesneler)
//	{
//		if(render[key])
//		{
//			if (key == "Missile" && FireTankMissile)
//			{
//				XMFLOAT3 vTank = DivideFloat3(nesneler["Tank"].momentum, nesneler["Tank"].mass);
//				XMVECTOR wTank = XMVector3Transform(nesneler["Tank"].angular, nesneler["Tank"].inertiaInverse);
//				XMFLOAT3 tempF = SubtractFloat3(deger.position, nesneler["Tank"].position);
//				XMVECTOR MissileToTank = XMLoadFloat3(&tempF);
//				XMVECTOR tempVec = XMVector3Cross(wTank, MissileToTank);
//				XMStoreFloat3(&tempF, tempVec);
//				tempF = AddFloat3(vTank, tempF);
//				tempVec = XMLoadFloat3(&tempF);
//				XMVECTOR tempMomentum = XMVectorScale(tempVec, nesneler["Missile"].mass);
//				XMStoreFloat3(&tempF, tempMomentum);
//				deger.momentum = tempF;
//				deger.angular = XMVector3Cross(MissileToTank, tempMomentum);
//			}
//			if (key == "Enemy_Missile" && FireEnemyMissile)
//			{
//				XMFLOAT3 vTank = DivideFloat3(nesneler["Enemy"].momentum, nesneler["Enemy"].mass);
//				XMVECTOR wTank = XMVector3Transform(nesneler["Enemy"].angular, nesneler["Enemy"].inertiaInverse);
//				XMFLOAT3 tempF = SubtractFloat3(deger.position, nesneler["Enemy"].position);
//				XMVECTOR MissileToTank = XMLoadFloat3(&tempF);
//				XMVECTOR tempVec = XMVector3Cross(wTank, MissileToTank);
//				XMStoreFloat3(&tempF, tempVec);
//				tempF = AddFloat3(vTank, tempF);
//				tempVec = XMLoadFloat3(&tempF);
//				XMVECTOR tempMomentum = XMVectorScale(tempVec, deger.mass);
//				XMStoreFloat3(&tempF, tempMomentum);
//				deger.momentum = tempF;
//				deger.angular = XMVector3Cross(MissileToTank, tempMomentum);
//			}
//			XMVECTOR v = XMLoadFloat3(&deger.momentum);
//			if (XMVectorGetX(XMVector3Length(v)) <= 1e-6f && XMVectorGetX(XMVector3Length(deger.angular)) <= 1e-6f) { moved[key] = false; }
//			else { moved[key] = true; }
//
//			if (deger.mass != FLT_MAX && moved[key])
//			{
//				//sabit cisimlere hiç bir şey olmuyor zaten.
//				//hareketli cisimlerin world matrisleri ve konumları burada güncellensin.
//				deger.momentum = AddFloat3(deger.momentum, MultiplyFloat3(deger.kuvvet, deltaTime));
//				XMVECTOR linearVelocity = XMVectorScale(XMLoadFloat3(&deger.momentum), 1.0f / deger.mass);
//				XMVECTOR position = XMLoadFloat3(&deger.position);
//				position = XMVectorAdd(position, linearVelocity);
//				XMStoreFloat3(&deger.position, position);
//
//				// 2. Açısal hız = açısal momentum * inverse inertia (tam vektör)
//				XMVECTOR angularVelocity = XMVector3Transform(deger.angular, deger.inertiaInverse);
//
//				// 3. Açısal hız vektörünü quaternion'a çevirip çarp (küçük açılarda geçerli)
//				float angle = XMVectorGetX(XMVector3Length(angularVelocity)); // dönme açısı
//				if (angle > 1e-6f)
//				{
//					XMVECTOR axis = XMVector3Normalize(angularVelocity);
//					XMVECTOR qRotation = XMQuaternionRotationAxis(axis, angle);
//
//					// World matris oluştur
//					XMMATRIX mRotate = XMMatrixRotationQuaternion(qRotation);
//					XMMATRIX mTranslate = XMMatrixTranslationFromVector(position);
//					g_World_of[key] = mRotate * mTranslate;
//					m_constantBufferData.mWorld = XMMatrixTranspose(g_World_of[key]);
//				}
//				else
//				{
//					g_World_of[key] = XMMatrixTranslationFromVector(position);
//					m_constantBufferData.mWorld = XMMatrixTranspose(g_World_of[key]);
//				}
//				if (key == "Tank")
//				{
//					// TANKIN KONUM VE YÖNELİMİNE GÖRE EYE AT GÜNCELLENECEK   FAREYE GÖRE UP GÜNCELLENECEK FARE DURDUĞU ANDA AÇISAL MOMENTUM 0 OLACAK
//					XMMATRIX pitchMatrix = XMMatrixRotationX(camPitch);
//					XMVECTOR cameraOffset = XMVectorSet(0.0f, 2.0f, -3.0f, 0.0f);
//					cameraOffset = XMVector3TransformCoord(cameraOffset, pitchMatrix);
//					if (angle > 1e-6f)
//					{
//						cameraOffset = XMVector3TransformCoord(cameraOffset, XMMatrixRotationQuaternion(XMQuaternionRotationAxis(XMVector3Normalize(angularVelocity), angle)));
//					}
//
//					Eye = position + cameraOffset;
//					At = position + XMVectorSet(0, 2, 0, 0);
//					Up = XMVector3TransformCoord(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), pitchMatrix);
//					g_View = XMMatrixLookAtLH(Eye, At, Up);
//					m_constantBufferData.mView = XMMatrixTranspose(g_View);
//				}
//			}
//			else
//			{
//				m_constantBufferData.mWorld = XMMatrixTranspose(XMMatrixIdentity());
//			}
//			m_constantBufferData.mMeshColor = XMFLOAT4(0, 1, 0, 1);
//			memcpy(m_pCbvDataBegin + sira[key] * 256, &m_constantBufferData, sizeof(m_constantBufferData));
//		}
//	}
//}
//SİMÜLASYON

//void Effect(float deltaTime)
//{
//	rotation += 0.01f;
//	XMMATRIX mRotate = XMMatrixRotationY(rotation);
//	XMMATRIX mTranslate = XMMatrixTranslation(-50.0f, 100.0f, 0.0f);
//	XMVECTOR xmvLightPos = XMVectorSet(0, 0, 0, 0);
//	xmvLightPos = XMVector3Transform(xmvLightPos, mTranslate);
//	xmvLightPos = XMVector3Transform(xmvLightPos, mRotate);
//	XMStoreFloat4(&m_constantBufferData.mLightPos, xmvLightPos);
//
//	for (auto& [key, deger] : nesneler)
//	{
//		if (render[key])
//		{
//			// Missile momentumlarını tank hareketine göre ayarla
//			if (key == "Missile" && FireTankMissile)
//			{
//				XMFLOAT3 vTank = DivideFloat3(nesneler["Tank"].momentum, nesneler["Tank"].mass);
//				XMVECTOR wTank = XMVector3Transform(nesneler["Tank"].angular, nesneler["Tank"].inertiaInverse);
//				XMFLOAT3 tempF = SubtractFloat3(deger.position, nesneler["Tank"].position);
//				XMVECTOR MissileToTank = XMLoadFloat3(&tempF);
//				XMVECTOR tempVec = XMVector3Cross(wTank, MissileToTank);
//				XMStoreFloat3(&tempF, tempVec);
//				tempF = AddFloat3(vTank, tempF);
//				tempVec = XMLoadFloat3(&tempF);
//				XMVECTOR tempMomentum = XMVectorScale(tempVec, deger.mass);
//				XMStoreFloat3(&tempF, tempMomentum);
//				deger.momentum = tempF;
//				deger.angular = XMVector3Cross(MissileToTank, tempMomentum);
//			}
//			else if (key == "Enemy_Missile" && FireEnemyMissile)
//			{
//				XMFLOAT3 vEnemy = DivideFloat3(nesneler["Enemy"].momentum, nesneler["Enemy"].mass);
//				XMVECTOR wEnemy = XMVector3Transform(nesneler["Enemy"].angular, nesneler["Enemy"].inertiaInverse);
//				XMFLOAT3 tempF = SubtractFloat3(deger.position, nesneler["Enemy"].position);
//				XMVECTOR MissileToEnemy = XMLoadFloat3(&tempF);
//				XMVECTOR tempVec = XMVector3Cross(wEnemy, MissileToEnemy);
//				XMStoreFloat3(&tempF, tempVec);
//				tempF = AddFloat3(vEnemy, tempF);
//				tempVec = XMLoadFloat3(&tempF);
//				XMVECTOR tempMomentum = XMVectorScale(tempVec, deger.mass);
//				XMStoreFloat3(&tempF, tempMomentum);
//				deger.momentum = tempF;
//				deger.angular = XMVector3Cross(MissileToEnemy, tempMomentum);
//			}
//
//			XMVECTOR v = XMLoadFloat3(&deger.momentum);
//			if (XMVectorGetX(XMVector3Length(v)) <= 1e-6f && XMVectorGetX(XMVector3Length(deger.angular)) <= 1e-6f)
//			{
//				moved[key] = false;
//			}
//			else
//			{
//				moved[key] = true;
//			}
//
//			if (deger.mass != FLT_MAX && moved[key])
//			{
//				// 1. Momentumları kuvvet ve tork kullanarak güncelle
//				deger.momentum = AddFloat3(deger.momentum, MultiplyFloat3(deger.kuvvet, deltaTime));
//				deger.angular = XMVectorAdd(deger.angular, XMVectorScale(deger.tork, deltaTime));
//
//				// Kuvvet ve tork sıfırlanıyor (birikmesin diye)
//				deger.kuvvet = XMFLOAT3(0.0f, 0.0f, 0.0f);
//				deger.tork = XMVectorZero();
//
//				// 2. Hızları momentumdan hesapla
//				XMVECTOR linearVelocity = XMVectorScale(XMLoadFloat3(&deger.momentum), 1.0f / deger.mass);
//
//				// 3. Konumu hızla güncelle
//				XMVECTOR position = XMLoadFloat3(&deger.position);
//				position = XMVectorAdd(position, XMVectorScale(linearVelocity, deltaTime));
//				XMStoreFloat3(&deger.position, position);
//
//				// 4. Açısal hız = açısal momentum * inverse inertia
//				XMVECTOR angularVelocity = XMVector3Transform(deger.angular, deger.inertiaInverse);
//
//				// 5. Açısal hız vektörünü quaternion'a çevir
//				float angle = XMVectorGetX(XMVector3Length(angularVelocity));
//				if (angle > 1e-6f)
//				{
//					XMVECTOR axis = XMVector3Normalize(angularVelocity);
//					XMVECTOR qRotation = XMQuaternionRotationAxis(axis, angle);
//
//					// World matrisi oluştur
//					XMMATRIX mRotate = XMMatrixRotationQuaternion(qRotation);
//					XMMATRIX mTranslate = XMMatrixTranslationFromVector(position);
//					g_World_of[key] = mRotate * mTranslate;
//					m_constantBufferData.mWorld = XMMatrixTranspose(g_World_of[key]);
//				}
//				else
//				{
//					g_World_of[key] = XMMatrixTranslationFromVector(position);
//					m_constantBufferData.mWorld = XMMatrixTranspose(g_World_of[key]);
//				}
//
//				// Tank için kamera kontrolü
//				if (key == "Tank")
//				{
//					// Kamera açısı yaw ile tankın etrafında dönsün (örn: rotationYaw)
//					static float rotationYaw = 0.0f;
//					// Örnek: rotationYaw += fare hareketinden gelen delta (burada manuel arttırıyoruz, sende fare kontrol ekle)
//					// rotationYaw += deltaMouseX * sensitivity;
//
//					// Tank pozisyonu
//					XMVECTOR tankPos = position;
//
//					// Kamera offset'i: biraz yukarı ve geride
//					XMVECTOR cameraOffset = XMVectorSet(0.0f, 2.0f, -3.0f, 0.0f);
//
//					// Yaw dönüşüyle kamerayı tank etrafında döndür
//					XMMATRIX yawRotation = XMMatrixRotationY(rotationYaw);
//					cameraOffset = XMVector3TransformCoord(cameraOffset, yawRotation);
//
//					Eye = tankPos + cameraOffset;
//					At = tankPos + XMVectorSet(0, 1.5f, 0, 0);
//					Up = XMVectorSet(0, 1, 0, 0);
//
//					g_View = XMMatrixLookAtLH(Eye, At, Up);
//					m_constantBufferData.mView = XMMatrixTranspose(g_View);
//				}
//			}
//			else
//			{
//				m_constantBufferData.mWorld = XMMatrixTranspose(XMMatrixIdentity());
//			}
//
//			m_constantBufferData.mMeshColor = XMFLOAT4(0, 1, 0, 1);
//			memcpy(m_pCbvDataBegin + sira[key] * 256, &m_constantBufferData, sizeof(m_constantBufferData));
//		}
//	}
//}

//void Effect(float deltaTime)
//{
//	rotation += 0.01f;
//	XMMATRIX mRotate = XMMatrixRotationY(rotation);
//	XMMATRIX mTranslate = XMMatrixTranslation(-50.0f, 100.0f, 0.0f);
//	XMVECTOR xmvLightPos = XMVectorSet(0, 0, 0, 0);
//	xmvLightPos = XMVector3Transform(xmvLightPos, mTranslate);
//	xmvLightPos = XMVector3Transform(xmvLightPos, mRotate);
//	XMStoreFloat4(&m_constantBufferData.mLightPos, xmvLightPos);
//
//	for (auto& [key, deger] : nesneler)
//	{
//		if (render[key])
//		{
//			if (key == "Missile" && FireTankMissile)
//			{
//				XMFLOAT3 vTank = DivideFloat3(nesneler["Tank"].momentum, nesneler["Tank"].mass);
//				XMVECTOR wTank = XMVector3Transform(nesneler["Tank"].angular, nesneler["Tank"].inertiaInverse);
//				XMFLOAT3 tempF = SubtractFloat3(deger.position, nesneler["Tank"].position);
//				XMVECTOR MissileToTank = XMLoadFloat3(&tempF);
//				XMVECTOR tempVec = XMVector3Cross(wTank, MissileToTank);
//				XMStoreFloat3(&tempF, tempVec);
//				tempF = AddFloat3(vTank, tempF);
//				tempVec = XMLoadFloat3(&tempF);
//				XMVECTOR tempMomentum = XMVectorScale(tempVec, nesneler["Missile"].mass);
//				XMStoreFloat3(&tempF, tempMomentum);
//				deger.momentum = tempF;
//				deger.angular = XMVector3Cross(MissileToTank, tempMomentum);
//			}
//			if (key == "Enemy_Missile" && FireEnemyMissile)
//			{
//				XMFLOAT3 vTank = DivideFloat3(nesneler["Enemy"].momentum, nesneler["Enemy"].mass);
//				XMVECTOR wTank = XMVector3Transform(nesneler["Enemy"].angular, nesneler["Enemy"].inertiaInverse);
//				XMFLOAT3 tempF = SubtractFloat3(deger.position, nesneler["Enemy"].position);
//				XMVECTOR MissileToTank = XMLoadFloat3(&tempF);
//				XMVECTOR tempVec = XMVector3Cross(wTank, MissileToTank);
//				XMStoreFloat3(&tempF, tempVec);
//				tempF = AddFloat3(vTank, tempF);
//				tempVec = XMLoadFloat3(&tempF);
//				XMVECTOR tempMomentum = XMVectorScale(tempVec, deger.mass);
//				XMStoreFloat3(&tempF, tempMomentum);
//				deger.momentum = tempF;
//				deger.angular = XMVector3Cross(MissileToTank, tempMomentum);
//			}
//
//			XMVECTOR v = XMLoadFloat3(&deger.momentum);
//			if (XMVectorGetX(XMVector3Length(v)) <= 1e-6f && XMVectorGetX(XMVector3Length(deger.angular)) <= 1e-6f)
//			{
//				moved[key] = false;
//			}
//			else
//			{
//				moved[key] = true;
//			}
//
//			if (deger.mass != FLT_MAX && moved[key])
//			{
//				// 💡 EKLENDİ: Tork ve kuvvet uygulaması (momentum güncelleme)
//				deger.momentum = AddFloat3(deger.momentum, MultiplyFloat3(deger.kuvvet, deltaTime));
//				deger.angular = XMVectorAdd(deger.angular, XMVectorScale(deger.tork, deltaTime));
//				deger.kuvvet = XMFLOAT3(0.0f, 0.0f, 0.0f);
//				deger.tork = XMVectorZero();
//
//				XMVECTOR linearVelocity = XMVectorScale(XMLoadFloat3(&deger.momentum), 1.0f / deger.mass);
//				XMVECTOR position = XMLoadFloat3(&deger.position);
//				position = XMVectorAdd(position, linearVelocity);
//				XMStoreFloat3(&deger.position, position);
//
//				XMVECTOR angularVelocity = XMVector3Transform(deger.angular, deger.inertiaInverse);
//				float angle = XMVectorGetX(XMVector3Length(angularVelocity));
//
//				if (angle > 1e-6f)
//				{
//					XMVECTOR axis = XMVector3Normalize(angularVelocity);
//					XMVECTOR qRotation = XMQuaternionRotationAxis(axis, angle);
//					XMMATRIX mRotate = XMMatrixRotationQuaternion(qRotation);
//					XMMATRIX mTranslate = XMMatrixTranslationFromVector(position);
//					g_World_of[key] = mRotate * mTranslate;
//					m_constantBufferData.mWorld = XMMatrixTranspose(g_World_of[key]);
//				}
//				else
//				{
//					g_World_of[key] = XMMatrixTranslationFromVector(position);
//					m_constantBufferData.mWorld = XMMatrixTranspose(g_World_of[key]);
//				}
//
//				if (key == "Tank")
//				{
//					XMMATRIX pitchMatrix = XMMatrixRotationX(camPitch);
//					XMVECTOR cameraOffset = XMVectorSet(0.0f, 2.0f, -3.0f, 0.0f);
//					cameraOffset = XMVector3TransformCoord(cameraOffset, pitchMatrix);
//					if (angle > 1e-6f)
//					{
//						cameraOffset = XMVector3TransformCoord(cameraOffset,
//							XMMatrixRotationQuaternion(
//								XMQuaternionRotationAxis(XMVector3Normalize(angularVelocity), angle)));
//					}
//					Eye = position + cameraOffset;
//					At = position + XMVectorSet(0, 2, 0, 0);
//					Up = XMVector3TransformCoord(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), pitchMatrix);
//					g_View = XMMatrixLookAtLH(Eye, At, Up);
//					m_constantBufferData.mView = XMMatrixTranspose(g_View);
//				}
//			}
//			else
//			{
//				m_constantBufferData.mWorld = XMMatrixTranspose(XMMatrixIdentity());
//			}
//
//			m_constantBufferData.mMeshColor = XMFLOAT4(0, 1, 0, 1);
//			memcpy(m_pCbvDataBegin + sira[key] * 256, &m_constantBufferData, sizeof(m_constantBufferData));
//		}
//	}
//}

//void Effect(float deltaTime)
//{
//	rotation += 0.01f;
//	XMMATRIX mRotate = XMMatrixRotationY(rotation);
//	XMMATRIX mTranslate = XMMatrixTranslation(-50.0f, 100.0f, 0.0f);
//	XMVECTOR xmvLightPos = XMVectorSet(0, 0, 0, 0);
//	xmvLightPos = XMVector3Transform(xmvLightPos, mTranslate);
//	xmvLightPos = XMVector3Transform(xmvLightPos, mRotate);
//	XMStoreFloat4(&m_constantBufferData.mLightPos, xmvLightPos);
//
//	for (auto& [key, deger] : nesneler)
//	{
//		if (!render[key]) continue;
//
//		// Fırlatma fiziksel başlangıç
//		if (key == "Missile" && FireTankMissile)
//		{
//			XMFLOAT3 vTank = DivideFloat3(nesneler["Tank"].momentum, nesneler["Tank"].mass);
//			XMVECTOR wTank = XMVector3Transform(nesneler["Tank"].angular, nesneler["Tank"].inertiaInverse);
//			XMFLOAT3 tempF = SubtractFloat3(deger.position, nesneler["Tank"].position);
//			XMVECTOR MissileToTank = XMLoadFloat3(&tempF);
//			XMVECTOR tempVec = XMVector3Cross(wTank, MissileToTank);
//			XMStoreFloat3(&tempF, tempVec);
//			tempF = AddFloat3(vTank, tempF);
//			tempVec = XMLoadFloat3(&tempF);
//			XMVECTOR tempMomentum = XMVectorScale(tempVec, deger.mass);
//			XMStoreFloat3(&tempF, tempMomentum);
//			deger.momentum = tempF;
//			deger.angular = XMVector3Cross(MissileToTank, tempMomentum);
//		}
//		else if (key == "Enemy_Missile" && FireEnemyMissile)
//		{
//			XMFLOAT3 vEnemy = DivideFloat3(nesneler["Enemy"].momentum, nesneler["Enemy"].mass);
//			XMVECTOR wEnemy = XMVector3Transform(nesneler["Enemy"].angular, nesneler["Enemy"].inertiaInverse);
//			XMFLOAT3 tempF = SubtractFloat3(deger.position, nesneler["Enemy"].position);
//			XMVECTOR MissileToEnemy = XMLoadFloat3(&tempF);
//			XMVECTOR tempVec = XMVector3Cross(wEnemy, MissileToEnemy);
//			XMStoreFloat3(&tempF, tempVec);
//			tempF = AddFloat3(vEnemy, tempF);
//			tempVec = XMLoadFloat3(&tempF);
//			XMVECTOR tempMomentum = XMVectorScale(tempVec, deger.mass);
//			XMStoreFloat3(&tempF, tempMomentum);
//			deger.momentum = tempF;
//			deger.angular = XMVector3Cross(MissileToEnemy, tempMomentum);
//		}
//
//		XMVECTOR v = XMLoadFloat3(&deger.momentum);
//		if (XMVectorGetX(XMVector3Length(v)) <= 1e-6f && XMVectorGetX(XMVector3Length(deger.angular)) <= 1e-6f)
//		{
//			moved[key] = false;
//		}
//		else
//		{
//			moved[key] = true;
//		}
//
//		// 🛠️ ANA DÜZELTME: moved false olsa bile world matrix her karede yazılmalı
//		XMMATRIX worldMatrix;
//		XMVECTOR position = XMLoadFloat3(&deger.position);
//
//		if (deger.mass != FLT_MAX && moved[key])
//		{
//			// Kuvvet ve tork güncelle
//			deger.momentum = AddFloat3(deger.momentum, MultiplyFloat3(deger.kuvvet, deltaTime));
//			deger.angular = XMVectorAdd(deger.angular, XMVectorScale(deger.tork, deltaTime));
//			deger.kuvvet = XMFLOAT3(0.0f, 0.0f, 0.0f);
//			deger.tork = XMVectorZero();
//
//			// Konum güncelle
//			XMVECTOR linearVelocity = XMVectorScale(XMLoadFloat3(&deger.momentum), 1.0f / deger.mass);
//			position = XMVectorAdd(position, XMVectorScale(linearVelocity, deltaTime));
//			XMStoreFloat3(&deger.position, position);
//
//			// Dönüş var mı?
//			XMVECTOR angularVelocity = XMVector3Transform(deger.angular, deger.inertiaInverse);
//			float angle = XMVectorGetX(XMVector3Length(angularVelocity));
//			if (angle > 1e-6f)
//			{
//				XMVECTOR axis = XMVector3Normalize(angularVelocity);
//				XMVECTOR qRotation = XMQuaternionRotationAxis(axis, angle);
//				XMMATRIX mRotate = XMMatrixRotationQuaternion(qRotation);
//				XMMATRIX mTranslate = XMMatrixTranslationFromVector(position);
//				worldMatrix = mRotate * mTranslate;
//			}
//			else
//			{
//				worldMatrix = XMMatrixTranslationFromVector(position);
//			}
//		}
//		else
//		{
//			// Sabit cisim ya da hareket etmiyor ama world matrix yine de yazılmalı
//			worldMatrix = XMMatrixTranslationFromVector(position);
//		}
//
//		// Kamera takibi varsa burada (tank için)
//		if (key == "Tank")
//		{
//			XMMATRIX pitchMatrix = XMMatrixRotationX(camPitch);
//			XMVECTOR cameraOffset = XMVectorSet(0.0f, 2.0f, -3.0f, 0.0f);
//			cameraOffset = XMVector3TransformCoord(cameraOffset, pitchMatrix);
//			XMVECTOR angularVelocity = XMVector3Transform(deger.angular, deger.inertiaInverse);
//			float angle = XMVectorGetX(XMVector3Length(angularVelocity));
//			if (angle > 1e-6f)
//			{
//				cameraOffset = XMVector3TransformCoord(cameraOffset, XMMatrixRotationQuaternion(XMQuaternionRotationAxis(XMVector3Normalize(angularVelocity), angle)));
//			}
//			
//			Eye = position + cameraOffset;
//			At = position + XMVectorSet(0, 2, 0, 0);
//			Up = XMVector3TransformCoord(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), pitchMatrix);
//			g_View = XMMatrixLookAtLH(Eye, At, Up);
//			m_constantBufferData.mView = XMMatrixTranspose(g_View);
//		}
//
//		// World matrix buffer'a yaz
//		g_World_of[key] = worldMatrix;
//		m_constantBufferData.mWorld = XMMatrixTranspose(worldMatrix);
//		m_constantBufferData.mView = XMMatrixTranspose(g_View);
//		m_constantBufferData.mProjection = XMMatrixTranspose(g_Projection);
//		m_constantBufferData.mMeshColor = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
//
//		memcpy(m_pCbvDataBegin + sira[key] * 256, &m_constantBufferData, sizeof(m_constantBufferData));
//	}
//}


//bool TriangleTriangleIntersection(const Triangle& t1, const Triangle& t2,vector<XMFLOAT3>& outPoints, XMVECTOR& outNormal, int& pointCount)
//{
//	auto Load = [](const XMFLOAT3& f) { return XMLoadFloat3(&f); };
//	auto Store = [](XMVECTOR v) { XMFLOAT3 o; XMStoreFloat3(&o, v); return o; };
//
//	XMVECTOR A0 = Load(t1.v0), A1 = Load(t1.v1), A2 = Load(t1.v2);
//	XMVECTOR B0 = Load(t2.v0), B1 = Load(t2.v1), B2 = Load(t2.v2);
//
//	XMVECTOR N1 = XMVector3Normalize(XMVector3Cross(A1 - A0, A2 - A0));
//	XMVECTOR N2 = XMVector3Normalize(XMVector3Cross(B1 - B0, B2 - B0));
//
//	const float EPS = 1e-6f;
//
//	auto ClipEdge = [&](XMVECTOR p1, XMVECTOR p2, XMVECTOR a, XMVECTOR b, XMVECTOR c, vector<XMFLOAT3>& contacts) {
//		XMVECTOR N = XMVector3Cross(b - a, c - a);
//		XMVECTOR dir = p2 - p1;
//		float denom = XMVectorGetX(XMVector3Dot(N, dir));
//		if (fabs(denom) < EPS) return;
//
//		float t = XMVectorGetX(XMVector3Dot(N, a - p1)) / denom;
//		if (t < -EPS || t > 1.0f + EPS) return;
//
//		XMVECTOR P = p1 + t * dir;
//		if (PointInTriangle(P, a, b, c)) {
//			contacts.push_back(Store(P));
//		}
//		};
//
//	vector<XMFLOAT3> contacts;
//
//	// A kenarları, B üçgenine karşı
//	ClipEdge(A0, A1, B0, B1, B2, contacts);
//	ClipEdge(A1, A2, B0, B1, B2, contacts);
//	ClipEdge(A2, A0, B0, B1, B2, contacts);
//
//	// B kenarları, A üçgenine karşı
//	ClipEdge(B0, B1, A0, A1, A2, contacts);
//	ClipEdge(B1, B2, A0, A1, A2, contacts);
//	ClipEdge(B2, B0, A0, A1, A2, contacts);
//
//	// İçeride kalan noktaları da ekle (uç noktalar)
//	if (PointInTriangle(A0, B0, B1, B2)) contacts.push_back(Store(A0));
//	if (PointInTriangle(A1, B0, B1, B2)) contacts.push_back(Store(A1));
//	if (PointInTriangle(A2, B0, B1, B2)) contacts.push_back(Store(A2));
//	if (PointInTriangle(B0, A0, A1, A2)) contacts.push_back(Store(B0));
//	if (PointInTriangle(B1, A0, A1, A2)) contacts.push_back(Store(B1));
//	if (PointInTriangle(B2, A0, A1, A2)) contacts.push_back(Store(B2));
//
//	// Temas varsa normal ve nokta sayısını ayarla
//	if (!contacts.empty()) {
//		outPoints.insert(outPoints.end(), contacts.begin(), contacts.end());
//		pointCount += static_cast<int>(contacts.size());
//		outNormal += N1 + N2;
//		return true;
//	}
//
//	return false;
//}