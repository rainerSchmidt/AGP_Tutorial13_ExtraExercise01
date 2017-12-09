#pragma once

#include <d3d11.h>
#define _XM_NO_INTINSICS_
#define XM_NO_ALIGNMENT
#include <xnamath.h>

class Camera
{
	public: 
		Camera(float cameraPositionX, float cameraPositionY, float cameraPositionZ, float cameraRotation);
		~Camera();
		void Rotate(float amountInDegrees);
		void Forward(float distance);
		void Up();
		void Strafe(float distance);
		float GetX();
		float GetY();
		float GetZ();
		XMMATRIX GetViewMatrix();
	private:
		float m_x, m_y, m_z, m_dx, m_dz, m_camera_rotation=0.0f;
		XMVECTOR m_position, m_lookat, m_up;
};
