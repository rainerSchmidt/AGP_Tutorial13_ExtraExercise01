#include "camera.h"
#include <math.h>

	Camera::Camera(float cameraPositionX, float cameraPositionY, float cameraPositionZ, float cameraRotation)
	{
		m_x = cameraPositionX;
		m_y = cameraPositionY;
		m_z = cameraPositionZ;
		m_camera_rotation = cameraRotation;

		m_dx = sin(m_camera_rotation * (XM_PI / 180.0));
		m_dz = cos(m_camera_rotation * (XM_PI / 180.0));
	}

	Camera::~Camera()
	{
	}

	void Camera::Rotate(float amountInDegrees)
	{
		m_camera_rotation += amountInDegrees;

		m_dx = sin(m_camera_rotation * (XM_PI / 180.0));
		m_dz = cos(m_camera_rotation * (XM_PI / 180.0));
	}

	void Camera::Forward(float distance)
	{
		m_x += m_dx * distance;
		m_z += m_dz * distance;
	}

	

	void Camera::Up()
	{
		 
	}

	XMMATRIX Camera::GetViewMatrix()
	{
		m_position = XMVectorSet(m_x, m_y, m_z, 0.0);
		m_lookat = XMVectorSet(m_x + m_dx, m_y, m_z + m_dz, 0.0);
		m_up = XMVectorSet(0.0, 1.0, 0.0, 0.0);

		XMMATRIX view = XMMatrixLookAtLH(m_position, m_lookat, m_up);

		return view;
	}

	void Camera::Strafe(float distance)
	{
		XMVECTOR right = XMVector3Cross((m_position - m_lookat), (m_up - m_position));
		m_x += right.m128_f32[0] *distance;
		m_z += right.m128_f32[2] * distance;
	}

	float Camera::GetX()
	{
		return m_x;
	}
	float Camera::GetY()
	{
		return m_y;
	}
	float Camera::GetZ()
	{
		return m_z;
	}

	