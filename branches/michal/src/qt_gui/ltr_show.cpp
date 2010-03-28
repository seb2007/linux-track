#include <QImage>
#include <QPixmap>
#include <QLabel>
#include <QTimer>
#include <QThread>
#include <iostream>
#include <ltr_show.h>
#include <cal.h>
#include <utils.h>
#include <pref_global.h>
#include <pref_int.h>
#include <iostream>

QImage *img;
QPixmap *pic;
QLabel *label;
QTimer *timer;

unsigned char *bitmap = NULL;
bool flag;
static unsigned char *qt_bitmap = NULL;
static unsigned int w = 0;
static unsigned int h = 0;

extern "C" {
  int frame_callback(struct camera_control_block *ccb, struct frame_type *frame);
}

CaptureThread *ct;

void CaptureThread::run()
{
  static struct camera_control_block ccb;
  if(get_device(&ccb) == false){
    log_message("Can't get device category!\n");
    return;
  }
  ccb.mode = operational_3dot;
  ccb.diag = false;
  cal_run(&ccb, frame_callback);
}

void CaptureThread::signal_new_frame()
{
  emit new_frame();
}

LtrGuiForm::LtrGuiForm()
 {
     ui.setupUi(this);
     label = new QLabel();
     ui.pix_box->addWidget(label);
     timer = new QTimer();
     ui.pauseButton->setDisabled(true);
     ui.wakeButton->setDisabled(true);
     ui.stopButton->setDisabled(true);
 }

int frame_callback(struct camera_control_block *ccb, struct frame_type *frame)
{
  static int cnt = 0;
  
  cnt++;
  std::cout<<cnt<<". frame"<<std::endl;
  
  if((w != frame->width) || (h != frame->height)){
    w = frame->width;
    h = frame->height;
    if(bitmap != NULL){
      free(bitmap);
    }
    bitmap = (unsigned char *)my_malloc(frame->width * frame->height);
    if(qt_bitmap != NULL){
      free(qt_bitmap);
    }
    if(qt_bitmap != NULL){
      
    }
    qt_bitmap = (unsigned char*)my_malloc(h * w * 3);
    img = new QImage(qt_bitmap, w, h, w * 3, QImage::Format_RGB888);
  }
  frame->bitmap = bitmap;
  if(flag == false){
    flag = true;
    ct->signal_new_frame();
  }
  return 0;
}


void LtrGuiForm::on_startButton_pressed()
{
  ct = new CaptureThread();
  connect(ct, SIGNAL(new_frame()), this, SLOT(update()));
  ct->start();
  ui.statusbar->showMessage("Starting!");
  ui.startButton->setDisabled(true);
  ui.pauseButton->setDisabled(false);
  ui.wakeButton->setDisabled(true);
  ui.stopButton->setDisabled(false);
}

void LtrGuiForm::on_pauseButton_pressed()
{
  cal_suspend();
  ui.statusbar->showMessage("Paused!");
  ui.startButton->setDisabled(true);
  ui.pauseButton->setDisabled(true);
  ui.wakeButton->setDisabled(false);
  ui.stopButton->setDisabled(false);
}

void LtrGuiForm::on_wakeButton_pressed()
{
  cal_wakeup();
  ui.statusbar->showMessage("Waking!");
  ui.startButton->setDisabled(true);
  ui.pauseButton->setDisabled(false);
  ui.wakeButton->setDisabled(true);
  ui.stopButton->setDisabled(false);
}


void LtrGuiForm::on_stopButton_pressed()
{
  cal_shutdown();
  ct->wait(2000);
  ui.statusbar->showMessage("Stopping!");
  ui.startButton->setDisabled(false);
  ui.pauseButton->setDisabled(true);
  ui.wakeButton->setDisabled(true);
  ui.stopButton->setDisabled(true);
}

void LtrGuiForm::update()
{
  static int cnt = 0;
  cnt++;
  ui.statusbar->showMessage(QString("").setNum(cnt) + ". frame");
  
  unsigned char *src, *dst;
  unsigned int pixcnt;

  src = bitmap;
  dst = qt_bitmap;
  for(pixcnt = 0; pixcnt < w * h; ++pixcnt){
    *(dst++) = *src;
    *(dst++) = *src;
    *(dst++) = *src;
    *src = 0;
    src++;
  }
  
  label->setPixmap(pic->fromImage(*img));
  flag = false;
}


