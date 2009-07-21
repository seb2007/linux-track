#include <stdio.h>
#include "pref_global.h"
#include "ltlib.h"
#include "utils.h" 

/**************************/
/* private Static members */
/**************************/
static struct camera_control_block ccb;
static float filterfactor=1.0;
static float angle_scalefactor=1.0;
static struct bloblist_type filtered_bloblist;
static struct blob_type filtered_blobs[3];
static bool first_frame_read = false;

/*******************************/
/* private function prototypes */
/*******************************/
float expfilt(float x, 
              float y_minus_1,
              float filtfactor);

void expfilt_vec(float x[3], 
              float y_minus_1[3],
              float filtfactor,
              float res[3]);

float clamp_angle(float angle);

/************************/
/* function definitions */
/************************/
int lt_init(struct lt_configuration_type config)
{
  struct reflector_model_type rm;
  filterfactor = config.filterfactor;
  angle_scalefactor = config.angle_scalefactor;

  if(get_device(&(ccb.device.category)) == false){
    log_message("Can't get device category!\n");
    return 1;
  }
//  ccb.device.category = tir4_camera;

  ccb.mode = operational_3dot;
  if(cal_init(&ccb)!= 0){
    return -1;
  }
  cal_set_good_indication(&ccb, true);
  cal_thread_start(&ccb);

/*
  rm.p1[0] = -35.0;
  rm.p1[1] = -50.0;
  rm.p1[2] = -92.5;
  rm.p2[0] = +35.0;
  rm.p2[1] = -50.0;
  rm.p2[2] = -92.5;
  rm.hc[0] = +0.0;
  rm.hc[1] = -100.0;
  rm.hc[2] = +90.0;
*/
  if(get_pose_setup(&rm) == false){
    log_message("Can't get pose setup!\n");
    return 1;
  }
  pose_init(rm, 0.0);

  filtered_bloblist.num_blobs = 3;
  filtered_bloblist.blobs = filtered_blobs;
  first_frame_read = false;
  return 0;
}


int lt_get_camera_update(float *heading,
                         float *pitch,
                         float *roll,
                         float *tx,
                         float *ty,
                         float *tz)
{
  float raw_angles[3];
  float raw_translations[3];
  static float filtered_angles[3] = {0.0f, 0.0f, 0.0f};
  static float filtered_translations[3] = {0.0f, 0.0f, 0.0f};
  
  struct transform t;
  struct frame_type frame;
  bool frame_valid;
  cal_thread_get_frame(&frame, 
                       &frame_valid);
  if (frame_valid) {
    pose_sort_blobs(frame.bloblist);
    int i;
    for(i=0;i<3;i++) {
      if (first_frame_read) {
        filtered_bloblist.blobs[i].x = expfilt(frame.bloblist.blobs[i].x,
                                               filtered_bloblist.blobs[i].x,
                                               filterfactor);
        filtered_bloblist.blobs[i].y = expfilt(frame.bloblist.blobs[i].y,
                                               filtered_bloblist.blobs[i].y,
                                               filterfactor);
      }
      else {
        first_frame_read = true;
        filtered_bloblist.blobs[i].x = frame.bloblist.blobs[i].x;
        filtered_bloblist.blobs[i].y = frame.bloblist.blobs[i].y;
      }
    }
/*     printf("*** RAW blobs ***\n"); */
/*     bloblist_print(frame.bloblist); */
/*     printf("*** filtered blobs ***\n"); */
/*     bloblist_print(filtered_bloblist); */

    pose_process_blobs(filtered_bloblist, &t);
    pose_compute_camera_update(t,
                               &raw_angles[0], //heading
                               &raw_angles[1], //pitch
                               &raw_angles[2], //roll
                               &raw_translations[0], //tx
                               &raw_translations[1], //ty
                               &raw_translations[1]);//tz
    frame_free(&ccb, &frame);
    
    expfilt_vec(raw_angles, filtered_angles, filterfactor, filtered_angles);
    expfilt_vec(raw_translations, filtered_translations, filterfactor, 
            filtered_translations);
    
  }  
  *heading = clamp_angle(angle_scalefactor * filtered_angles[0]);
  *pitch = clamp_angle(angle_scalefactor * filtered_angles[1]);
  *roll = clamp_angle(angle_scalefactor * filtered_angles[2]);
  *tx = filtered_translations[0];
  *ty = filtered_translations[1];
  *tz = filtered_translations[2];
}

int lt_shutdown(void)
{
  cal_thread_stop();
  cal_shutdown(&ccb);
}

void lt_recenter(void)
{
  pose_recenter();
}

float expfilt(float x, 
              float y_minus_1,
              float filterfactor) 
{
  float y;
  
  y = y_minus_1*(1.0-filterfactor) + filterfactor*x;

  return y;
}

void expfilt_vec(float x[3], 
              float y_minus_1[3],
              float filterfactor,
              float res[3]) 
{
  res[0] = expfilt(x[0], y_minus_1[0], filterfactor);
  res[1] = expfilt(x[1], y_minus_1[1], filterfactor);
  res[2] = expfilt(x[2], y_minus_1[2], filterfactor);
}

float clamp_angle(float angle)
{
  if(angle<-180.0){
    return -180.0;
  }else if(angle>180.0){
    return 180.0;
  }else{
    return angle;
  }
}
