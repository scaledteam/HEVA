//
// Created by scaled
//

//#define TRACKING_LOG
//#define TRACKING_STRAIT
//#define TRACKING_THREADING_LOG

#define POINTS_SMOOTHING .125
#define POINTS_SMOOTHING_DISTANCE 24

#define ADAPT_SPEED_1 .0025
#define ADAPT_SPEED_2 .005
#define ADAPT_SPEED_3 .01
#define ADAPT_SPEED_4 .1
#define ADAPT_STRENGTH_SPEED .9998

#include "face_recognition.h"

#include <dlib/image_processing.h>
#include <dlib/image_processing/frontal_face_detector.h>

#define FACE_FORM_NEUTRAL 0
#define FACE_FORM_E 1
#define FACE_FORM_A 2
#define FACE_FORM_Fun 3
#define FACE_FORM_U 4

#include <dlib/filtering.h>

pthread_mutex_t dlib_thread2_mutex_cimg = PTHREAD_MUTEX_INITIALIZER;

dlib::array2d<unsigned char> cimg;
#include "capture-v4l2.c"


double clamp(double value) { return std::max(0.0, std::min(1.0, value)); }

dlib::point dlib_rect_center(dlib::rectangle rect) {
  return (rect.tl_corner() + rect.br_corner()) * .5;
}

void *dlib_thread2_function(void *data) {
  dlib_thread_data *dlib_data = (dlib_thread_data *)data;

  dlib_data->faceCenter = dlib::point(-1, -1);
  dlib_data->tracker_reference_psr = -1;
  dlib_data->facesFound = false;
  dlib_data->faceTracked = false;
  dlib_data->tracker = dlib::correlation_tracker();

  // Load face detection and pose estimation models.
  dlib::frontal_face_detector detector;
  detector = dlib::get_frontal_face_detector();

  pthread_mutex_t dlib_thread2_mutex2 = PTHREAD_MUTEX_INITIALIZER;

  while (dlib_data->dlib_thread_active) {
    // wait until signal is got
    // printf("  -- 2 wait until signal is got\n");

    if (dlib_data->thread1_waiting) {
      pthread_cond_signal(&dlib_data->dlib_thread2_cond1);
      #ifdef TRACKING_THREADING_LOG
      printf("  !! 2 thread is ready, but 1 still waiting\n");
      #endif
      dlib::sleep(1000/60);
      continue;
    }

    pthread_mutex_lock(&dlib_thread2_mutex2);
    pthread_cond_wait(&dlib_data->dlib_thread2_cond2, &dlib_thread2_mutex2);
    pthread_mutex_unlock(&dlib_thread2_mutex2);

    pthread_mutex_lock(&dlib_thread2_mutex_cimg);
    #ifdef TRACKING_THREADING_LOG
    printf("  -- 2 mutex locked\n");
    #endif
    
    std::vector<dlib::rectangle> faces = detector(cimg);
    
    if (faces.size() > 0) {
      dlib_data->facesFound = true;

      // printf("length: %f\n", (dlib_data->faceCenter -
      // dlib_rect_center(faces[0])).length());

      int faceId = 0;
      if (faces.size() > 1) {
        printf("few faces detected:%d\n", faces.size());

        float length =
            (dlib_data->faceCenter - dlib_rect_center(faces[0])).length();

        for (int i = 1; i < faces.size(); i++) {
          float length2 =
              (dlib_data->faceCenter - dlib_rect_center(faces[i])).length();

          if (length2 < length) {
            length = length2;
            faceId = i;
          }
        }
      }

      dlib_data->faceRect = faces[faceId];
      dlib_data->faceCenter = dlib_rect_center(faces[faceId]);

      // Start tracking face in parallel
      #ifndef TRACKING_STRAIT
      dlib_data->tracker.start_track(cimg, faces[faceId]);
      dlib_data->faceTracked = true;
      dlib_data->tracker_reference_psr = -1;
      #endif
      
      #ifdef TRACKING_THREADING_LOG
      printf("1 <- 2 faces found\n");
      #endif
      pthread_cond_signal(&dlib_data->dlib_thread2_cond1);
    } else {
      dlib_data->facesFound = false;
      #ifdef TRACKING_THREADING_LOG
      printf("1 <- 2 faces not found\n");
      #endif
      pthread_cond_signal(&dlib_data->dlib_thread2_cond1);
    }
    pthread_mutex_unlock(&dlib_thread2_mutex_cimg);
    #ifdef TRACKING_THREADING_LOG
    printf("  -- 2 mutex unlocked\n");
    #endif

    // signal to make sure it's not stuck
    // printf("1 <- 2 send proper signal to continue working\n");
    // pthread_cond_signal(&dlib_data->dlib_thread2_cond1);
  }

  // printf("     2 thread stopped !!!!!!!!!!!!!!!!!!!!!!!!!!\n");

  return NULL;
}

void *dlib_thread1_function(void *data) {
  dlib_thread_data *dlib_data = (dlib_thread_data *)data;
  face_recognition_data *face_data = &dlib_data->face_data;
  webcam_settings_t *webcam_settings = dlib_data->webcam_settings;

  // Varriables
  dlib::shape_predictor pose_model;
  dlib::deserialize(webcam_settings->shapePredictorPath) >> pose_model;
  dlib::full_object_detection shape;

  pthread_mutex_t dlib_thread2_mutex1 = PTHREAD_MUTEX_INITIALIZER;
  dlib_data->thread1_waiting = false;

  dlib::vector rotation_vector = dlib::vector<double,3>();
  dlib::vector translation_vector = dlib::vector<double,3>();
  dlib::vector rotation_vector_offset = dlib::vector<double,3>();
  dlib::vector translation_vector_offset = dlib::vector<double,3>();

  int facesLostCounter = 0;
  dlib::vector rotation_vector1 = dlib::vector<double,3>();
  dlib::vector rotation_vector2 = dlib::vector<double,3>();
  dlib::vector rotation_vector3 = dlib::vector<double,3>();
  dlib::vector translation_vector1 = dlib::vector<double,3>();
  dlib::vector translation_vector2 = dlib::vector<double,3>();
  dlib::vector translation_vector3 = dlib::vector<double,3>();

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
  face_data->on_screen = true;


  /*double faceForms[] = {
          0.42, 0.0,	// Neutral
          0.54, 0.16,	// E
          0.42, 0.25,	// A
          0.54, 0.0,	// Fun
          0.32, 0.05	// U
  };*/

  double faceForms[] = {0.377549, 0.023321, 0.429809, 0.051127, 0.397514,
                        0.151053, 0.454052, 0.007649, 0.371302, 0.108068};

  double faceFormsMultiplier = 6;

  double nose_mouth_old = 0;
  double nose_mouth_old2 = 0;
  double nose_mouth_average = 0;
  double nose_mouth_distance = 0;
  
  dlib::point face_nose;      // 30
  dlib::point face_jaw;       // 8
  dlib::point face_cheek_l;   // 0
  dlib::point face_cheek_r;   // 16

  // momentum_filter
  // Filter arguments:
  // 	measurement_noise
  // 	typical_acceleration
  // 	max_measurement_deviation

  dlib::momentum_filter filterMouthHeight = dlib::momentum_filter(2, 2, 3);
  dlib::momentum_filter filterMouthWidth = dlib::momentum_filter(2, 2, 3);
  dlib::momentum_filter filterBrow = dlib::momentum_filter(32, 1, 3);


  // Init
  // To reconnect camera
  while (dlib_data->dlib_thread_active) {
    // First priority webcam
    /*
    if (webcam_settings->PreferredName != "")
      cap = cv::VideoCapture(webcam_settings->PreferredName, WEBCAM_BACKEND);

    if (webcam_settings->PreferredId > 0 && !cap.isOpened())
      cap = cv::VideoCapture(webcam_settings->PreferredId, WEBCAM_BACKEND);

    // Generic webcam_settings->
    if (!cap.isOpened())
      for (int i = 0; i < 10; i++)
        if (!cap.isOpened())
          cap = cv::VideoCapture(i, WEBCAM_BACKEND);
        else
          break;
    */

    //cimg = dlib::array2d<unsigned char>(webcam_settings->Height, webcam_settings->Width);
    
    if (webcam_settings->PreferredName != "") {
      init_device(webcam_settings->PreferredName, webcam_settings->Width, webcam_settings->Height);
      if (capture_status()) {
        cimg.set_size(webcam_settings->Height, webcam_settings->Width);
      }
    }
    
    if (!capture_status()) {
      char default_video_source[] = "/dev/video0";
      
      init_device(default_video_source, 640, 480);
      cimg.set_size(480, 640);
    }

    start_capturing();

    // Necessary settings
    //if (cap.isOpened()) {
    if (1) {
      dlib_data->dlib_thread_ready = 1;

      // Cycle
      while (dlib_data->dlib_thread_active) {
        // Handle dignals
        if (dlib_data->dlib_thread_signal == DLIB_THREAD_SIGNAL_RESET) {
          first = true;
          firstCounter = 0;
          dlib_data->dlib_thread_signal = DLIB_THREAD_SIGNAL_NONE;
        }

        //cv::Mat temp;
        //if (cap.grab()) {
        if (1) {
          // Send sync signal to main process (before recodnition to sync with camera)
          if (webcam_settings->Sync)
            pthread_cond_signal(&dlib_data->dlib_thread_cond);

          //cimg = dlib::cv_image<unsigned char>(mat);
          capture_frame(data);
          
          if (!capture_status()) {
            //capture_cleanup();
            dlib::sleep(2000);
            
            init_device(webcam_settings->PreferredName);
            if (capture_status())
              start_capturing();
            continue;
          }

          if (first || !dlib_data->faceTracked) {
            // send signal to work
            #ifdef TRACKING_THREADING_LOG
            printf("1 -> 2 send signal to work\n");
            #endif
            pthread_cond_signal(&dlib_data->dlib_thread2_cond2);

            // wait until face detected
            pthread_mutex_lock(&dlib_thread2_mutex1);
            dlib_data->thread1_waiting = true;
            pthread_cond_wait(&dlib_data->dlib_thread2_cond1,
                              &dlib_thread2_mutex1);
            dlib_data->thread1_waiting = false;
            pthread_mutex_unlock(&dlib_thread2_mutex1);

            // get shape
            if (dlib_data->facesFound)
              shape = pose_model(cimg, dlib_data->faceRect);
          }

          #ifndef TRACKING_STRAIT
          else if (dlib_data->faceTracked) {
            double psr = dlib_data->tracker.update(cimg);

            if (dlib_data->tracker_reference_psr == -1)
              dlib_data->tracker_reference_psr = psr;

            dlib_data->tracker_reference_psr +=
                0.01 * (psr - dlib_data->tracker_reference_psr);

            // printf("%f\n", psr / dlib_data->tracker_reference_psr);

            if (psr > 0.8 * dlib_data->tracker_reference_psr) {
              shape = pose_model(cimg,
                                 dlib_data->tracker.get_position());
              dlib_data->faceCenter =
                  dlib_rect_center(dlib_data->tracker.get_position());
              #ifdef TRACKING_LOG
              fputc('|', stdout);
              #endif
            } else if (psr > 0.5 * dlib_data->tracker_reference_psr) {
              shape = pose_model(cimg,
                                 dlib_data->tracker.get_position());
              dlib_data->faceCenter =
                  dlib_rect_center(dlib_data->tracker.get_position());
              dlib_data->faceTracked = false;
              #ifdef TRACKING_LOG
              fputs(" - update reference\n", stdout);
              #endif

              // ask to track in parallel
              #ifdef TRACKING_THREADING_LOG
              printf("1 -> 2 ask to track in parallel\n");
              #endif
              pthread_cond_signal(&dlib_data->dlib_thread2_cond2);
            } else {
              dlib_data->faceTracked = false;
              dlib_data->facesFound = false;
              #ifdef TRACKING_LOG
              fputs(" - tracking lost\n", stdout);
              #endif

              // ask to track in parallel
              #ifdef TRACKING_THREADING_LOG
              printf("1 -> 2 ask to track in parallel\n");
              #endif
              pthread_cond_signal(&dlib_data->dlib_thread2_cond2);
            }
          }
          #endif

          if (shape.num_parts() > 0) {
            if (first) {
              face_nose = shape.part(30);
              face_jaw = shape.part(8);
              face_cheek_l = shape.part(0);
              face_cheek_r = shape.part(16);
            }
            else {
              dlib::point speed = shape.part(30) - face_nose;
              face_nose(0) += POINTS_SMOOTHING * speed.x() * std::min(1.0, dlib::length(speed) / POINTS_SMOOTHING_DISTANCE);
              face_nose(1) += POINTS_SMOOTHING * speed.y() * std::min(1.0, dlib::length(speed) / POINTS_SMOOTHING_DISTANCE);
              speed = shape.part(8) - face_jaw;
              face_jaw(0) += POINTS_SMOOTHING * speed.x() * std::min(1.0, dlib::length(speed) / POINTS_SMOOTHING_DISTANCE);
              face_jaw(1) += POINTS_SMOOTHING * speed.y() * std::min(1.0, dlib::length(speed) / POINTS_SMOOTHING_DISTANCE);
              speed = shape.part(0) - face_cheek_l;
              face_cheek_l(0) += POINTS_SMOOTHING * speed.x() * std::min(1.0, dlib::length(speed) / POINTS_SMOOTHING_DISTANCE);
              face_cheek_l(1) += POINTS_SMOOTHING * speed.y() * std::min(1.0, dlib::length(speed) / POINTS_SMOOTHING_DISTANCE);
              speed = shape.part(16) - face_cheek_r;
              face_cheek_r(0) += POINTS_SMOOTHING * speed.x() * std::min(1.0, dlib::length(speed) / POINTS_SMOOTHING_DISTANCE);
              face_cheek_r(1) += POINTS_SMOOTHING * speed.y() * std::min(1.0, dlib::length(speed) / POINTS_SMOOTHING_DISTANCE);
            }
            
            // Find the pose of each face.
            dlib::point face_tilt = face_cheek_r - face_cheek_l;
            
            double distance = -0.7 * webcam_settings->Height /
                              dlib::length(face_tilt);
            translation_vector(2) = distance;
            translation_vector(0) =
                (double)face_nose.x() / webcam_settings->Height * distance;
            translation_vector(1) =
                (double)face_nose.y() / webcam_settings->Height * distance;
            //printf("%f\t%f\t%f\n", translation_vector.x(), translation_vector.y(), translation_vector.z());

            // tilt
            rotation_vector(2) = atan2(face_tilt.y(), face_tilt.x());

            // left-right
            rotation_vector(1) =
                5 *
                (dlib::length(face_jaw - face_cheek_r) -
                 dlib::length(face_cheek_l - face_jaw)) /
                webcam_settings->Height;

            // up-down
            //rotation_vector(0) = (dlib::length(face_tilt) / (dlib::length(face_cheek_l - face_jaw) + dlib::length(face_cheek_r - face_jaw)));
            rotation_vector(0) = -7 * ((dlib::length(face_tilt) - 0.5 * (dlib::length(face_cheek_l - face_jaw) + dlib::length(face_cheek_r - face_jaw))) / webcam_settings->Height - 0.05);

            //printf("%f\t%f\t%f\n", rotation_vector.x(), rotation_vector.y(), rotation_vector.z());
          }
          if (dlib_data->facesFound) {
            if (first) {
              adaptStrength = 1;
              // i don't know why there is no vector copy
              for (int i=0; i < 3; i++) {
                rotation_vector_offset(i) = rotation_vector(i);
                translation_vector_offset(i) = translation_vector(i);

                translation_vector1(i) = translation_vector(i);
                translation_vector2(i) = translation_vector(i);
                //translation_vector3(i) = translation_vector(i);
                rotation_vector1(i) = rotation_vector(i);
                rotation_vector2(i) = rotation_vector(i);
                //rotation_vector3(i) = rotation_vector(i);
              }
            } else {
              for (int i=0; i < 3; i++) {
                //translation_vector3(i) = translation_vector2(i);
                translation_vector2(i) = translation_vector1(i);
                translation_vector1(i) = translation_vector(i);
                //rotation_vector3(i) = rotation_vector2(i);
                rotation_vector2(i) = rotation_vector1(i);
                rotation_vector1(i) = rotation_vector(i);
              }

              adaptStrength *= ADAPT_STRENGTH_SPEED;
              rotation_vector_offset +=
                  (rotation_vector - rotation_vector_offset) * adaptStrength *
                  ADAPT_SPEED_1;
              translation_vector_offset +=
                  (translation_vector - translation_vector_offset) *
                  adaptStrength * ADAPT_SPEED_1;
            }
            facesLostCounter = 0;
            face_data->on_screen = true;

            face_data->rotation1 = -(rotation_vector.x() -
                                     rotation_vector_offset.x()) *
                                   180 / 3.1415;
            face_data->rotation2 = -(rotation_vector.z() -
                                     rotation_vector_offset.z()) *
                                   180 / 3.1415;
            face_data->rotation3 = -(rotation_vector.y() -
                                     rotation_vector_offset.y()) *
                                   180 / 3.1415;
            
            dlib::vector translation_vector_send = translation_vector - translation_vector_offset;
            
            /*dlib::matrix<double> rotation_matrix1(3,3);
            rotation_matrix1 =  1.0, 0.0, 0.0,
                                0.0, 1.0, 0.0,
                                0.0, 0.0, 1.0;
            translation_vector_send = rotation_matrix1 * translation_vector_send;*/
            
            dlib::point_transform_affine3d transform_class;
            transform_class = dlib::rotate_around_x(-rotation_vector_offset.x());
            translation_vector_send = transform_class(translation_vector_send);
            transform_class = dlib::rotate_around_y(-rotation_vector_offset.y());
            translation_vector_send = transform_class(translation_vector_send);
            transform_class = dlib::rotate_around_z(-rotation_vector_offset.z());
            translation_vector_send = transform_class(translation_vector_send);
            
            face_data->translation1 = translation_vector_send.x();
            face_data->translation2 = translation_vector_send.y();
            face_data->translation3 = translation_vector_send.z();
            
            /*face_data->translation1 = translation_vector.x() -
                                      translation_vector_offset.x();
            face_data->translation2 = translation_vector.y() -
                                      translation_vector_offset.y();
            face_data->translation3 = translation_vector.z() -
                                      translation_vector_offset.z();*/

            // Face
            //double distance_mul_2 = translation_vector.z() * (-240.0 / webcam_settings->Height);
            double distance_mul_2 = 0.35 * webcam_settings->Height / dlib::length(shape.part(16) - shape.part(0));
            //printf("%f\t%f\n", distance_mul_2, 0.35 * webcam_settings->Height / dlib::length(shape.part(16) - shape.part(0)));

            // Mouth
            double mth_width_input =
                dlib::length(shape.part(54) - shape.part(48)) * distance_mul_2 *
                0.05;
            double mth_height_input =
                dlib::length(shape.part(65) + shape.part(66) + shape.part(67) -
                             shape.part(61) - shape.part(62) - shape.part(63)) *
                distance_mul_2 * 0.025;

            double mth_left_input =
                dlib::length((shape.part(52) + shape.part(58)) -
                             2 * shape.part(49)) *
                distance_mul_2;
            double mth_right_input =
                dlib::length((shape.part(52) + shape.part(58)) -
                             2 * shape.part(55)) *
                distance_mul_2;

            // moustache detected as mouth mitigation
            if (webcam_settings->MouthIndirect) {
              double nose_mouth =
                  dlib::length(shape.part(31) - shape.part(67) +
                               shape.part(35) - shape.part(65)) *
                  0.5 * distance_mul_2;

              if (first) {
                nose_mouth_average = nose_mouth;
                nose_mouth_distance = nose_mouth;
              } else {

                // detect low peak
                // 0 1 2
                // 0 is now, 1 is 1 frame old, 2 is 2 frames old
                // 0 is HIGH, 1 is LOW, 2 is HIGH it mean peak
                // Also it must be lover than average

                nose_mouth_average += adaptStrength * ADAPT_SPEED_4 *
                                      (nose_mouth - nose_mouth_average);

                if (nose_mouth_old < nose_mouth_average &&
                    nose_mouth_old < nose_mouth &&
                    nose_mouth_old < nose_mouth_old2) {
                  nose_mouth_distance += adaptStrength * ADAPT_SPEED_4 *
                                         (nose_mouth - nose_mouth_distance);
                }

                nose_mouth_old2 = nose_mouth_old;
                nose_mouth_old = nose_mouth;
              }

              double mth_height_indirect =
                  (dlib::length((shape.part(31) + shape.part(35)) * 1.5 -
                                shape.part(65) - shape.part(66) -
                                shape.part(67)) /
                       3 * distance_mul_2 -
                   nose_mouth_distance) *
                      0.075 * 0.9 +
                  0.15;

              // 100% indirect
              mth_height_input = mth_height_indirect;
            }

            if (first) {
              faceFormsMultiplier =
                  mth_width_input / faceForms[FACE_FORM_NEUTRAL * 2 + 0];
            } else {
              faceFormsMultiplier +=
                  adaptStrength * ADAPT_SPEED_3 *
                  (mth_width_input / faceForms[FACE_FORM_NEUTRAL * 2 + 0] -
                   faceFormsMultiplier);
            }
            mth_width_input = filterMouthWidth(mth_width_input);
            mth_height_input = filterMouthHeight(mth_height_input);

            double height, width;

            width = mth_width_input -
                    faceForms[FACE_FORM_NEUTRAL * 2 + 0] * faceFormsMultiplier;
            height = std::max(0.0, mth_height_input -
                                       faceForms[FACE_FORM_NEUTRAL * 2 + 1] *
                                           faceFormsMultiplier);
            double faceForm_Neutral =
                1 / std::max(0.01, width * width + height * height);

            width =
                std::min(0.0, mth_width_input - faceForms[FACE_FORM_E * 2 + 0] *
                                                    faceFormsMultiplier);
            height = mth_height_input -
                     faceForms[FACE_FORM_E * 2 + 1] * faceFormsMultiplier;
            double faceForm_E =
                1 / std::max(0.01, width * width + height * height);

            width = mth_width_input -
                    faceForms[FACE_FORM_A * 2 + 0] * faceFormsMultiplier;
            height = std::min(0.0, mth_height_input -
                                       faceForms[FACE_FORM_A * 2 + 1] *
                                           faceFormsMultiplier);
            double faceForm_A =
                1 / std::max(0.01, width * width + height * height);

            width = std::min(0.0, mth_width_input -
                                      faceForms[FACE_FORM_Fun * 2 + 0] *
                                          faceFormsMultiplier);
            height = mth_height_input -
                     faceForms[FACE_FORM_Fun * 2 + 1] * faceFormsMultiplier;
            double faceForm_Fun =
                1 / std::max(0.01, width * width + height * height);

            width =
                std::max(0.0, mth_width_input - faceForms[FACE_FORM_U * 2 + 0] *
                                                    faceFormsMultiplier);
            height = mth_height_input -
                     faceForms[FACE_FORM_U * 2 + 1] * faceFormsMultiplier;
            double faceForm_U =
                1 / std::max(0.01, width * width + height * height);

            double faceForm_Sum = faceForm_Neutral + faceForm_E + faceForm_A +
                                  faceForm_Fun + faceForm_U;

            face_data->MTH_E = faceForm_E / faceForm_Sum;
            face_data->MTH_A = faceForm_A / faceForm_Sum;
            face_data->MTH_Fun = faceForm_Fun / faceForm_Sum;
            face_data->MTH_U = faceForm_U / faceForm_Sum;

            // Calibrate
            if (dlib_data->dlib_thread_signal ==
                DLIB_THREAD_SIGNAL_CALIBRATE_MTH_Neutral) {
              faceForms[FACE_FORM_NEUTRAL * 2 + 0] =
                  mth_width_input / faceFormsMultiplier;
              faceForms[FACE_FORM_NEUTRAL * 2 + 1] =
                  mth_height_input / faceFormsMultiplier;
            }
            if (dlib_data->dlib_thread_signal ==
                DLIB_THREAD_SIGNAL_CALIBRATE_MTH_E) {
              faceForms[FACE_FORM_E * 2 + 0] =
                  mth_width_input / faceFormsMultiplier;
              faceForms[FACE_FORM_E * 2 + 1] =
                  mth_height_input / faceFormsMultiplier;
            }
            if (dlib_data->dlib_thread_signal ==
                DLIB_THREAD_SIGNAL_CALIBRATE_MTH_A) {
              faceForms[FACE_FORM_A * 2 + 0] =
                  mth_width_input / faceFormsMultiplier;
              faceForms[FACE_FORM_A * 2 + 1] =
                  mth_height_input / faceFormsMultiplier;
            }
            if (dlib_data->dlib_thread_signal ==
                DLIB_THREAD_SIGNAL_CALIBRATE_MTH_Fun) {
              faceForms[FACE_FORM_Fun * 2 + 0] =
                  mth_width_input / faceFormsMultiplier;
              faceForms[FACE_FORM_Fun * 2 + 1] =
                  mth_height_input / faceFormsMultiplier;
            }
            if (dlib_data->dlib_thread_signal ==
                DLIB_THREAD_SIGNAL_CALIBRATE_MTH_U) {
              faceForms[FACE_FORM_U * 2 + 0] =
                  mth_width_input / faceFormsMultiplier;
              faceForms[FACE_FORM_U * 2 + 1] =
                  mth_height_input / faceFormsMultiplier;
            }
            if (DLIB_THREAD_SIGNAL_CALIBRATE_MTH_Neutral <=
                    dlib_data->dlib_thread_signal &&
                dlib_data->dlib_thread_signal <=
                    DLIB_THREAD_SIGNAL_CALIBRATE_MTH_U) {
              fputs("double faceForms[] = {", stdout);
              for (int i = 0; i < 9; i++)
                printf("%f, ", faceForms[i]);
              printf("%f", faceForms[9]);

              fputs("};\n", stdout);

              dlib_data->dlib_thread_signal = DLIB_THREAD_SIGNAL_NONE;
            }

            // Brows
            double brow_input = dlib::length(shape.part(19) + shape.part(24) -
                                             shape.part(27) * 2) *
                                distance_mul_2 * 0.4;
            if (first) {
              brow = brow_input;
              brow_offset = brow;
            } else {
              brow = filterBrow(brow_input);
              brow_offset +=
                  adaptStrength * ADAPT_SPEED_2 * (brow - brow_offset);
            }

            face_data->BRW_Fun = clamp((brow - brow_offset) * .2);
            face_data->BRW_Angry = clamp(-(brow - brow_offset) * .2);

            double eye_l_input = dlib::length(shape.part(41) + shape.part(40) -
                                              shape.part(38) - shape.part(37)) *
                                 distance_mul_2 * -1.1;
            double eye_r_input = dlib::length(shape.part(44) + shape.part(43) -
                                              shape.part(46) - shape.part(47)) *
                                 distance_mul_2 * -1.1;
            if (abs(rotation_vector.x()) > 0.2) {
              double eye_input_correction =
                  -22 * (abs(rotation_vector.x()) - 0.2);
              eye_l_input += eye_input_correction;
              eye_r_input += eye_input_correction;
            }
            // printf("%f\t%f\t%f\n", eye_l_input, eye_r_input, rotation_vector.x());
            // printf("%f\t%f\t%f\n", eye_l_input, eye_r_input, distance_mul_2);

            if (first) {
              eye_l = eye_l_input;
              eye_r = eye_r_input;
              eye_l_offset = eye_l_input;
              eye_r_offset = eye_r_input;
            } else {
              eye_l += .3 * (eye_l_input - eye_l) *
                       std::min(1.0, abs(eye_l_input - eye_l) * .12);
              eye_r += .3 * (eye_r_input - eye_r) *
                       std::min(1.0, abs(eye_r_input - eye_r) * .12);
              eye_l_offset +=
                  adaptStrength * ADAPT_SPEED_2 * (eye_l - eye_l_offset);
              eye_r_offset +=
                  adaptStrength * ADAPT_SPEED_2 * (eye_r - eye_r_offset);
            }

            double eye_sum = (eye_l - eye_l_offset + eye_r - eye_r_offset) * .5;
            if (webcam_settings->EyeSync) {
              face_data->EYE_Close = clamp((eye_sum)*.4 - .05);
            } else {
              face_data->EYE_Close_L = clamp((eye_l - eye_l_offset) * .4 - .05);
              face_data->EYE_Close_R = clamp((eye_r - eye_r_offset) * .4 - .05);
            }

            if (first) {
              first = false;
            }
            firstCounter++;
          }
          // if faces not found
          else {
            facesLostCounter++;

            if (facesLostCounter <= 2) {
              #ifdef TRACKING_LOG
              printf("Face lost counter: %d\n", facesLostCounter);
              #endif
              for (int i=0; i < 3; i++) {
                translation_vector3(i) = translation_vector2(i);
                translation_vector2(i) = translation_vector1(i);
                translation_vector1(i) = translation_vector(i);
                rotation_vector3(i) = rotation_vector2(i);
                rotation_vector2(i) = rotation_vector1(i);
                rotation_vector1(i) = rotation_vector(i);
              }

              if (facesLostCounter == 1) {
                translation_vector =
                    translation_vector1 +
                    (translation_vector1 - translation_vector2) +
                    ((translation_vector1 - translation_vector2) -
                     (translation_vector2 - translation_vector3));
                rotation_vector = rotation_vector1 +
                                  (rotation_vector1 - rotation_vector2) +
                                  ((rotation_vector1 - rotation_vector2) -
                                   (rotation_vector2 - rotation_vector3));
              } else {
                translation_vector =
                    translation_vector1 +
                    (translation_vector1 - translation_vector2);
                rotation_vector =
                    rotation_vector1 + (rotation_vector1 - rotation_vector2);
              }

              face_data->rotation1 = -(rotation_vector.x() -
                                       rotation_vector_offset.x()) *
                                     180 / 3.1415;
              face_data->rotation2 = -(rotation_vector.z() -
                                       rotation_vector_offset.z()) *
                                     180 / 3.1415;
              face_data->rotation3 = -(rotation_vector.y() -
                                       rotation_vector_offset.y()) *
                                     180 / 3.1415;

              face_data->translation1 = translation_vector.x() -
                                        translation_vector_offset.x();
              face_data->translation2 = translation_vector.y() -
                                        translation_vector_offset.y();
              face_data->translation3 = translation_vector.z() -
                                        translation_vector_offset.z();
            } else {
              face_data->MTH_E *= 0.9;
              face_data->MTH_A *= 0.9;
              face_data->MTH_Fun *= 0.9;
              face_data->MTH_U *= 0.9;
              face_data->BRW_Fun *= 0.9;
              face_data->BRW_Angry *= 0.9;
              face_data->EYE_Close *= 0.9;
              face_data->EYE_Close_L *= 0.9;
              face_data->EYE_Close_R *= 0.9;

              // return when inactive 3 seconds
              if (facesLostCounter > 90) {
                face_data->rotation1 *= 0.99;
                face_data->rotation2 *= 0.99;
                face_data->rotation3 *= 0.99;

                face_data->translation1 *= 0.99;
                face_data->translation2 *= 0.99;
                face_data->translation3 *= 0.99;
                
                face_data->on_screen = false;
              }
            }
          }
          // Send sync signal to main process (after recognition to reduce
          // latency)
          if (webcam_settings->SyncType2)
            pthread_cond_signal(&dlib_data->dlib_thread_cond);
        } else {
          break;
        }
      }

      dlib_data->dlib_thread_ready = 0;
      pthread_cond_signal(&dlib_data->dlib_thread2_cond2);
      pthread_cond_signal(&dlib_data->dlib_thread_cond);
      //cap.release();
      stop_capturing();
    }
    if (!first)
      dlib::sleep(2000);
    else
      break;
  }
  capture_cleanup();

  return NULL;
}
