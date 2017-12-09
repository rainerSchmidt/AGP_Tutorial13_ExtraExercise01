#pragma once
#include "objfilemodel.h"

class Model
{
private:
	ID3D11Device * m_pD3DDevice;
	ID3D11DeviceContext * m_pImmediateContext;

	ObjFileModel *			m_pObject;
	ID3D11VertexShader *	m_pVShader;
	ID3D11PixelShader *		m_pPShader;
	ID3D11InputLayout *		m_pInputLayout;
	ID3D11Buffer *			m_pConstantBuffer;

	float m_x, m_y, m_z;
	float m_xangle, m_zangle, m_yangle;
	float m_scale;

	float m_bounding_sphere_centre_x;
	float m_bounding_sphere_centre_y;
	float m_bounding_sphere_centre_z;
	float m_bounding_sphere_centre_radius;

	void CalculateModelCentrePoint();
	void CalculateBoundingSphereRadius();
public:

	Model(ID3D11Device * device, ID3D11DeviceContext * deviceContext, float X, float Y, float Z);
	~Model();
	int LoadObjModel(char* fielname);
	void Draw(XMMATRIX * view, XMMATRIX * projection);
	void LookAt_XZ(float X, float Z);
	void MoveForwards(float Distance);
	bool CheckCollision(Model* Model);

	float GetM_X();
	float GetM_Y();
	float GetM_Z();
	float GetRotX();
	float GetRotY();
	float GetRotZ();
	float GetScale();
	
	float GetBoundingSphereCentreX();
	float GetBoundingSphereCentreY();
	float GetBoundingSphereCentreZ();
	float GetBoundingSphereRadius();
	XMVECTOR GetBoundingSphereWorldSpacePosition();

	void SetPosX(float pos);
	void SetPosY(float pos);
	void SetPosZ(float pos);
	void SetRotX(float rot);
	void SetRotY(float rot);
	void SetRotZ(float rot);
	void SetScale(float amount);

};
