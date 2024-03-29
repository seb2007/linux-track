#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "pose.h"
#include "pref.h"
#include "pref_global.h"
#include "utils.h"
#include "axis.h"

#include "pathconfig.h"

typedef enum {UNSET, YES, NO} bool_val_t;

void ltr_int_close_prefs()
{
  ltr_int_free_prefs();
}

char *ltr_int_get_device_section()
{
  return ltr_int_get_key("Global", "Input");
}

static bool model_changed = true;

bool ltr_int_model_changed(bool reset_flag)
{
  bool flag = model_changed;
  if(reset_flag){
    model_changed = false;
  }
  return flag;
}

void ltr_int_announce_model_change()
{
  model_changed = true;
}

static char *ltr_int_get_model_section()
{
  return ltr_int_get_key("Global", "Model");
}

bool ltr_int_is_model_active()
{
  char *section = ltr_int_get_model_section();
  char *active = ltr_int_get_key(section, "Active");
  free(section);
  section = NULL;
  if(active == NULL){
    ltr_int_log_message("Unspecified if model is active, assuming it is not...\n");
    return false;
  }
  bool res = (strcasecmp(active, "yes") == 0) ? true : false;
  free(active);
  return res;
}

static bool_val_t use_alter = UNSET;

bool ltr_int_use_alter()
{
  if(use_alter == UNSET){
    char *pose_method = NULL;
    use_alter = NO;
    pose_method = ltr_int_get_key("Global", "Legacy-pose-computation");
    if(pose_method != NULL){
      if(strcasecmp(pose_method, "yes") == 0){
        use_alter = YES;
      }else{
        use_alter = NO;
      }
      free(pose_method);
    }
  }
  return (use_alter == YES);
}

void ltr_int_set_use_alter(bool state)
{
  use_alter = state ? YES: NO;
  ltr_int_change_key("Global", "Legacy-pose-computation", state?"yes":"no");
}

static float focal_length = -1.0f;

float ltr_int_get_focal_length()
{
  if(focal_length < 0.0f){
    if((!ltr_int_get_key_flt("Global", "Focal-length", &focal_length)) || (focal_length < 0.0f)){
      focal_length = 660.0f;
    }
  }
  return focal_length;
}

void ltr_int_set_focal_length(float fl)
{
  focal_length = fl;
  ltr_int_change_key_flt("Global", "Focal-length", fl);
}

static bool_val_t use_oldrot = UNSET;

bool ltr_int_use_oldrot()
{
  if(use_oldrot == UNSET){
    char *rot_method = NULL;
    use_oldrot = NO;
    rot_method = ltr_int_get_key("Global", "Legacy-rotation-computation");
    if(rot_method != NULL){
      if(strcasecmp(rot_method, "yes") == 0){
        use_oldrot = YES;
      }else{
        use_oldrot = NO;
      }
      free(rot_method);
    }
  }
  return (use_oldrot == YES);
}

void ltr_int_set_use_oldrot(bool state)
{
  use_oldrot = state ? YES: NO;
  ltr_int_change_key("Global", "Legacy-rotation-computation", state?"yes":"no");
}


static bool_val_t tr_align = UNSET;

bool ltr_int_do_tr_align()
{
  if(tr_align == UNSET){
    tr_align = YES;
    static char *tmp = NULL;
    tmp = ltr_int_get_key("Global", "Align-translations");
    if(tmp != NULL){
      if(strcasecmp(tmp, "yes") == 0){
        tr_align = YES;
      }else{
        tr_align = NO;
      }
      free(tmp);
    }
  }
  return (tr_align == YES);
}

void ltr_int_set_tr_align(bool state)
{
  tr_align = state ? YES: NO;
  ltr_int_change_key("Global", "Align-translations", state?"yes":"no");
}

bool ltr_int_get_device(struct camera_control_block *ccb)
{
  bool dev_ok = false;
  char *dev_section = ltr_int_get_device_section();
  if(dev_section == NULL){
    return false;
  }
  char *dev_type = ltr_int_get_key(dev_section, "Capture-device");
  if (dev_type == NULL) {
    dev_ok = false;
  } else {
    if(strcasecmp(dev_type, "Tir") == 0){
      ltr_int_log_message("Device Type: Track IR\n");
      ccb->device.category = tir;
      dev_ok = true;
    }
    if(strcasecmp(dev_type, "Tir4") == 0){
      ltr_int_log_message("Device Type: Track IR 4\n");
      ccb->device.category = tir4_camera;
      dev_ok = true;
    }
    if(strcasecmp(dev_type, "Webcam") == 0){
      ltr_int_log_message("Device Type: Webcam\n");
      ccb->device.category = webcam;
      dev_ok = true;
    }
    if(strcasecmp(dev_type, "Webcam-face") == 0){
      ltr_int_log_message("Device Type: Webcam - facetrack\n");
      ccb->device.category = webcam_ft;
      dev_ok = true;
    }
    if(strcasecmp(dev_type, "MacWebcam") == 0){
      ltr_int_log_message("Device Type: Webcam\n");
      ccb->device.category = mac_webcam;
      dev_ok = true;
    }
    if(strcasecmp(dev_type, "MacWebcam-face") == 0){
      ltr_int_log_message("Device Type: Webcam - facetrack\n");
      ccb->device.category = mac_webcam_ft;
      dev_ok = true;
    }
    if(strcasecmp(dev_type, "Wiimote") == 0){
      ltr_int_log_message("Device Type: Wiimote\n");
      ccb->device.category = wiimote;
      dev_ok = true;
    }
    if(dev_ok == false){
      ltr_int_log_message("Wrong device type found: '%s'\n", dev_type);
      ltr_int_log_message(" Valid options are: 'Tir4', 'Tir', 'Tir_openusb', 'Webcam', 'Wiimote'.\n");
    }
    free(dev_type);
  }

  char *dev_id = ltr_int_get_key(dev_section, "Capture-device-id");
  free(dev_section);
  if (dev_id == NULL) {
    dev_ok = false;
  }else{
    ccb->device.device_id = dev_id;
  }
  return dev_ok;
}

bool ltr_int_get_coord(char *coord_id, float *f)
{
  char *model_section = ltr_int_get_model_section();
  if(model_section == NULL){
    return false;
  }
  char *str = ltr_int_get_key(model_section, coord_id);
  free(model_section);
  model_section = NULL;
  if(str == NULL){
    ltr_int_log_message("Cannot find key %s in section %s!\n", coord_id, model_section);
    return false;
  }
  *f = atof(str);
  free(str);
  return true;
}

typedef enum {X, Y, Z, H_Y, H_Z} cap_index;

static bool setup_cap(reflector_model_type *rm, char *model_section)
{
  static char *ids[] = {"Cap-X", "Cap-Y", "Cap-Z", "Head-Y", "Head-Z"};
  ltr_int_log_message("Setting up Cap\n");

  float x, y, z, hy, hz;
  bool res = ltr_int_get_key_flt(model_section, ids[X], &x) &&
    ltr_int_get_key_flt(model_section, ids[Y], &y) &&
    ltr_int_get_key_flt(model_section, ids[Z], &z) &&
    ltr_int_get_key_flt(model_section, ids[H_Y], &hy) &&
    ltr_int_get_key_flt(model_section, ids[H_Z], &hz);

  if(!res){
    return false;
  }

  rm->p0[0] = 0;
  rm->p0[1] = y;
  rm->p0[2] = 0;
  rm->p1[0] = -x/2;
  rm->p1[1] = 0;
  rm->p1[2] = -z;
  rm->p2[0] = +x/2;
  rm->p2[1] = 0;
  rm->p2[2] = -z;
  rm->hc[0] = 0.0;
  rm->hc[1] = -hy;
  rm->hc[2] = hz;
  rm->type = CAP;
  return true;
}

typedef enum {Y1, Y2, Z1, Z2, HX, HY, HZ} clip_index;

static bool setup_clip(reflector_model_type *rm, char *model_section)
{
  ltr_int_log_message("Setting up Clip...\n");
  static char *ids[] = {"Clip-Y1", "Clip-Y2", "Clip-Z1", "Clip-Z2",
  			"Head-X", "Head-Y", "Head-Z"};

  float y1, y2, z1, z2, hx, hy, hz;
  bool res = ltr_int_get_key_flt(model_section, ids[Y1], &y1) &&
    ltr_int_get_key_flt(model_section, ids[Y2], &y2) &&
    ltr_int_get_key_flt(model_section, ids[Z1], &z1) &&
    ltr_int_get_key_flt(model_section, ids[Z2], &z2) &&
    ltr_int_get_key_flt(model_section, ids[HX], &hx) &&
    ltr_int_get_key_flt(model_section, ids[HY], &hy) &&
    ltr_int_get_key_flt(model_section, ids[HZ], &hz);

  /*
  y1 is vertical dist of upper and middle point
  y2 is vertical dist of upper and lower point
  z1 is horizontal dist of upper and middle point
  z2 is horizontal dist of uper and lower point
  hx,hy,hz are head center coords with upper point as origin
  */
  if(!res){
    return false;
  }

  rm->p0[0] = 0;
  rm->p0[1] = y1;
  rm->p0[2] = -z1;
  rm->p1[0] = 0;
  rm->p1[1] = 0;
  rm->p1[2] = 0;
  rm->p2[0] = 0;
  rm->p2[1] = y1 - y2;
  rm->p2[2] = -z1 - z2;
  rm->hc[0] = hx;
  rm->hc[1] = hy;
  rm->hc[2] = hz;
  rm->type = CLIP;
  return true;
}

bool ltr_int_get_model_setup(reflector_model_type *rm)
{
  assert(rm != NULL);
  char *model_section = ltr_int_get_model_section();
  assert(model_section != NULL);
  static bool res = false;
  char *model_type = ltr_int_get_key(model_section, "Model-type");
  assert(model_type != NULL);

  if(strcasecmp(model_type, "Cap") == 0){
    res = setup_cap(rm, model_section);
  }else if(strcasecmp(model_type, "Clip") == 0){
    res = setup_clip(rm, model_section);
  }else if(strcasecmp(model_type, "SinglePoint") == 0){
    rm->type = SINGLE;
    res = true;
  }else if(strcasecmp(model_type, "Face") == 0){
    rm->type = FACE;
    res = true;
  }else{
    ltr_int_log_message("Unknown modeltype specified in section %s\n", model_section);
    res = false;
  }
  free(model_type);
  free(model_section);
  return res;
}

int ltr_int_get_orientation()
{
  int orientation = 0;
  if(ltr_int_get_key_int("Global", "Camera-orientation", &orientation)){
    return orientation;
  }else{
    return 0;
  }
}
