//
// Created by scaled
//

#define DLIB_THREAD_SIGNAL_NONE 0
#define DLIB_THREAD_SIGNAL_RESET 1

#define DLIB_THREAD_SIGNAL_CALIBRATE_MTH_Neutral 2
#define DLIB_THREAD_SIGNAL_CALIBRATE_MTH_E 3
#define DLIB_THREAD_SIGNAL_CALIBRATE_MTH_A 4
#define DLIB_THREAD_SIGNAL_CALIBRATE_MTH_Fun 5
#define DLIB_THREAD_SIGNAL_CALIBRATE_MTH_U 6

#define HEVA_CONFIG_PATH "heva.ini"

#include <pthread.h>
#include <dlib/image_processing.h>
#include <dlib/image_processing/frontal_face_detector.h>

struct face_recognition_data {
	double MTH_A;
	double MTH_U;
	double MTH_Fun;
	double MTH_E;
	double MTH_LeftRight;
	double BRW_Fun;
	double BRW_Angry;
	double EYE_Close;
	double EYE_Close_L;
	double EYE_Close_R;
	double rotation1;
	double rotation2;
	double rotation3;
	double translation1;
	double translation2;
	double translation3;
	bool on_screen;
};

struct webcam_settings_t {
	int Width = -1;
	int Height = -1;
	int Fps = -1;
	bool Setup = true;
	bool YUYV = false;
	bool LimitTo24or25 = false;
	char PreferredName[256] = "";
	int PreferredId = -1;
	char Format[4] = "";
	bool Sync = true;
	bool SyncType2 = true;
	bool MouthIndirect = false;
	float Gamma = 1.0;
	int Buffer = -1;
	bool EyeSync = true;
	char shapePredictorPath[256] = "";
};

struct dlib_thread_data {
	unsigned char dlib_thread_active;
	unsigned char dlib_thread_ready;
	unsigned char dlib_thread_signal;
	pthread_cond_t dlib_thread_cond;
	face_recognition_data face_data;
	webcam_settings_t* webcam_settings;
	
	pthread_cond_t dlib_thread2_cond1;
	pthread_cond_t dlib_thread2_cond2;
	bool facesFound;
	bool faceTracked;
	dlib::correlation_tracker tracker;
	double tracker_reference_psr;
	dlib::point faceCenter;
	dlib::rectangle faceRect;
	
	bool thread1_waiting;
};

void* dlib_thread1_function(void* data);
void* dlib_thread2_function(void* data);
