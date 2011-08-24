#ifndef LTR_SHOW__H
#define LTR_SHOW__H

#include "ui_ltr_gui.h"
#include "ui_ltr.h"
#include <linuxtrack.h>

#include <QThread>
#include <QCloseEvent>
#include "window.h"

class ScpForm;
class LtrGuiForm;
class QSettings;

class CaptureThread : public QThread
{
  Q_OBJECT
 public:
  CaptureThread(LtrGuiForm *p);
  void run();
  void signal_new_frame();
 signals:
  void new_frame();  
 private:
  LtrGuiForm *parent;
  
};

class CameraView : public QWidget
{
  Q_OBJECT
 public:
  CameraView(QWidget *parent = 0);
// public slots:
  void redraw(QImage *img);
 protected:
  void paintEvent(QPaintEvent *event);
 private:
  QImage *image;
};

class LtrGuiForm : public QMainWindow
{
   Q_OBJECT
  public:
   LtrGuiForm(const Ui::LinuxtrackMainForm &tmp_gui, ScpForm *s, QSettings &settings);
   ~LtrGuiForm();
   void allowCloseWindow();
   void StorePrefs(QSettings &settings);
  public slots:
   void update();
    
  private slots:
   void on_startButton_pressed();
   void on_recenterButton_pressed();
   void on_pauseButton_pressed();
   void on_wakeButton_pressed();
   void on_stopButton_pressed();
   void disableCamView_stateChanged(int state);
   void disable3DView_stateChanged(int state);
   void stateChanged(ltr_state_type current_state);
  protected:
   void closeEvent(QCloseEvent *event);
  private:
   Ui::Ltr_gui ui;
   ScpForm *sens;
   Window *glw;
   QTimer *timer;
   CameraView *cv;
   bool allowClose;
   const Ui::LinuxtrackMainForm &main_gui;
   void trackerStopped();
   void trackerRunning();
   void trackerPaused();
};

#endif