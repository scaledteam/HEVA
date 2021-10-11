//
// Created by scaled
//

#define DLIB_THREAD_SIGNAL_NONE 0
#define DLIB_THREAD_SIGNAL_RESET 1

#include <pthread.h>

struct face_recognition_data {
	double MTH_A;
	double MTH_U;
	double MTH_Fun;
	double MTH_E;
	double BRW_Fun;
	double BRW_Angry;
	double EYE_Close_L;
	double EYE_Close_R;
	double rotation1;
	double rotation2;
	double rotation3;
	double translation1;
	double translation2;
	double translation3;
};

struct dlib_thread_data {
	unsigned char dlib_thread_active;
	unsigned char dlib_thread_ready;
	unsigned char dlib_thread_signal;
	pthread_cond_t dlib_thread_cond;
	face_recognition_data face_data;
};

void* dlib_thread_function(void* data);
