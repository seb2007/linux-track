#include <string.h>
#include "XPLMDisplay.h"
#include "XPLMUtilities.h"
#include "XPLMDataAccess.h"
#include <stdio.h>
#include <math.h>

XPLMHotKeyID	gHotKey = NULL;
XPLMDataRef		head_psi = NULL;
XPLMDataRef		head_the = NULL;

void	MyHotKeyCallback(void *               inRefcon);    

int	AircraftDrawCallback(	XPLMDrawingPhase     inPhase,
                            int                  inIsBefore,
                            void *               inRefcon);


PLUGIN_API int XPluginStart(
						char *		outName,
						char *		outSig,
						char *		outDesc)
{
	strcpy(outName, "linuxTrack");
	strcpy(outSig, "linuxtrack.camera");
	strcpy(outDesc, "A plugin that controls view using your webcam.");

	/* Register our hot key for the new view. */
	gHotKey = XPLMRegisterHotKey(XPLM_VK_F8, xplm_DownFlag, 
				"3D linuxTrack view",
				MyHotKeyCallback,
				NULL);
        head_psi = XPLMFindDataRef("sim/graphics/view/pilots_head_psi");
        head_the = XPLMFindDataRef("sim/graphics/view/pilots_head_the");
        if((head_psi==NULL)||(head_the==NULL)){
          return(0);
        }
//!!!        if(lt_InitCapture(NULL)!=0){
//!!!          return(0);
//!!!        }
	return(1);
}

PLUGIN_API void	XPluginStop(void)
{
	XPLMUnregisterHotKey(gHotKey);
//!!!        lt_CloseCapture();
}

PLUGIN_API void XPluginDisable(void)
{
}

PLUGIN_API int XPluginEnable(void)
{
	return 1;
}

PLUGIN_API void XPluginReceiveMessage(
					XPLMPluginID	inFromWho,
					long			inMessage,
					void *			inParam)
{
}

void	MyHotKeyCallback(void *               inRefcon)
{
  static int active=0;
	/* This is the hotkey callback.  First we simulate a joystick press and
	 * release to put us in 'free view 1'.  This guarantees that no panels
	 * are showing and we are an external view. */
	 
	/* Now we control the camera until the view changes. */
	if(active==0){
	  active=1;
//!!!          lt_Center();
	XPLMRegisterDrawCallback(
					AircraftDrawCallback,
					xplm_Phase_LastCockpit,
					0,
					NULL);
	}else{
	  active=0;
	  XPLMUnregisterDrawCallback(
					AircraftDrawCallback,
					xplm_Phase_LastCockpit,
					0,
					NULL);
	  XPLMSetDataf(head_psi,0.0f);
	  XPLMSetDataf(head_the,0.0f);
	}
}

int	AircraftDrawCallback(	XPLMDrawingPhase     inPhase,
                            int                  inIsBefore,
                            void *               inRefcon)
{
		float heading, pitch;
		float roll;
		//!!!lt_GetPosition(&pitch,&roll,&heading);
		
		/* Fill out the camera position info. */
		XPLMSetDataf(head_psi,heading);
		XPLMSetDataf(head_the,pitch);
	return 1;
}                                   
