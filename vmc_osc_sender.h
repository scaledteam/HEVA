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
	Urho3D::Quaternion eyesRot;
	Urho3D::Vector3 headLoc;
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
