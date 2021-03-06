//
// Created by scaled
//

//#define TRACKING_LOG
//#define TRACKING_STRAIT
//#define TRACKING_DISPLAY

#define POINTS_SMOOTHING .25
#define POINTS_SMOOTHING_DISTANCE 12

#define ADAPT_SPEED_1 .0025
#define ADAPT_SPEED_2 .005
#define ADAPT_SPEED_3 .01
#define ADAPT_SPEED_4 .1
#define ADAPT_STRENGTH_SPEED .9998

#include "face_recognition.h"

#include <dlib/opencv.h>
#include <opencv2/highgui/highgui.hpp>
#include <dlib/image_processing/frontal_face_detector.h>
#include <dlib/image_processing.h>
#include <opencv2/calib3d.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#define FACE_FORM_NEUTRAL 0
#define FACE_FORM_E 1
#define FACE_FORM_A 2
#define FACE_FORM_Fun 3
#define FACE_FORM_U 4

#ifdef ALL_IN_ONE
#include "./inih-r53/cpp/INIReader.h"
#else
#include "INIReader.h"
#endif

#include <dlib/filtering.h>

#ifdef TRACKING_DISPLAY
#include <dlib/image_processing/render_face_detections.h>
#include <dlib/gui_widgets.h>

dlib::image_window win;
#endif

//#ifdef linux
#ifdef __unix__
	#define WEBCAM_BACKEND cv::CAP_V4L2
#else
	#ifdef _WIN32
		#define WEBCAM_BACKEND cv::CAP_DSHOW
	#else
		#define WEBCAM_BACKEND cv::CAP_ANY
	#endif
#endif

double clamp(double value) {
	return std::max(0.0, std::min(1.0, value));
}

dlib::point dlib_rect_center(dlib::rectangle rect) {
	return (rect.tl_corner() + rect.br_corner())*.5;
}

void* dlib_thread_function(void* data)
{
	dlib_thread_data* dlib_data = (dlib_thread_data*)data;
	face_recognition_data* face_data = &dlib_data->face_data;
	
	// Import config
	int webcamWidth = -1;
	int webcamHeight = -1;
	int webcamFps = -1;
	bool webcamSetup = true;
	bool webcamYUYV = false;
	bool webcamLimitTo24or25 = false;
	std::string webcamPreferredName = "";
	int webcamPreferredId = -1;
	std::string webcamFormat = "";
	bool webcamSync = true;
	bool webcamSyncType2 = true;
	bool webcam_mouth_indirect = false;
	float webcamGamma = 1.0;
	int webcamBuffer = -1;
	
	INIReader reader(HEVA_CONFIG_PATH);
	//INIReader reader("/home/scaled/projects/Urho3D/VTuberFace2/DlibThread.ini");
	if (reader.ParseError() < 0) {
		printf("Can't load 'test.ini'\n");
	}
	else {
		printf("Ini loaded.\n");
		
		webcamPreferredName = reader.Get("webcam", "preferred_name", "");
		webcamPreferredId = reader.GetInteger("webcam", "preferred_id", -1);
		webcamFormat = reader.Get("webcam", "format", "");
		
		webcamSync = reader.GetBoolean("webcam", "sync", true);
		webcamSyncType2 = reader.GetBoolean("webcam", "sync_type2", true);
		webcam_mouth_indirect = reader.GetBoolean("webcam", "mouth_indirect", false);
		webcamGamma = reader.GetReal("webcam", "gamma", 1.0);
		webcamBuffer = reader.GetInteger("webcam", "buffer", -1);
		
		webcamSetup = reader.GetBoolean("webcam", "setup", true);
		webcamWidth = reader.GetInteger("webcam", "width", -1);
		webcamHeight = reader.GetInteger("webcam", "height", -1);
		webcamFps = reader.GetInteger("webcam", "fps", -1);
		webcamLimitTo24or25 = reader.GetBoolean("webcam", "limitTo24or25", false);
		webcamYUYV = reader.GetBoolean("webcam", "optimised_YUYV_conversion", false);
	}
	
	// Varriables
	cv::VideoCapture cap;
	dlib::frontal_face_detector detector;
	dlib::shape_predictor pose_model;
	
	std::vector<cv::Point3d> modelPoints;
	modelPoints.push_back(cv::Point3d(-0,		  0,		   0		 ));
	modelPoints.push_back(cv::Point3d(-0,		 -0.33,	   -0.06500001));
	modelPoints.push_back(cv::Point3d( 0.225,	  0.17,	   -0.135	 ));
	modelPoints.push_back(cv::Point3d(-0.225,	  0.17,	   -0.135	 ));
	modelPoints.push_back(cv::Point3d( 0.15,	  -0.15,	   -0.125	 ));
	modelPoints.push_back(cv::Point3d(-0.15,	  -0.15,	   -0.125	 ));

	dlib::correlation_tracker tracker;
	double tracker_reference_psr = -1;

	std::vector<cv::Point2d> imagePoints;
	for (int i = 0; i < 6; i++)
		imagePoints.push_back( cv::Point2d() );
	int shapeParts[] = {30, 8, 36, 45, 48, 54};
	dlib::full_object_detection shape;

	cv::Mat distCoeffs = cv::Mat::zeros(4,1,cv::DataType<double>::type);

	cv::Mat cameraMatrix;
	cv::Mat rotation_vector = cv::Mat::zeros(3,1,cv::DataType<double>::type);
	cv::Mat translation_vector = cv::Mat::zeros(3,1,cv::DataType<double>::type);
	cv::Mat rotation_vector_offset = cv::Mat::zeros(3,1,cv::DataType<double>::type);
	cv::Mat translation_vector_offset = cv::Mat::zeros(3,1,cv::DataType<double>::type);
	
	int facesLostCounter = 0;
	cv::Mat rotation_vector1 = cv::Mat::zeros(3,1,cv::DataType<double>::type);
	cv::Mat rotation_vector2 = cv::Mat::zeros(3,1,cv::DataType<double>::type);
	cv::Mat rotation_vector3 = cv::Mat::zeros(3,1,cv::DataType<double>::type);
	cv::Mat translation_vector1 = cv::Mat::zeros(3,1,cv::DataType<double>::type);
	cv::Mat translation_vector2 = cv::Mat::zeros(3,1,cv::DataType<double>::type);
	cv::Mat translation_vector3 = cv::Mat::zeros(3,1,cv::DataType<double>::type);
	
	double mth_a = 0;
	double mth_a_offset = 0;
	double mth_width = 0;
	double mth_width_offset = 0;
	double eye_l = 0;
	double eye_r = 0;
	double eye_l_offset = 0;
	double eye_r_offset = 0;
	double brow = 0;
	double brow_offset = 0;

	bool first = true;
	int firstCounter = 0;
	
	double adaptStrength = 1;
	
	dlib::point faceCenter(-1, -1);
	bool facesFound = false;
	bool faceTracked = false;
	
	/*double faceForms[] = {
		0.42, 0.0,	// Neutral
		0.68, 0.157,	// E
		0.538, 0.305,	// A
		0.76, 0.0,	// Fun
		0.085, 0.0	// U
	};*/
	double faceForms[] = {
		0.42, 0.0,	// Neutral
		0.54, 0.16,	// E
		0.42, 0.25,	// A
		0.54, 0.0,	// Fun
		0.32, 0.05	// U
	};
	double faceFormsMultiplier = 6;
	
	double nose_mouth_old = 0;
	double nose_mouth_old2 = 0;
	double nose_mouth_average = 0;
	double nose_mouth_distance = 0;
	
	// Load face detection and pose estimation models.
	detector = dlib::get_frontal_face_detector();
	#ifdef ALL_IN_ONE
	dlib::deserialize("shape_predictor_68_face_landmarks.dat") >> pose_model;
	#else
	dlib::deserialize("/usr/share/dlib/shape_predictor_68_face_landmarks.dat") >> pose_model;
	#endif
	
	// momentum_filter
	// Filter arguments:
	// measurement_noise
	// typical_acceleration
	// max_measurement_deviation
	
	/*dlib::momentum_filter filterMouthHeight = dlib::momentum_filter(0.003, 0.00001, 10.0);
	dlib::momentum_filter filterMouthWidth = dlib::momentum_filter(0.003, 0.00001, 10.0);
	dlib::momentum_filter filterBrow = dlib::momentum_filter(0.3, 0.0005, 7.0);*/
	
	dlib::momentum_filter filterMouthHeight = dlib::momentum_filter(2, 2, 3);
	dlib::momentum_filter filterMouthWidth = dlib::momentum_filter(2, 2, 3);
	dlib::momentum_filter filterBrow = dlib::momentum_filter(32, 1, 3);
	
	cv::Mat gammaCorrectionLUT(1, 256, CV_8U);
	if (webcamGamma != 1.0) {
		uchar* p = gammaCorrectionLUT.ptr();
		for( int i = 0; i < 256; ++i)
			p[i] = (unsigned char)(pow(i / 255.0, webcamGamma) * 255.0);
	}
	
	// Init
	// To reconnect camera
	while (dlib_data->dlib_thread_active) {
		// First priority webcam
		if (webcamPreferredName != "")
			cap = cv::VideoCapture(webcamPreferredName, WEBCAM_BACKEND);
		
		if (webcamPreferredId > 0 && !cap.isOpened())
			cap = cv::VideoCapture(webcamPreferredId, WEBCAM_BACKEND);
		
		// Generic webcam
		if (!cap.isOpened())
			for (int i = 0; i < 10; i++)
				if (!cap.isOpened())
					cap = cv::VideoCapture(i, WEBCAM_BACKEND);
				else
					break;
		
		
		// Necessary settings
		if (cap.isOpened())
		{
			if (webcamSetup) {
				if (webcamYUYV) {
					cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('Y', 'U', 'Y', 'V'));
					cap.set(cv::CAP_PROP_CONVERT_RGB, false);
				}
				else if (webcamFormat != "") {
					printf("format: %c %c %c %c\n", webcamFormat[0], webcamFormat[1], webcamFormat[2], webcamFormat[3]);
					cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc(webcamFormat[0], webcamFormat[1], webcamFormat[2], webcamFormat[3]));
				}
				
				if (webcamWidth > 0 && webcamHeight > 0) {
					cap.set(cv::CAP_PROP_FRAME_WIDTH, webcamWidth);
					cap.set(cv::CAP_PROP_FRAME_HEIGHT, webcamHeight);
				}
				
				if (webcamFps > 0) {
					int fps = cap.get(cv::CAP_PROP_FPS);
					
					cap.set(cv::CAP_PROP_FPS, webcamFps);
					
					if(!cap.isOpened())
						cap.set(cv::CAP_PROP_FPS, 30);
						
					if(!cap.isOpened())
						cap.set(cv::CAP_PROP_FPS, fps);
				}
				else if (webcamLimitTo24or25) {
					int fps = cap.get(cv::CAP_PROP_FPS);
					
					cap.set(cv::CAP_PROP_FPS, 25);
					
					if(!cap.isOpened())
						cap.set(cv::CAP_PROP_FPS, 24);
						
					if(!cap.isOpened())
						cap.set(cv::CAP_PROP_FPS, 30);
						
					if(!cap.isOpened())
						cap.set(cv::CAP_PROP_FPS, fps);
				}
				
				if (webcamBuffer != -1) {
					cap.set(cv::CAP_PROP_BUFFERSIZE, webcamBuffer);
				}
			}
			
			if (webcamWidth <= 0 || webcamHeight <= 0) {
				webcamWidth = cap.get(cv::CAP_PROP_FRAME_WIDTH);
				webcamHeight = cap.get(cv::CAP_PROP_FRAME_HEIGHT);
			}
			cameraMatrix = (
				cv::Mat_<double>(3,3) << 
				(float)webcamHeight, 0, (float)webcamWidth/2,
				0, (float)webcamHeight, (float)webcamHeight/2,
				0, 0, 1
			);
			printf("Resolution is %dx%d.\n", 
				webcamWidth,
				webcamHeight
			);
			
			dlib_data->dlib_thread_ready = 1;
			
			// Cycle
			while (dlib_data->dlib_thread_active) {
				// Handle dignals
				if (dlib_data->dlib_thread_signal == DLIB_THREAD_SIGNAL_RESET) {
					first = true;
					firstCounter = 0;
				}
				
				dlib_data->dlib_thread_signal = DLIB_THREAD_SIGNAL_NONE;
				
				// Do nothing for now, could be extended to eg. animate the display
				cv::Mat temp;
				//if (cap.read(temp)) {
				if (cap.grab()) {
					// Send sync signal to main process (before recodnition to sync with camera)
					if (webcamSync && webcamSyncType2)
						pthread_cond_signal(&dlib_data->dlib_thread_cond);
					
					cap.retrieve(temp);
					
					// translate to grayscale
					cv::Mat mat;
					cv::cvtColor(temp, mat, webcamYUYV ? cv::COLOR_YUV2GRAY_YUY2 : cv::COLOR_BGR2GRAY);
					
					if (webcamGamma != 1.0) {
						LUT(mat, gammaCorrectionLUT, mat);
					}
					
					dlib::cv_image<unsigned char> cimg(mat);
					
					if (first || !faceTracked) {
						std::vector<dlib::rectangle> faces = detector(cimg);
						if (faces.size() > 0) {
							facesFound = true;
							
							//printf("length: %f\n", (faceCenter - dlib_rect_center(faces[0])).length());
							
							int faceId = 0;
							if (faces.size() > 1) {
								printf("few faces detected:%d\n", faces.size());
								
								float length = (faceCenter - dlib_rect_center(faces[0])).length();
								
								for (int i = 1; i < faces.size(); i++) {
									float length2 = (faceCenter - dlib_rect_center(faces[i])).length();
									
									if (length2 < length) {
										length = length2;
										faceId = i;
									}
								}
							}
							
							shape = pose_model(cimg, faces[faceId]);
							faceCenter = dlib_rect_center(faces[faceId]);
							
							#ifndef TRACKING_STRAIT
							tracker.start_track(cimg, faces[faceId]);
							faceTracked = true;
							tracker_reference_psr = -1;
							#endif
						}
						else {
							facesFound = false;
						}
					}
					
					#ifndef TRACKING_STRAIT
					else if (faceTracked) {
						double psr = tracker.update(cimg);
						//double psr = tracker.update_noscale(cimg);
						
						if (tracker_reference_psr == -1)
							tracker_reference_psr = psr;
						
						tracker_reference_psr += 0.01 * (psr - tracker_reference_psr);
						
						//printf("%f\n", psr / tracker_reference_psr);
						
						//if (psr > 0.7*tracker_reference_psr) {
						if (psr > 0.8*tracker_reference_psr) {
							shape = pose_model(cimg, tracker.get_position());
							faceCenter = dlib_rect_center(tracker.get_position());
							#ifdef TRACKING_LOG
							fputc('|', stdout);
							#endif
						}
						//else if (psr > 0.5*tracker_reference_psr) {
						else if (psr > 0.5*tracker_reference_psr) {
							shape = pose_model(cimg, tracker.get_position());
							faceCenter = dlib_rect_center(tracker.get_position());
							faceTracked = false;
							#ifdef TRACKING_LOG
							fputs(" - update reference\n", stdout);
							#endif
						}
						else {
							faceTracked = false;
							facesFound = false;
							#ifdef TRACKING_LOG
							fputs(" - tracking lost\n", stdout);
							#endif
						}
					}
					#endif
					
					// Find the pose of each face.
					if (facesFound) {
						if (first) {
							for (int i = 0; i < 6; i++) {
								imagePoints[i].x = shape.part(shapeParts[i]).x();
								imagePoints[i].y = shape.part(shapeParts[i]).y();
							}
						}
						else {
							for (int i = 0; i < 6; i++) {
								double speed_x = shape.part(shapeParts[i]).x() - imagePoints[i].x;
								double speed_y = shape.part(shapeParts[i]).y() - imagePoints[i].y;
								double speed_length = sqrt(speed_x*speed_x + speed_y*speed_y);
								
								imagePoints[i].x += POINTS_SMOOTHING * speed_x * std::min(1.0, speed_length / POINTS_SMOOTHING_DISTANCE);
								imagePoints[i].y += POINTS_SMOOTHING * speed_y * std::min(1.0, speed_length / POINTS_SMOOTHING_DISTANCE);
							}
						}
						
						cv::solvePnP(modelPoints, imagePoints, cameraMatrix, distCoeffs, rotation_vector, translation_vector, first, cv::SOLVEPNP_ITERATIVE);
						//printf("%lf\n", adaptStrength);
						
						/*face_data->rotation1 = std::max(-90.0, std::min(90.0, face_data->rotation1));
						face_data->rotation2 = std::max(-90.0, std::min(90.0, face_data->rotation2));
						face_data->rotation3 = std::max(-90.0, std::min(90.0, face_data->rotation3));*/
						
						//printf("%f\n", translation_vector.at<double>(2));
						//printf("%f, %f, %f\n", face_data->translation1, face_data->translation2, face_data->translation3);
						//printf("%f, %f, %f\n", face_data->rotation1, face_data->rotation2, face_data->rotation3);
						
						if (
							translation_vector.at<double>(2) > 0 
							|| (translation_vector.at<double>(2) / translation_vector1.at<double>(2)) > 1.1
							|| (translation_vector1.at<double>(2) / translation_vector.at<double>(2)) > 1.1
							/*|| fabs(rotation_vector.at<double>(0))*180/3.1415 > 90
							|| fabs(rotation_vector.at<double>(1))*180/3.1415 > 90
							|| fabs(rotation_vector.at<double>(2))*180/3.1415 > 90*/
						) {
							#ifdef TRACKING_LOG
							fputs("- Face data looks strange\n", stdout);
							#endif
							faceTracked = false;
							facesFound = false;
							facesLostCounter = 10;
							translation_vector = translation_vector1;
							rotation_vector = rotation_vector1;
						}
					}
					
					if (facesFound) {
						if (first) {
							adaptStrength = 1;
							rotation_vector.copyTo(rotation_vector_offset);
							translation_vector.copyTo(translation_vector_offset);
							
							translation_vector1 = translation_vector;
							translation_vector2 = translation_vector;
							rotation_vector1 = rotation_vector;
							rotation_vector2 = rotation_vector;
						}
						else {
							translation_vector2 = translation_vector1;
							translation_vector1 = translation_vector;
							rotation_vector2 = rotation_vector1;
							rotation_vector1 = rotation_vector;
							
							adaptStrength *= ADAPT_STRENGTH_SPEED;
							rotation_vector_offset += (rotation_vector - rotation_vector_offset) * adaptStrength * ADAPT_SPEED_1;
							translation_vector_offset += (translation_vector - translation_vector_offset) * adaptStrength * ADAPT_SPEED_1;
						}
						facesLostCounter = 0;
						
						face_data->rotation1 = -(rotation_vector.at<double>(0) - rotation_vector_offset.at<double>(0))*180/3.1415;
						face_data->rotation2 = -(rotation_vector.at<double>(2) - rotation_vector_offset.at<double>(2))*180/3.1415;
						face_data->rotation3 = -(rotation_vector.at<double>(1) - rotation_vector_offset.at<double>(1))*180/3.1415;
						
						face_data->translation1 = translation_vector.at<double>(0)-translation_vector_offset.at<double>(0);
						face_data->translation2 = translation_vector.at<double>(1)-translation_vector_offset.at<double>(1);
						face_data->translation3 = translation_vector.at<double>(2)-translation_vector_offset.at<double>(2);
						
						
						// Face
						double distance_mul_2 = translation_vector.at<double>(2) * (-240.0 / webcamHeight);
						
						
						// Mouth
						double mth_width_input = dlib::length(shape.part(54) - shape.part(48)) * distance_mul_2 * 0.05;
						double mth_height_input = dlib::length(shape.part(65) + shape.part(66) + shape.part(67) - shape.part(61) - shape.part(62) - shape.part(63)) * distance_mul_2 * 0.025;
						
						// moustache detected as mouth mitigation
						if (webcam_mouth_indirect) {
							//double nose_mouth = dlib::length(shape.part(31) - shape.part(50)  +  shape.part(35) - shape.part(52)) * distance_mul_2;
							//double nose_mouth = dlib::length(shape.part(31) - shape.part(61)  +  shape.part(35) - shape.part(63)) * 0.5 * distance_mul_2;
							double nose_mouth = dlib::length(shape.part(31) - shape.part(67)  +  shape.part(35) - shape.part(65)) * 0.5 * distance_mul_2;
							
							if (first) {
								nose_mouth_average = nose_mouth;
								nose_mouth_distance = nose_mouth;
							}
							else {
							
								// detect low peak
								// 0 1 2 
								// 0 is now, 1 is 1 frame old, 2 is 2 frames old
								// 0 is HIGH, 1 is LOW, 2 is HIGH it mean peak
								// Also it must be lover than average
								
								nose_mouth_average += adaptStrength * ADAPT_SPEED_4 * (nose_mouth - nose_mouth_average);
								
								if (
									 nose_mouth_old < nose_mouth_average && 
									 nose_mouth_old < nose_mouth && 
									 nose_mouth_old < nose_mouth_old2
								)
								{
									nose_mouth_distance += adaptStrength * ADAPT_SPEED_4 * ( nose_mouth - nose_mouth_distance);
								}
								
								 nose_mouth_old2 =  nose_mouth_old;
								 nose_mouth_old =  nose_mouth;
							}
							
							double mth_height_indirect = (dlib::length( (shape.part(31) + shape.part(35)) * 1.5 - shape.part(65) - shape.part(66) - shape.part(67)) / 3 * distance_mul_2 - nose_mouth_distance) * 0.075  * 0.9 + 0.15;
							
							//printf("%f\t%f\n", mth_height_input, mth_height_indirect);
							
							/*double mth_height_indirect = (dlib::length( (shape.part(31) + shape.part(35)) * 1.5 - shape.part(65) - shape.part(66) - shape.part(67)) / 3 * distance_mul_2) * 0.075;
							
							if (first) {
								nose_mouth_average = mth_height_indirect;
								mth_height_indirect = 0;
							}
							else {
								mth_height_indirect -= nose_mouth_average;
								nose_mouth_average += adaptStrength * ADAPT_SPEED_2 * mth_height_indirect;
								mth_height_indirect += 0.1;
							}*/
							
							// Mix with indirect
							// Lowest
							/*if (mth_height_input > mth_height_indirect)
								mth_height_input = mth_height_indirect;*/
							
							// 50% - 50%
							//mth_height_input = (mth_height_input + mth_height_indirect) * 0.5;
							
							// 100% indirect
							mth_height_input = mth_height_indirect;
						}
						
						if (first) {
							faceFormsMultiplier = mth_width_input / faceForms[FACE_FORM_NEUTRAL * 2 + 0];
						}
						else {
							faceFormsMultiplier += adaptStrength * ADAPT_SPEED_3 * (mth_width_input / faceForms[FACE_FORM_NEUTRAL * 2 + 0] - faceFormsMultiplier);
						}
						mth_width_input = filterMouthWidth(mth_width_input);
						mth_height_input = filterMouthHeight(mth_height_input);
						//printf("%f\n", mth_height_input);
						
						double height,width;
						
						width =  mth_width_input - faceForms[FACE_FORM_NEUTRAL * 2 + 0] * faceFormsMultiplier;
						height = mth_height_input - faceForms[FACE_FORM_NEUTRAL * 2 + 1] * faceFormsMultiplier;
						double faceForm_Neutral = 1 / std::max(0.01, width*width + height*height);
						
						width =  std::min(0.0, mth_width_input - faceForms[FACE_FORM_E * 2 + 0] * faceFormsMultiplier);
						height = mth_height_input - faceForms[FACE_FORM_E * 2 + 1] * faceFormsMultiplier;
						double faceForm_E = 1 / std::max(0.01, width*width + height*height);
						
						width =  mth_width_input - faceForms[FACE_FORM_A * 2 + 0] * faceFormsMultiplier;
						height = std::min(0.0, mth_height_input - faceForms[FACE_FORM_A * 2 + 1] * faceFormsMultiplier);
						double faceForm_A = 1 / std::max(0.01, width*width + height*height);
						
						width =  std::min(0.0, mth_width_input - faceForms[FACE_FORM_Fun * 2 + 0] * faceFormsMultiplier);
						height = mth_height_input - faceForms[FACE_FORM_Fun * 2 + 1] * faceFormsMultiplier;
						double faceForm_Fun = 1 / std::max(0.01, width*width + height*height);
						
						width =  std::max(0.0, mth_width_input - faceForms[FACE_FORM_U * 2 + 0] * faceFormsMultiplier);
						height = mth_height_input - faceForms[FACE_FORM_U * 2 + 1] * faceFormsMultiplier;
						double faceForm_U = 1 / std::max(0.01, width*width + height*height);
						
						double faceForm_Sum = faceForm_Neutral + faceForm_E + faceForm_A + faceForm_Fun + faceForm_U;
						
						face_data->MTH_E = faceForm_E / faceForm_Sum;
						face_data->MTH_A = faceForm_A / faceForm_Sum;
						face_data->MTH_Fun = faceForm_Fun / faceForm_Sum;
						face_data->MTH_U = faceForm_U / faceForm_Sum;
						//printf("faceForm_Neutral = %lf\nfaceForm_E = %lf\nfaceForm_A = %lf\nfaceForm_Fun = %lf\nfaceForm_U = %lf\nfaceForm_Sum = %lf\n",
						//	faceForm_Neutral, faceForm_E, faceForm_A, faceForm_Fun, faceForm_U, faceForm_Sum);
						
						// Brows
						double brow_input = dlib::length(shape.part(19) + shape.part(24) - shape.part(27)*2) * distance_mul_2 * 0.4;
						if (first) {
							brow = brow_input;
							brow_offset = brow;
						}
						else {
							brow = filterBrow(brow_input);
							brow_offset += adaptStrength * ADAPT_SPEED_2 * (brow - brow_offset);
						}
						
						face_data->BRW_Fun = clamp((brow - brow_offset)*.2);
						face_data->BRW_Angry = clamp(-(brow - brow_offset)*.2);
						
						
						double eye_l_input = dlib::length(shape.part(41) + shape.part(40) - shape.part(38) - shape.part(37)) * distance_mul_2 * -1.1;
						double eye_r_input = dlib::length(shape.part(44) + shape.part(43) - shape.part(46) - shape.part(47)) * distance_mul_2 * -1.1;
						if (first) {
							eye_l = eye_l_input;
							eye_r = eye_r_input;
							eye_l_offset = eye_l_input;
							eye_r_offset = eye_r_input;
						}
						else {
							eye_l += .3 * (eye_l_input - eye_l) * std::min(1.0, abs(eye_l_input - eye_l)*.15);
							eye_r += .3 * (eye_r_input - eye_r) * std::min(1.0, abs(eye_r_input - eye_r)*.15);
							eye_l_offset += adaptStrength * ADAPT_SPEED_2 * (eye_l - eye_l_offset);
							eye_r_offset += adaptStrength * ADAPT_SPEED_2 * (eye_r - eye_r_offset);
						}
						
						double eye_sum = (eye_l - eye_l_offset + eye_r - eye_r_offset) * .5;
						//*EYE_Close_L = pow(clamp((eye_sum) * .4 - .1), .75);
						//*EYE_Close_R = pow(clamp((eye_sum) * .4 - .1), .75);
						face_data->EYE_Close_L = clamp((eye_sum) * .4 - .05);
						face_data->EYE_Close_R = clamp((eye_sum) * .4 - .05);
						
						if (first) {
							first = false;
						}
						firstCounter++;
						
						#ifdef TRACKING_DISPLAY
						win.clear_overlay();
						win.set_image(cimg);
						win.add_overlay(render_face_detections(shape));
						#endif
					}
					// if faces not found
					else {
						facesLostCounter++;
						
						if (facesLostCounter <= 2) {
							//printf("%d\n", facesLostCounter);
							translation_vector3 = translation_vector2;
							translation_vector2 = translation_vector1;
							translation_vector1 = translation_vector;
							rotation_vector3 = rotation_vector2;
							rotation_vector2 = rotation_vector1;
							rotation_vector1 = rotation_vector;
							
							// predicted pos = pos_old + velocity + acceleration
							// predicted pos = pos_old + velocity + (velocity - velocity_old)
							if (facesLostCounter == 1) {
								translation_vector = translation_vector1 
									+ (translation_vector1 - translation_vector2) 
									+ ( (translation_vector1 - translation_vector2) - (translation_vector2 - translation_vector3) );
								rotation_vector = rotation_vector1 
									+ (rotation_vector1 - rotation_vector2) 
									+ ( (rotation_vector1 - rotation_vector2) - (rotation_vector2 - rotation_vector3) );
							}
							else {
								translation_vector = translation_vector1 
									+ (translation_vector1 - translation_vector2);
								rotation_vector = rotation_vector1 
									+ (rotation_vector1 - rotation_vector2);
							}
							
							face_data->rotation1 = -(rotation_vector.at<double>(0) - rotation_vector_offset.at<double>(0))*180/3.1415;
							face_data->rotation2 = -(rotation_vector.at<double>(2) - rotation_vector_offset.at<double>(2))*180/3.1415;
							face_data->rotation3 = -(rotation_vector.at<double>(1) - rotation_vector_offset.at<double>(1))*180/3.1415;
							
							face_data->translation1 = translation_vector.at<double>(0)-translation_vector_offset.at<double>(0);
							face_data->translation2 = translation_vector.at<double>(1)-translation_vector_offset.at<double>(1);
							face_data->translation3 = translation_vector.at<double>(2)-translation_vector_offset.at<double>(2);
						}
						else {
							face_data->MTH_E *= 0.9;
							face_data->MTH_A *= 0.9;
							face_data->MTH_Fun *= 0.9;
							face_data->MTH_U *= 0.9;
							face_data->BRW_Fun *= 0.9;
							face_data->BRW_Angry *= 0.9;
							face_data->EYE_Close_L *= 0.9;
							face_data->EYE_Close_R *= 0.9;
							
							// return when inactive 10 seconds
							if (facesLostCounter > 250) {
								face_data->rotation1 *= 0.99;
								face_data->rotation2 *= 0.99;
								face_data->rotation3 *= 0.99;
							
								face_data->translation1 *= 0.99;
								face_data->translation2 *= 0.99;
								face_data->translation3 *= 0.99;
							}
						}
					}
					// Send sync signal to main process (after recodnition to reduce latency)
					if (webcamSync && !webcamSyncType2)
						pthread_cond_signal(&dlib_data->dlib_thread_cond);
				}
				else {
					break;
				}
			}
			
			dlib_data->dlib_thread_ready = 0;
			pthread_cond_signal(&dlib_data->dlib_thread_cond);
			cap.release();
		}
		if (!first)
			dlib::sleep(2000);
		else
			break;
	}
	
	return NULL;
}
