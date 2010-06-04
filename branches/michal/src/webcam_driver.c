#define _GNU_SOURCE
#include <features.h>
#include <stdio.h>
#undef _GNU_SOURCE

#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <dirent.h>
#include <stdbool.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>
#include <sys/poll.h>
#include "webcam_driver.h"
#include "utils.h"
#include "pref.h"
#include "pref_global.h"
#include "image_process.h"
#include "pref_int.h"
#include "runloop.h"

#define NUM_OF_BUFFERS 8
typedef struct {
        void *start;
        size_t length;
} mmap_buffer;

static mmap_buffer *buffers = NULL;


typedef struct{
  int fd;
  int expecting_blobs;
  bool is_diag;
  unsigned int buffers;
  int w;
  int h;
  unsigned char *bw_frame;
  unsigned int threshold;
  int min_blob_pixels;
  int max_blob_pixels;
} webcam_info;

static webcam_info wc_info;

/*************/
/* interface */
/*************/

int ltr_int_tracker_wakeup();
int ltr_int_tracker_suspend();



static char *get_webcam_id(int fd)
{
  struct v4l2_capability capability;

  //Query device capabilities
  int ioctl_res = ioctl(fd, VIDIOC_QUERYCAP, &capability);
  if(ioctl_res == 0){
    __u32 cap = capability.capabilities;
    ltr_int_log_message("  Found V4L2 webcam: '%s'\n", 
      		capability.card);
    //Look for capabilities we need
    if((cap & V4L2_CAP_VIDEO_CAPTURE) && 
      (cap & V4L2_CAP_STREAMING)){
      return ltr_int_my_strdup((char *)capability.card);
    }else{
      ltr_int_log_message("  Found V4L2 webcam but it doesn't support streaming:-(\n");
    }
  }
  return NULL;
}

static int is_our_webcam(char *fname, char *webcam_id)
{
  int fd = open(fname, O_RDWR | O_NONBLOCK);
  if(fd == -1){
    ltr_int_log_message("Can't open file '%s'!\n", fname);
    return -1;
  }
  
  char *current_id = get_webcam_id(fd);
  if((current_id != NULL) && strncasecmp(current_id, webcam_id, strlen(webcam_id)) == 0){
    //this is the device we are looking for!
    free(current_id);
    return fd;
  }else{
    free(current_id);
    close(fd);
  }
  return -1;
}

int ltr_int_enum_webcams(char **ids[])
{
  assert(ids != NULL);
  int counter = 1; //Already plus one!!!
  plist wc_list = ltr_int_create_list();
  char *id;
  DIR *dev = opendir("/dev");
  if(dev == NULL){
    ltr_int_log_message("Can't open /dev for reading!\n");
    return -1;
  }
  struct dirent *de;
  //Get list of all wabcams
  while((de = readdir(dev)) != NULL){
    if(strncmp("video", de->d_name, 5) == 0){
      char *fname;
      asprintf(&fname, "/dev/%s", de->d_name);
      
      int fd = open(fname, O_RDWR | O_NONBLOCK);
      if(fd == -1){
	ltr_int_log_message("Can't open file '%s'!\n", fname);
	return -1;
      }
      
      id = get_webcam_id(fd);
      if(id != NULL){
	++counter;
	ltr_int_add_element(wc_list, id); 
      }
      close(fd);
      free(fname);
    }
  }
  closedir(dev);
  //Convert list to array
  return ltr_int_list2string_list(wc_list, ids);
}


static int search_for_webcam(char *webcam_id);

int ltr_int_enum_webcam_formats(char *id, webcam_formats *all_formats)
{
  int fd = search_for_webcam(id);
  if(fd < 0){
    return -1;
  }
  
  int fmt_cntr = 0;
  int sizes_cntr;
  int ival_cntr;
  int items = 0;
  struct v4l2_fmtdesc fmt;
  struct v4l2_frmsizeenum frm;
  struct v4l2_frmivalenum ival;
  plist fmt_strings = ltr_int_create_list();
  plist formats = ltr_int_create_list();
  
  //Enumerate all available formats
  while(1){
    fmt.index = fmt_cntr;
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(ioctl(fd, VIDIOC_ENUM_FMT, &fmt) != 0){
      break;
    }
    ++fmt_cntr;
    ltr_int_log_message("Supported format: %s\n", (char *)fmt.description);
    ltr_int_add_element(fmt_strings, ltr_int_my_strdup((char *)fmt.description));
    sizes_cntr = 0;
    while(1){
      frm.index = sizes_cntr++;
      frm.pixel_format = fmt.pixelformat;
      if(ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frm) != 0){
	break;
      }
      if(frm.type == V4L2_FRMSIZE_TYPE_DISCRETE){
	ltr_int_log_message("Frame size %d x %d\n", frm.discrete.width, frm.discrete.height);
	ival_cntr = 0;
	while(1){
	  ival.index = ival_cntr++;
	  ival.pixel_format = fmt.pixelformat;
	  ival.width = frm.discrete.width;
	  ival.height = frm.discrete.height;
	  if(ioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &ival) != 0){
	    break;
	  }
	  if(ival.type == V4L2_FRMIVAL_TYPE_DISCRETE){
	    ltr_int_log_message("Supported framerate: %f\n", 
				(float)ival.discrete.denominator / ival.discrete.numerator);
	    webcam_format *new_fmt = (webcam_format*)ltr_int_my_malloc(sizeof(webcam_format));
	    new_fmt->i = fmt_cntr - 1;
	    new_fmt->fourcc = fmt.pixelformat;
	    new_fmt->w = frm.discrete.width;
	    new_fmt->h = frm.discrete.height;
	    new_fmt->fps_num = ival.discrete.numerator;
	    new_fmt->fps_den = ival.discrete.denominator;
	    ltr_int_add_element(formats, new_fmt);
	    ++items;
	  }else{
	    ltr_int_log_message("Unsupported framerate spec format!\n");
	  }
	}
      }else{
	ltr_int_log_message("Unsupported framesize format! %d\n", frm.type);
      }
    }
  }
  char **strs = (char **)ltr_int_my_malloc((fmt_cntr + 1) * sizeof(char *));
  int cntr = 0;
  iterator i;
  char *desc;
  ltr_int_init_iterator(fmt_strings, &i);
  
  while((desc = (char *)ltr_int_get_next(&i)) != NULL){
    strs[cntr++] = desc;
  }
  strs[cntr] = NULL;
  all_formats->fmt_strings = strs;

  webcam_format *fmt_array = (webcam_format*)ltr_int_my_malloc(items * sizeof(webcam_format));
  ltr_int_init_iterator(formats, &i);
  webcam_format *wf;
  cntr = 0;
  while((wf = (webcam_format*)ltr_int_get_next(&i)) != NULL){
    fmt_array[cntr++] = *wf;
  }
  all_formats->formats = fmt_array;
  all_formats->entries = items;
  
  ltr_int_free_list(formats, true);
  ltr_int_free_list(fmt_strings, false);
  close(fd);
  return items;
}

int ltr_int_enum_webcam_formats_cleanup(webcam_formats *all_formats)
{
  int j = 0;
  while(all_formats->fmt_strings[j] != NULL){
    free(all_formats->fmt_strings[j++]);
  }
  free(all_formats->fmt_strings);
  all_formats->fmt_strings = NULL;
  free(all_formats->formats);
  all_formats->formats = NULL;
  return 0;
}


int search_for_webcam(char *webcam_id)
{
  if(webcam_id == NULL){
    ltr_int_log_message("Please spacify webcam Id!\n");
    return -1;
  }
  DIR *dev = opendir("/dev");
  int wfd = -1;
  if(dev == NULL){
    ltr_int_log_message("Can't open /dev for reading!\n");
    return -1;
  }
  struct dirent *de;
  while((de = readdir(dev)) != NULL){
    if(strncmp("video", de->d_name, 5) == 0){
      char *fname;
      asprintf(&fname, "/dev/%s", de->d_name);
      if((wfd = is_our_webcam(fname, webcam_id)) != -1){
        ltr_int_log_message("Found webcam '%s' (%s)\n", de->d_name, fname);
        free(fname);
        break;
      }
      free(fname);
    }
  }
  closedir(dev);
  return wfd;
}



static bool read_pref_format(struct v4l2_format *fmt)
{
  memset(fmt, 0, sizeof(struct v4l2_format));
  
  char *dev_section = ltr_int_get_device_section();
  if(dev_section == NULL){
    return false;
  }
  const char *res = ltr_int_get_key(dev_section, "Resolution");
  if(res == NULL){
    ltr_int_log_message("No resolution specified!\n");
    return false;
  }
  const char *pix = ltr_int_get_key(dev_section, "Pixel-format");
  if(pix == NULL){
    ltr_int_log_message("No pixel format specified!\n");
    return false;
  }
  
  int x, y;
  if(sscanf(res, "%d x %d", &x, &y)!= 2){
    ltr_int_log_message("I don't understand resolution specified as '%s'!\n", res);
    return false;
  } 
  
  fmt->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  fmt->fmt.pix.width = x;
  fmt->fmt.pix.height = y;
  fmt->fmt.pix.pixelformat = *(__u32*)pix;
  fmt->fmt.pix.field = V4L2_FIELD_ANY;
  
  return true;
}


static bool set_capture_format(struct camera_control_block *ccb)
{
  struct v4l2_format fmt;
  if(read_pref_format(&fmt) != true){
    return false;
  }
  
  if(0 != ioctl(wc_info.fd, VIDIOC_S_FMT, &fmt)){
    switch(errno){
      case EBUSY:
        ltr_int_log_message("Can't switch formats right now!\n");
        break;
      case EINVAL:
        ltr_int_log_message("Using wrong data to switch formats!\n");
        break;
    }
    return false;
  }
  ccb->pixel_width = wc_info.w = fmt.fmt.pix.width;
  ccb->pixel_height = wc_info.h = fmt.fmt.pix.height;
  wc_info.bw_frame = (unsigned char *)ltr_int_my_malloc(wc_info.w * wc_info.h);
  ltr_int_log_message("Switch of the format successfull!\n");
  return true;
}

static bool set_stream_params()
{
  char *dev_section = ltr_int_get_device_section();
  if(dev_section == NULL){
    return false;
  }
  const char *fps = ltr_int_get_key(dev_section, "Fps");
  if(fps == NULL){
    ltr_int_log_message("No framerate specified!\n");
    return false;
  }
  int num, den;
  if(sscanf(fps, "%d/%d", &num, &den)!= 2){
    ltr_int_log_message("I don't understand fps specified as '%s'!\n", fps);
    return false;
  } 

  struct v4l2_streamparm sp;
  memset(&sp, 0, sizeof(sp));
  sp.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  //Switched num and den - to get time instead of fps;
  sp.parm.capture.timeperframe.numerator = den;
  sp.parm.capture.timeperframe.denominator = num;

  if(-1 == ioctl(wc_info.fd, VIDIOC_S_PARM, &sp)){
    ltr_int_log_message("Stream parameters setup failed! (%s)\n", strerror(errno));
    return false;
  }
  return true;
}


/*
 * Sends request to driver for mmap-able buffers
 * Uses constant NUM_OF_BUFFERS
 *
 * Returns number of buffers granted
 */
static int request_streaming_buffers()
{
  struct v4l2_requestbuffers reqb;
  memset(&reqb, 0, sizeof(reqb));
  reqb.count = NUM_OF_BUFFERS;
  reqb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  reqb.memory = V4L2_MEMORY_MMAP;
  
  //request buffers from driver
  if(0 != ioctl(wc_info.fd, VIDIOC_REQBUFS, &reqb)){
    ltr_int_log_message("Couldn't get streaming buffers! (%s)\n", strerror(errno));
    return 0;
  }
  if(reqb.count < NUM_OF_BUFFERS){
    ltr_int_log_message("Got fewer buffers than expected! (%d instead of %d)\n",
                reqb.count, NUM_OF_BUFFERS);
  }else{
    ltr_int_log_message("Got %d buffers...\n", reqb.count);
  }
  return reqb.count;
}

/*
 * Prepares buffers for streaming (mmaps them and allocs
 * space for output BW frames)
 * 
 * Returns TRUE on success, FALSE otherwise
 */
static bool setup_streaming_buffers()
{
  ltr_int_log_message("Setting up buffers for streaming...\n");
  wc_info.buffers = request_streaming_buffers();
  if(0 == wc_info.buffers){
      ltr_int_log_message("Request for buffers failed...\n");
      return false;
  }
  //alloc memory for array of buffers
  buffers = ltr_int_my_malloc(wc_info.buffers * sizeof(mmap_buffer));
  memset(buffers, 0, sizeof(mmap_buffer) * wc_info.buffers);
  
  //initialize buffer structures... 
  unsigned int cntr;
  for(cntr = 0; cntr < wc_info.buffers; ++cntr){
    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));
    
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = cntr;

    if(0 != ioctl(wc_info.fd, VIDIOC_QUERYBUF, &buf)){
      ltr_int_log_message("Request for buffer failed...\n");
      return false;
    }
    
    buffers[cntr].length = buf.length;
    buffers[cntr].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
      MAP_SHARED, wc_info.fd, buf.m.offset);
    if(MAP_FAILED == buffers[cntr].start){
      ltr_int_log_message("Mmap failed...\n");
      return false;
    }
  }
  ltr_int_log_message("Mmap setup successfull!\n");
  return true;
}


/*
 * Unmaps and frees streaming buffers...
 *  
 * Returns TRUE on success, FALSE otherwise
 */
static bool release_buffers()
{
  if(NULL == buffers){
    ltr_int_log_message("Trying to release already released buffers...\n");
    return false;
  }
  unsigned int cntr;
  for(cntr = 0; cntr < wc_info.buffers; ++cntr){
    if(-1 == munmap(buffers[cntr].start, buffers[cntr].length)){
      ltr_int_log_message("Munmap failed!\n");
    }
  }
  free(buffers);
  buffers = NULL;
  ltr_int_log_message("Buffers unmapped!\n");
  return true;
}

static bool read_img_processing_prefs()
{
  char *dev_section = ltr_int_get_device_section();
  if(dev_section == NULL){
    return false;
  }
  const char *thres = ltr_int_get_key(dev_section, "Threshold");
  if(thres == NULL){
    ltr_int_log_message("No threshold specified!\n");
    return false;
  }
  const char *max = ltr_int_get_key(dev_section, "Max-blob");
  if(max == NULL){
    ltr_int_log_message("No maximal pixel count for blob specified!\n");
    return false;
  }
  const char *min = ltr_int_get_key(dev_section, "Min-blob");
  if(min == NULL){
    ltr_int_log_message("No minimal pixel count for blob specified!\n");
    return false;
  }
  wc_info.threshold = (unsigned int)atoi(thres);
  wc_info.min_blob_pixels = atoi(min);
  wc_info.max_blob_pixels = atoi(max);
  return true;
}

/*
 * I'm going to actively ignore resolution and I'll set it from prefs...
 */
int ltr_int_tracker_init(struct camera_control_block *ccb)
{
  assert(ccb != NULL);
  assert(ccb->device.category == webcam);
  assert(ccb->device.device_id != NULL);
  int fd = search_for_webcam(ccb->device.device_id);
  if(fd == -1){
    return -1;
  }
  wc_info.fd = fd;
  wc_info.expecting_blobs = 3;
  
  if(set_capture_format(ccb) != true){
    ltr_int_log_message("Couldn't set capture format!\n");
    close(fd);
    return -1;
  }
  if(set_stream_params() != true){
    ltr_int_log_message("Couldn't set stream parameters!\n");
    close(fd);
    return -1;
  }
  if(setup_streaming_buffers() != true){
    ltr_int_log_message("Couldn't initialize mmap!\n");
    close(fd);
    return -1;
  }
  if(read_img_processing_prefs() != true){
    ltr_int_log_message("Couldn't initialize mmap!\n");
    close(fd);
    return -1;
  }
  ltr_int_prepare_for_processing(ccb->pixel_width, ccb->pixel_height);
  if(ltr_int_tracker_resume() != 0){
    ltr_int_log_message("Couldn't start streaming!\n");
    close(fd);
    return -1;
  }
  return 0;
}

int ltr_int_tracker_close()
{
  ltr_int_log_message("Webcam shutting down!\n");
  release_buffers();
  free(wc_info.bw_frame);
  close(wc_info.fd);
  return 0;
}

int ltr_int_tracker_resume()
{
  ltr_int_log_message("Queuing buffers...\n");
  enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  unsigned int cntr;
  for(cntr = 0; cntr < wc_info.buffers; ++cntr){
    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));
    
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = cntr;

    if(0 != ioctl(wc_info.fd, VIDIOC_QBUF, &buf)){
      ltr_int_log_message("Queuing of buffer failed...\n");
      return -1;
    }
  }
  ltr_int_log_message("Buffers queued, starting to stream!\n");
  
  if(-1 == ioctl(wc_info.fd, VIDIOC_STREAMON, &type)){
    ltr_int_log_message("Start of streaming failed!\n");
    return -1;
  }
  return 0;
}

int ltr_int_tracker_pause()
{
  enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if(-1 == ioctl(wc_info.fd, VIDIOC_STREAMOFF, &type)){
    ltr_int_log_message("Problem stopping streaming!\n");
    return -1;
  }
  ltr_int_log_message("Streaming stopped!\n");
  return 0;
}


int ltr_int_tracker_get_frame(struct camera_control_block *ccb, struct frame_type *f)
{
  (void) ccb;
  read_img_processing_prefs();
  f->bloblist.num_blobs = wc_info.expecting_blobs;
  f->width = wc_info.w;
  f->height = wc_info.h;
  struct v4l2_buffer buf;
  memset(&buf, 0, sizeof(buf));
  buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  buf.memory = V4L2_MEMORY_MMAP;

  int res;
  struct pollfd pfd = {
    .fd = wc_info.fd,
    .events = POLLIN | POLLRDNORM,
    .revents = 0
  };
  
  while(1){
    res = poll(&pfd, 1, 500);
    if(res == 1){
      if(pfd.revents == (POLLIN | POLLRDNORM)){
        //we have data!
        break;
      }else{
        if((pfd.revents && POLLERR) != 0){
          ltr_int_log_message("Poll returned error (%s)!\n", strerror(errno));
	}else{
	  ltr_int_log_message("Poll returned unexpected event! (%X)\n",pfd.revents);
	}
      }
    }else if(res == -1){
      ltr_int_log_message("Poll returned error! (%s)", strerror(errno));
      return -1;
    }else if(res == 0){
      ltr_int_log_message("Poll timed out!\n");
    }else{
      ltr_int_log_message("Poll returned unexpected value %d!\n", res);
      return -1;
    }
  };
  
  while(-1 == ioctl(wc_info.fd, VIDIOC_DQBUF, &buf)){
    switch(errno){
      case EAGAIN:
        continue;
        break;
      default:
        ltr_int_log_message("Problem dequeing buffer! (%s)\n", strerror(errno));
        return -1;
    }
  }
  assert(buf.index < wc_info.buffers);
  
  //Convert YUYV format to bw image
  //FIXME!!! - this would work only with YUYV!
  unsigned char *source_buf = (buffers[buf.index]).start;
  unsigned char *dest_buf = (f->bitmap != NULL) ? f->bitmap : wc_info.bw_frame;
  unsigned int cntr, cntr1;
  //int pts = 0;
  
  for(cntr = cntr1 = 0; cntr < buf.bytesused; cntr += 2, cntr1++){
    if(source_buf[cntr] > wc_info.threshold){
      dest_buf[cntr1] = source_buf[cntr];
      //++pts;
    }else{
      dest_buf[cntr1] = 0;
    }
  }
  
  //ltr_int_log_message("%d points found!\n", pts);

  if(-1 == ioctl(wc_info.fd, VIDIOC_QBUF, &buf)){
    ltr_int_log_message("Error queuing buffer!\n");
  }
  //ltr_int_log_message("Queued buffer %d\n", buf.index);
  image img = {
    .bitmap = dest_buf,
    .w = wc_info.w,
    .h = wc_info.h,
    .ratio = 1.0f
  };
  ltr_int_to_stripes(&img);
  ltr_int_stripes_to_blobs(3, &(f->bloblist), wc_info.min_blob_pixels, 
		   wc_info.max_blob_pixels, &img);
  return 0;
}
