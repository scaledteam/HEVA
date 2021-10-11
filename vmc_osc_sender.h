//
// Created by scaled
//

#include <pthread.h>
#include <Urho3D/Math/Quaternion.h>
#include <Urho3D/Math/Vector3.h>

struct vmc_osc_data {
	unsigned char vmc_osc_sender_thread_active;
	pthread_cond_t cond;
	const char* udp_addr;
	unsigned int udp_port;
	Urho3D::Quaternion headRot;
	Urho3D::Quaternion neckRot;
	Urho3D::Quaternion hipsRot;
	Urho3D::Quaternion boneLeg_L;
	Urho3D::Quaternion boneLeg_L_2;
	Urho3D::Quaternion boneLeg_L_3;
	Urho3D::Quaternion boneLeg_R;
	Urho3D::Quaternion boneLeg_R_2;
	Urho3D::Quaternion boneLeg_R_3;
	Urho3D::Vector3 hipsLoc;
	double MTH_A;
	double MTH_U;
	double MTH_Fun;
	double MTH_E;
	double BRW_Fun;
	double BRW_Angry;
	double EYE_Close_L;
	double EYE_Close_R;
};

void* vmc_osc_sender_thread_function(void* data);
