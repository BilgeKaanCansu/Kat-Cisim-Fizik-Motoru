#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <DirectXMath.h>
#include <windows.h>
#include <map>
#include <windows.h>
#include <WindowsX.h>
#include <d3d12.h>
#include "d3dx12.h"
#include <dxgi1_4.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include <string>
#include <sstream>
#include <vector>
#include <wrl.h>
#include "resource.h"
#include "DDSTextureLoader.h"
#include "dinput.h"
#include <fstream>

using namespace DirectX;
using namespace std;
namespace fs = std::filesystem;
using Microsoft::WRL::ComPtr;

struct Vertex
{
    XMFLOAT3 position;
    XMFLOAT3 normal;
    XMFLOAT2 texture;
};

typedef struct
{
	float x, y, z;
} VertexType;

typedef struct
{
	int vIndex1, vIndex2, vIndex3;
	int tIndex1, tIndex2, tIndex3;
	int nIndex1, nIndex2, nIndex3;
} FaceType;

struct Object
{
	Vertex* Points;
	XMFLOAT3 velocity;
	XMFLOAT3 angular;
	float density;
};

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
float Pi = 3.1415926535f;

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

float mTheta = 1.5f * XM_PI;
float mPhi = XM_PIDIV4;    /////yakýnlaþtýrma ayarý için deðiþebilen deðer
float mRadius = 20.0f;
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

map<string, int>vertex_count_of;
map<string, ComPtr<ID3D12Resource>>m_vertexBuffer_of;
map<string, D3D12_VERTEX_BUFFER_VIEW>m_vertexBufferView_of;
map<string, ComPtr<ID3D12Resource>>textureBuffer_of;
map<string, ComPtr<ID3D12Resource>>textureBufferUploadHeap_Tank;


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

vector<string> Texture_Finder(const string& folderPath) {
	vector<string> filePaths;

	try {
		for (const auto& entry : fs::directory_iterator(folderPath)) {
			if (entry.is_regular_file() && entry.path().extension().string() == ".dds") {
				filePaths.push_back(entry.path().string()); // Tam dosya yolu
			}
		}
	}
	catch (const fs::filesystem_error& e) {
		cerr << "Hata: " << e.what() << endl;
	}

	return filePaths;
}

Object* Obj_Loader(char* filename, int* verticesCount)
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

	// Read from the file and continue to read until the end of the file is reached.
	fin.get(input);
	while (!fin.eof())
	{
		// If the line starts with 'v' then count either the vertex, the texture coordinates, or the normal vector.
		if (input == 'v')
		{
			fin.get(input);
			if (input == ' ') { vertexCount++; }
			if (input == 't') { textureCount++; }
			if (input == 'n') { normalCount++; }
		}

		// If the line starts with 'f' then increment the face count.
		if (input == 'f')
		{
			fin.get(input);
			if (input == ' ') { faceCount++; }
		}

		// Otherwise read in the remainder of the line.
		while (input != '\n')
		{
			fin.get(input);
		}

		// Start reading the beginning of the next line.
		fin.get(input);
	}

	// Close the file.
	fin.close();

	VertexType* vertices, * texcoords, * normals, velocity, angular;
	FaceType* faces;
	int vertexIndex, texcoordIndex, normalIndex, faceIndex, vIndex, tIndex, nIndex;
	char input2;
	float density

		// Initialize the four data structures.
		vertices = new VertexType[vertexCount];
	texcoords = new VertexType[textureCount];
	normals = new VertexType[normalCount];
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
	while (!fin.eof())
	{
		if (input == 'v')
		{
			fin.get(input);

			// Read in the vertices.
			if (input == ' ')
			{
				fin >> vertices[vertexIndex].x >> vertices[vertexIndex].y >> vertices[vertexIndex].z;

				// Invert the Z vertex to change to left hand system.
				vertices[vertexIndex].z = vertices[vertexIndex].z * -1.0f;
				vertexIndex++;
			}

			// Read in the texture uv coordinates.
			if (input == 't')
			{
				fin >> texcoords[texcoordIndex].x >> texcoords[texcoordIndex].y;

				// Invert the V texture coordinates to left hand system.
				texcoords[texcoordIndex].y = 1.0f - texcoords[texcoordIndex].y;
				texcoordIndex++;
			}

			// Read in the normals.
			if (input == 'n')
			{
				fin >> normals[normalIndex].x >> normals[normalIndex].y >> normals[normalIndex].z;

				// Invert the Z normal to change to left hand system.
				normals[normalIndex].z = normals[normalIndex].z * -1.0f;
				normalIndex++;
			}
		}

		// Read in the faces.
		if (input == 'f')
		{
			fin.get(input);
			if (input == ' ')
			{
				// Read the face data in backwards to convert it to a left hand system from right hand system.
				fin >> faces[faceIndex].vIndex3 >> input2 >> faces[faceIndex].tIndex3 >> input2 >> faces[faceIndex].nIndex3
					>> faces[faceIndex].vIndex2 >> input2 >> faces[faceIndex].tIndex2 >> input2 >> faces[faceIndex].nIndex2
					>> faces[faceIndex].vIndex1 >> input2 >> faces[faceIndex].tIndex1 >> input2 >> faces[faceIndex].nIndex1;
				faceIndex++;
			}
		}
		if (input == 'd')
		{
			fin.get(input);
			if (input == ' ')
			{
				fin >> density;
			}
		}
		if (input == 'h')
		{
			fin.get(input);
			if (input == ' ')
			{
				fin >> velocity.x >> velocity.y >> velocity.z;
			}
		}
		if (input == 'a')
		{
			fin.get(input);
			if (input == ' ')
			{
				fin >> angular.x >> angular.y >> angular.z;
			}
		}

		// Read in the remainder of the line.
		while (input != '\n')
		{
			fin.get(input);
		}

		// Start reading the beginning of the next line.
		fin.get(input);
	}

	// Close the file.
	fin.close();

	*verticesCount = faceCount * 3;
	Vertex* verticesModel = new Vertex[*verticesCount];

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

	Object* ObjectModel = new Object(verticesModel, velocity, angular, density);

	// Release the four data structures.
	delete[] vertices;
	delete[] texcoords;
	delete[] normals;
	delete[] faces;
	delete velocity;
	delete density;
	delete[] verticesModel;

	return ObjectModel;
}

map<string, Object> Obj_Lister(string path, map<string, int>verticeCount)
{
	vector<string> list = Obj_Finder(path);
	map<string, Object> Dict;
	for (int i = 0; i < list.size(); i++)
	{
		Dict[dosyaAdiAl(list[i])] = Obj_Loader(list[i], verticeCount[dosyaAdiAl(list[i])]);
	}
	return Dict;
}


void onInit(string objPath)
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
		srvHeapDesc.NumDescriptors = 3;	// Önemli! : Doku sayýsý deðiþince bunu da güncelle
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

	map<string, Object> nesneler = Obj_Lister(objPath,vertex_count_of);
	const UINT cbvSrvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	// Get a handle to the start of the descriptor heap.
	CD3DX12_CPU_DESCRIPTOR_HANDLE m_descriptorHeapHandle(m_descriptorHeap->GetCPUDescriptorHandleForHeapStart());
	
	string key;
	Object deger;
	int i = 0;
	for (const auto& [key]:nesneler)
	{
		const UINT vertexBufferSize = vertex_count_of[key] * sizeof(Vertex);
		ThrowIfFailed(m_device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize), D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_vertexBuffer_of[key])));

		// Copy the triangle data to the vertex buffer.
		UINT8* pVertexDataBegin;
		CD3DX12_RANGE readRange(0, 0);		// We do not intend to read from this resource on the CPU.
		ThrowIfFailed(m_vertexBuffer_of[key]->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
		memcpy(pVertexDataBegin, &nesneler[key]->vertices[0], vertexBufferSize);
		m_vertexBuffer_of[key]->Unmap(0, nullptr);

		// Initialize the vertex buffer view.
		m_vertexBufferView_of[key].BufferLocation = m_vertexBuffer_of[]->GetGPUVirtualAddress();
		m_vertexBufferView_of[key].StrideInBytes = sizeof(Vertex);
		m_vertexBufferView_of[key].SizeInBytes = vertexBufferSize;

		//Varsa texture Buffer
		if (texture_buffer_of.find(key) != texture_buffer_of.end())
		{
			ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(m_device.Get(), m_commandList.Get(), textures[key], textureBuffer_of[key], textureBufferUploadHeap_of[key]));

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

}



/*
* SABÝT OLARAK TANIMLANANLAR LÝSTESÝ
* 
XMMATRIX g_World;
XMMATRIX g_View;
XMMATRIX g_Projection;

HINSTANCE m_hinst			= NULL;
HWND m_hwnd					= NULL;
UINT m_width				= 1280;
UINT m_height				= 720;
UINT m_rtvDescriptorSize	= 0;
bool m_useWarpDevice		= false;	// Adapter info.
float rotation				= 0.0;
float speed					= 0.0;
const UINT FrameCount		= 2;
float Pi					= 3.1415926535f;

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
UINT8*								m_pCbvDataBegin = NULL;

// Synchronization objects.
UINT								m_frameIndex;
HANDLE								m_fenceEvent;
ComPtr<ID3D12Fence>					m_fence;
UINT64								m_fenceValue;

float mTheta	= 1.5f*XM_PI;
float mPhi		= XM_PIDIV4;    /////yakýnlaþtýrma ayarý için deðiþebilen deðer
float mRadius	= 20.0f;
POINT mLastMousePos;

XMVECTOR Eye;
XMVECTOR At;
XMVECTOR Up;

XMVECTOR DefaultForward		= XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
XMVECTOR DefaultRight		= XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
XMVECTOR camForward			= XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
XMVECTOR camRight			= XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);

XMMATRIX camRotationMatrix;
float moveLeftRight		= 0.0f;
float moveBackForward	= 0.0f;
float camYaw			= 0.0f;
float camPitch			= 0.0f;

initin baþ kýsýmlarýný bir init fonksiyonuna alalým
------------------------------------------->>>>>>>>>>>>>ÝNÝTÝN ÝÇÝNDE  """"srvHeapDesc.NumDescriptors= 3;// Önemli! : Doku sayýsý deðiþince bunu da güncelle  """""    kýsmýný araþtýr
sabit kýsýmlardan sonra obj_lister fonksiyonunu çaðýr bundan sonra

const UINT cbvSrvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	// Get a handle to the start of the descriptor heap.
	CD3DX12_CPU_DESCRIPTOR_HANDLE m_descriptorHeapHandle(m_descriptorHeap->GetCPUDescriptorHandleForHeapStart());

bundan sonra ise forlu bölgeye git
forlu kýsýmlardan dön ve
--------------------------------->>>>g_worldlarýn tamamýný XMMatrixIdentity() e eþitle//bu kýmý for ile yap 
ve ininti bitir
----------------------->>>>>>>>>>>>>>>>ONUPDATE FONKSÝYONUNU KULLANICIYA BIRAK KÜTÜPHANEYE ALMA ALIYORSAN DA EN SONA BIRAK<<<<<<<<<<<<<<<<<<<<<<<<<<------------------
on render in baþý ayný
* 
*/


/*
* ///FOR DÖNGÜSÜ ÝÇÝNDE DOLDURULACAKLAR

map<string, ComPtr<ID3D12Resource>> m_vertexBuffer_of;                 
map<string, D3D12_VERTEX_BUFFER_VIEW> m_vertexBufferView_of;
map<string, ComPtr<ID3D12Resource>> textureBuffer_of;
map<string, ComPtr<ID3D12Resource>> textureBufferUploadHeap_of;

map<string,Object> nesneleri

map<string, int> vertexCount_of;

map <string, XMMATRIX> g_world_of

////pozisyonlar ve rotasyonlar ro ve rd ler cisim tanýmýnda olsun

initin forlu kýsýmlarýnda 


// Create the Vertex Buffer for nesne

	const UINT vertexBufferSize = vertexCount_of[] * sizeof(Vertex);
	ThrowIfFailed(m_device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize), D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_vertexBuffer_Tank)));

	// Copy the triangle data to the vertex buffer.
	UINT8* pVertexDataBegin;
	CD3DX12_RANGE readRange(0, 0);		// We do not intend to read from this resource on the CPU.
	ThrowIfFailed(m_vertexBuffer_Tank->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
	memcpy(pVertexDataBegin, &nesneleri[]->vertices[0], vertexBufferSize);
	m_vertexBuffer_Tank->Unmap(0, nullptr);

	// Initialize the vertex buffer view.
	m_vertexBufferView_Tank.BufferLocation = m_vertexBuffer_of[]->GetGPUVirtualAddress();
	m_vertexBufferView_Tank.StrideInBytes  = sizeof(Vertex);
	m_vertexBufferView_Tank.SizeInBytes    = vertexBufferSize;

//Varsa texture Buffer
	
if(texture_buffer_of.find()!=texture_buffer_of.end())
	{
		
		
		ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(m_device.Get(), m_commandList.Get(), textures["key"], textureBuffer_of[], textureBufferUploadHeap_of[]));

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping			= D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format							= textureBuffer_of[]->GetDesc().Format;
		srvDesc.ViewDimension					= D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip		= 0;
		srvDesc.Texture2D.MipLevels				= textureBuffer_of[]->GetDesc().MipLevels;
		srvDesc.Texture2D.ResourceMinLODClamp	= 0.0f;

		m_device->CreateShaderResourceView(textureBuffer_of[].Get(), &srvDesc, m_descriptorHeapHandle);
		m_descriptorHeapHandle.Offset(1, cbvSrvDescriptorSize);	
	
	}
///buradan sonra initin normal kýsýmlarýna dönüyoruz

on renderin for lu kýsýmlarýnda 

if(texture_buffer_of.find()!=texture_buffer_of.end() && render[key])
	m_commandList->SetPipelineState(m_pipelineState_Textured.Get());
	
	Texture.Offset(1, cbvSrvDescriptorSize);                        ///////Bu kýsým farklý bir texture geçildiði zaman koþulacak düzelt bu kýsmý  //zorunlu deðil þimdilik...
	m_commandList->SetGraphicsRootDescriptorTable(1, Texture);
	
	m_commandList->SetGraphicsRootConstantBufferView(0, m_constantBuffer->GetGPUVirtualAddress() + i * 256);
	m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView_Ground);
	m_commandList->DrawInstanced(vertexCount_Ground, 1, 0, 0);

	else if(render[key])
		m_commandList->SetPipelineState(m_pipelineState_Phong.Get());
		m_commandList->SetGraphicsRootConstantBufferView(0, m_constantBuffer->GetGPUVirtualAddress() + i * 256);
		m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView_Walls);
		m_commandList->DrawInstanced(vertexCount_Walls, 1, 0, 0);

*/




/*
GÖRÜÞMEYE KADAR HEDEFLER

* buffer olaylarýna bak  //bunlarý birer fonksiyon haline getir. 
* object tanýmýný teze göre düzenle bunu yaparken fonksiyonlarý da düzenle  //konum ,quaternion, doðrusal ve açýsal momentum, yoðunluk   //Ibody ve tersini hesapla ve tut  //konumu vertexlerin aðýrlýk merkezi olarak hesapla
*/


/* SONRAKÝ HEDEFLER
* intersection kodunu yaz 
* çarpýþma kodunu yaz
* temas kodunu yaz
* deformasyon kodunu yaz
* duplicate olayýna bak   ///en son
* 
*/