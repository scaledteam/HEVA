//
// Created by scaled
//

#include "vmc_osc_sender.h"
#include "tinyosc.h"
//#include <thread>
#include <pthread.h>
#include <Urho3D/Math/Quaternion.h>
#include <Urho3D/Math/Vector3.h>
#include <cstdio>

#include <arpa/inet.h>
#include <sys/socket.h>

pthread_mutex_t vmc_osc_sender_thread_mutex = PTHREAD_MUTEX_INITIALIZER;

void* vmc_osc_sender_thread_function(void* arg)
{
	vmc_osc_data* data = (vmc_osc_data*)arg;
	
	int socket_fd;
	if ( (socket_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
		printf("socket creation failed");
		return NULL;
	}
	
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family    = AF_INET; // IPv4
	addr.sin_addr.s_addr = inet_addr(data->udp_addr);
	addr.sin_port = htons(data->udp_port);
	
	
	printf("Server started on addr %s, port %d\n", data->udp_addr, data->udp_port);
	
	while (data->vmc_osc_sender_thread_active) {
		pthread_mutex_lock(&vmc_osc_sender_thread_mutex);
		pthread_cond_wait(&data->cond, &vmc_osc_sender_thread_mutex);
		pthread_mutex_unlock(&vmc_osc_sender_thread_mutex);
		
		// declare a buffer for writing the OSC packet into
		char buffer[2048];
		
		tosc_bundle bundle;
		tosc_writeBundle(&bundle, 0, buffer, sizeof(buffer));


		tosc_writeNextMessage(&bundle, "/VMC/Ext/OK", "i", 1);
		
		tosc_writeNextMessage(&bundle, "/VMC/Ext/Blend/Val", "sf", "A", data->MTH_A);
		tosc_writeNextMessage(&bundle, "/VMC/Ext/Blend/Val", "sf", "U", data->MTH_U);
		tosc_writeNextMessage(&bundle, "/VMC/Ext/Blend/Val", "sf", "E", data->MTH_E);
		tosc_writeNextMessage(&bundle, "/VMC/Ext/Blend/Val", "sf", "Fun", data->MTH_Fun);
		tosc_writeNextMessage(&bundle, "/VMC/Ext/Blend/Val", "sf", "Blink_L", data->EYE_Close_L);
		tosc_writeNextMessage(&bundle, "/VMC/Ext/Blend/Val", "sf", "Blink_R", data->EYE_Close_R);
		tosc_writeNextMessage(&bundle, "/VMC/Ext/Blend/Val", "sf", "BrowsDownUp", data->BRW_Fun-data->BRW_Angry);
		
		tosc_writeNextMessage(&bundle, "/VMC/Ext/Bone/Pos", "sfffffff",
		    "Head", data->headLoc.x_, data->headLoc.y_, data->headLoc.z_, data->headRot.x_, data->headRot.y_, data->headRot.z_, data->headRot.w_
		    //"Head", data->headLoc.x_, data->headLoc.y_, data->headLoc.z_, data->headRot.z_, data->headRot.y_, data->headRot.x_, data->headRot.w_
		);
		
		tosc_writeNextMessage(&bundle, "/VMC/Ext/Bone/Pos", "sfffffff",
		    "LeftEye", 0.0, 0.0, 0.0, data->eyesRot.x_, data->eyesRot.y_, data->eyesRot.z_, data->eyesRot.w_
		);
		tosc_writeNextMessage(&bundle, "/VMC/Ext/Bone/Pos", "sfffffff",
		    "RightEye", 0.0, 0.0, 0.0, data->eyesRot.x_, data->eyesRot.y_, data->eyesRot.z_, data->eyesRot.w_
		);
		
		tosc_writeNextMessage(&bundle, "/VMC/Ext/Apply", "");
		
		sendto(socket_fd, buffer, tosc_getBundleLength(&bundle), 0, (struct sockaddr*) &addr, sizeof(addr));
		
    		//vmcp_client.send_message("/VMC/Ext/Blend/Val", ["Joy", random()])
	}
	return NULL;
}
