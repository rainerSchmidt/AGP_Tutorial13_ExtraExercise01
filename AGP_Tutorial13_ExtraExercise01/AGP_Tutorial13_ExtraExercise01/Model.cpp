#include "Model.h"

struct MODEL_CONSTANT_BUFFER
{
	XMMATRIX WorldViewProjection; 
}; // TOTAL SIZE = 64 bytes

Model::Model(ID3D11Device * device, ID3D11DeviceContext * deviceContext, float X, float Y, float Z)
{
	m_pD3DDevice = device;
	m_pImmediateContext = deviceContext;

	m_x = X;
	m_y = Y;
	m_z = Z;
	m_xangle = 0.0f;
	m_yangle = 0.0f;
	m_zangle = 0.0f;
	m_scale = 1.0f;
}

Model :: ~Model()
{
	if (m_pConstantBuffer) m_pConstantBuffer->Release();
	if (m_pInputLayout) m_pInputLayout->Release();
	if (m_pVShader) m_pVShader->Release();
	if (m_pPShader) m_pPShader->Release();
	delete m_pObject;
}

int Model::LoadObjModel(char * filename)
{


	m_pObject = new ObjFileModel(filename, m_pD3DDevice, m_pImmediateContext);

	if (m_pObject->filename == "FILE NOT LOADED") return S_FALSE;

	//___________________________COPIED FROM MAIN
	HRESULT hr = S_OK;

	// Load and compile pixel and vertex shaders - use vs_5_0 to target DX11 hardware only
	ID3DBlob *VS, *PS, *error;
	hr = D3DX11CompileFromFile("model_shaders.hlsl", 0, 0, "ModelVS", "vs_4_0", 0, 0, 0, &VS, &error, 0);

	if (error != 0) //check for shader compilation error
	{
		OutputDebugStringA((char*)error->GetBufferPointer());
		error->Release();
		if (FAILED(hr)) // dont fail if error is just a warning
		{
			return hr;
		};
	}

	hr = D3DX11CompileFromFile("model_shaders.hlsl", 0, 0, "ModelPS", "ps_4_0", 0, 0, 0, &PS, &error, 0);

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
	hr = m_pD3DDevice->CreateVertexShader(VS->GetBufferPointer(), VS->GetBufferSize(), NULL, &m_pVShader);

	if (FAILED(hr))
	{
		return hr;
	}

	hr = m_pD3DDevice->CreatePixelShader(PS->GetBufferPointer(), PS->GetBufferSize(), NULL, &m_pPShader);

	if (FAILED(hr))
	{
		return hr;
	}

	//// Set the shader objects as active
	//m_pImmediateContext->VSSetShader(m_pVShader, 0, 0);
	//m_pImmediateContext->PSSetShader(m_pPShader, 0, 0);

	//Create and set the Input layout object
	D3D11_INPUT_ELEMENT_DESC iedesc[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};

	hr = m_pD3DDevice->CreateInputLayout(iedesc, ARRAYSIZE(iedesc), VS->GetBufferPointer(), VS->GetBufferSize(), &m_pInputLayout);
	//g_pD3DDevice->CreateInputLayout(iedesc, ARRAYSIZE(iedesc), VS->GetBufferPointer(), VS->GetBufferSize(), &g_pInputLayout);

	if (FAILED(hr))
	{
		return hr;
	}

	// Set up and create constant buffer
	D3D11_BUFFER_DESC constant_buffer_desc;
	ZeroMemory(&constant_buffer_desc, sizeof(constant_buffer_desc));

	constant_buffer_desc.Usage = D3D11_USAGE_DEFAULT;  // Can use UpdateSubresource() to update
	constant_buffer_desc.ByteWidth = 64;  // Must be a multiple of 16, calculate from CB struct
	constant_buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER; // Use as a constant buffer

	hr = m_pD3DDevice->CreateBuffer(&constant_buffer_desc, NULL, &m_pConstantBuffer);

	if (FAILED(hr))
	{
		return hr;
	}

	//calculate model centre point
	CalculateModelCentrePoint();
	//Calculate bounding sphere radius
	CalculateBoundingSphereRadius();
}

void Model::Draw(XMMATRIX * view, XMMATRIX * projection)
{
	XMMATRIX world;
	world = XMMatrixScaling(m_scale, m_scale, m_scale);
	world *= XMMatrixRotationX(XMConvertToRadians(m_xangle));
	world *= XMMatrixRotationY(XMConvertToRadians(m_yangle));
	world *= XMMatrixRotationZ(XMConvertToRadians(m_zangle));
	world *= XMMatrixTranslation(m_x, m_y, m_z);

	MODEL_CONSTANT_BUFFER model_cb_values;
	model_cb_values.WorldViewProjection = world*(*view)*(*projection);

	// Set constant buffer to active
	m_pImmediateContext->VSSetConstantBuffers(0, 1, &m_pConstantBuffer);

	// upload the new values for the constant buffer
	m_pImmediateContext->UpdateSubresource(m_pConstantBuffer, 0, 0, &model_cb_values, 0, 0);
	

	// Set the shader objects as active
	m_pImmediateContext->VSSetShader(m_pVShader, 0, 0);
	m_pImmediateContext->PSSetShader(m_pPShader, 0, 0);

	m_pImmediateContext->IASetInputLayout(m_pInputLayout);

	m_pObject->Draw();
}

void Model::LookAt_XZ(float X, float Z)
{
	float DX = X - m_x;
	float DZ = Z - m_z;

	m_yangle = atan2(DX, DZ) * (180.0 / XM_PI);
}

void Model::MoveForwards(float Distance)
{
	m_x += sin(m_yangle * (XM_PI / 180.0)) * Distance;
	m_z += cos(m_yangle * (XM_PI/180.0)) * Distance;
}

void Model::CalculateModelCentrePoint()
{
	float MaxX = 0;
	float MaxY = 0;
	float MaxZ = 0;
	float MinX = 0;
	float MinY = 0;
	float MinZ = 0;

	for (int i = 0; i < m_pObject->numverts; i++)
	{
		//see if x is bigger than MaxX -> set MaxX
		if (m_pObject->vertices[i].Pos.x > MaxX)
			MaxX = m_pObject->vertices[i].Pos.x;
		//see if x is smaller than MinX -> set MinX
		if (m_pObject->vertices[i].Pos.x < MinX)
			MinX = m_pObject->vertices[i].Pos.x;

		//see if y is bigger than MaxY -> set MaxY
		if (m_pObject->vertices[i].Pos.x > MaxY)
			MaxY = m_pObject->vertices[i].Pos.x;
		//see if y is smaller than MinY -> set MinY
		if (m_pObject->vertices[i].Pos.x < MinY)
			MinY = m_pObject->vertices[i].Pos.x;

		//see if z is bigger than MaxZ -> set MaxZ
		if (m_pObject->vertices[i].Pos.x > MaxZ)
			MaxZ = m_pObject->vertices[i].Pos.x;
		//see if z is smaller than MinZ -> set MinZ
		if (m_pObject->vertices[i].Pos.x < MinZ)
			MinZ = m_pObject->vertices[i].Pos.x;
	}

	//calculate sphere centre points from min and max values
	m_bounding_sphere_centre_x = MinX + ((MaxX - MinX) / 2);
	m_bounding_sphere_centre_y = MinY + ((MaxY - MinY) / 2);
	m_bounding_sphere_centre_z = MinZ + ((MaxZ - MinZ) / 2);
}

void Model::CalculateBoundingSphereRadius()
{
	float Distance = 0;

	for (int i = 0; i < m_pObject->numverts; i++)
	{
		// see if distance between sphere centre point(X) and vertex point(X) is bigger than Distance -> set Distance
		float distanceX = m_pObject->vertices[i].Pos.x - m_bounding_sphere_centre_x;
		if (distanceX > Distance) Distance = distanceX;

		// see if distance between sphere centre point(Y) and vertex point(Y) is bigger than Distance -> set Distance
		float distanceY = m_pObject->vertices[i].Pos.y - m_bounding_sphere_centre_y;
		if (distanceY > Distance) Distance = distanceY;

		// see if distance between sphere centre point(Z) and vertex point(Z) is bigger than Distance -> set Distance
		float distanceZ = m_pObject->vertices[i].Pos.z - m_bounding_sphere_centre_z;
		if (distanceZ > Distance) Distance = distanceZ;
	}

	// set bounding sphere radius with calculated maximum distance
	m_bounding_sphere_centre_radius = Distance;

}

bool Model::CheckCollision(Model* Model)
{
	if (Model == this)
		return false;

	XMVECTOR CurrentModelWSP = GetBoundingSphereWorldSpacePosition();
	XMVECTOR OtherModelWSP = Model->GetBoundingSphereWorldSpacePosition();

	float x1 = XMVectorGetX(CurrentModelWSP);
	float y1 = XMVectorGetY(CurrentModelWSP);
	float z1 = XMVectorGetZ(CurrentModelWSP);
	float x2 = XMVectorGetX(OtherModelWSP);
	float y2 = XMVectorGetY(OtherModelWSP);
	float z2 = XMVectorGetZ(OtherModelWSP);
	float DistanceX = 0;
	float DistanceY = 0;
	float DistanceZ = 0;
	//string DebugString = "";

	// check: sphere collision on X plane
	DistanceX = x1 - x2;
	if (DistanceX < 0) DistanceX *= -1.0f;

	/*DebugString = "Sphere Collision Check: Distance X= " + to_string(DistanceX) + "\n";
	OutputDebugString(DebugString.c_str());*/

	// check: sphere collision on Y plane
	DistanceY = y1 - y2;
	if (DistanceY < 0) DistanceY *= -1.0f;

	/*DebugString = "Sphere Collision Check: Distance Y= " + to_string(DistanceY) + "\n";
	OutputDebugString(DebugString.c_str());*/

	// check: sphere collision on Z plane
	DistanceZ = z1 - z2;
	if (DistanceZ < 0)DistanceZ *= -1.0f;

	/*DebugString = "Sphere Collision Check: Distance Z= " + to_string(DistanceZ) + "\n";
	OutputDebugString(DebugString.c_str());*/

	// check if collision is happening -> return true
	if (DistanceX < (m_bounding_sphere_centre_radius + Model->GetBoundingSphereRadius())
		&& DistanceY < (m_bounding_sphere_centre_radius + Model->GetBoundingSphereRadius())
		&& DistanceZ < (m_bounding_sphere_centre_radius + Model->GetBoundingSphereRadius()))
		return true;

	return false;
}

XMVECTOR Model::GetBoundingSphereWorldSpacePosition()
{
	XMMATRIX world;
	world = XMMatrixScaling(m_scale, m_scale, m_scale);
	world *= XMMatrixRotationX(m_xangle);
	world *= XMMatrixRotationY(m_yangle);
	world *= XMMatrixRotationZ(m_zangle);
	world *= XMMatrixTranslation(m_x, m_y, m_z);

	XMVECTOR offset;
	offset = XMVectorSet(m_bounding_sphere_centre_x, m_bounding_sphere_centre_y, m_bounding_sphere_centre_z, 0.0);
	offset = XMVector3Transform(offset, world);

	return offset;
}

float Model::GetM_X() { return m_x; }
float Model::GetM_Y() { return m_y; }
float Model::GetM_Z() { return m_z; }
float Model::GetRotX() { return m_xangle; }
float Model::GetRotY() { return m_yangle; }
float Model::GetRotZ() { return m_zangle; }
float Model::GetScale() { return m_scale; }

float Model::GetBoundingSphereCentreX() { return m_bounding_sphere_centre_x; }
float Model::GetBoundingSphereCentreY() { return m_bounding_sphere_centre_y; }
float Model::GetBoundingSphereCentreZ() { return m_bounding_sphere_centre_z; }
float Model::GetBoundingSphereRadius() { return m_bounding_sphere_centre_radius; }

void Model::SetPosX(float pos) { m_x = pos; }
void Model::SetPosY(float pos) { m_y = pos; }
void Model::SetPosZ(float pos) { m_z = pos; }
void Model::SetRotX(float rot) { m_xangle = rot; }
void Model::SetRotY(float rot) { m_yangle = rot; }
void Model::SetRotZ(float rot) { m_zangle = rot; }
void Model::SetScale(float amount) { m_scale = amount; }