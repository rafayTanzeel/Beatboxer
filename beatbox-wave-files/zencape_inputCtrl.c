#include "zencape_inputCtrl.h"
#include <stdbool.h>
#include <pthread.h>
#include "joystick_ctrl.h"
#include "accelerometerCtrl.h"
#include "audioMixer.h"
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <sys/time.h>



#define MAX_VOLUME 100
#define MIN_VOLUME 0

#define TOTAL_MENUS 3;

static pthread_t zencapeThreadId;

static int menu=0;
static int volume=80;
static int tempo=120;
static int movement=0;
static double timers[]={0,0,0,0,0};

_Bool zencape_timeDiffGrtThan100ms(int i);

void zencape_init(void){
	menu=AudioMixer_getMode();
	volume=AudioMixer_getVolume();
	tempo=AudioMixer_getBPM();
	movement=joystick_getMovement();

	joystick_init();
	AudioMixer_init();
	accelerometer_init();
	// Launch playback thread:
	pthread_create(&zencapeThreadId, NULL, zencapeThread, NULL);

}


void* zencapeThread(void* arg){

	while(true){
			volume=AudioMixer_getVolume();
			tempo=AudioMixer_getBPM();
			movement=joystick_getMovement();
			menu=AudioMixer_getMode();
			update_postionVal();


//			printf("Volume: %d, Tempo: %d, Mode: %d\n",volume,tempo,menu);

			switch (movement) {
			case 00001: //Up Movement
				if(zencape_timeDiffGrtThan100ms(0))
					AudioMixer_setVolume((AudioMixer_getVolume()+5>MAX_VOLUME)?MAX_VOLUME:AudioMixer_getVolume()+5);
				break;
			case 00010: //Dn Movement
				if(zencape_timeDiffGrtThan100ms(1))
					AudioMixer_setVolume((AudioMixer_getVolume()-5<MIN_VOLUME)?MIN_VOLUME:AudioMixer_getVolume()-5);
				break;
			case 00100: //Lft Movement
				if(zencape_timeDiffGrtThan100ms(2))
					AudioMixer_setBPM(AudioMixer_getBPM()+5);
				break;
			case 01000: //Rt Movement
				if(zencape_timeDiffGrtThan100ms(3))
					AudioMixer_setBPM(AudioMixer_getBPM()-5);
				break;
			case 10000: //Pb Movement
				if(zencape_timeDiffGrtThan100ms(4)){
					++menu;
					menu%=TOTAL_MENUS;

					if (menu == 0) {
						standard_beats();
						AudioMixer_setMode(0);
					} else if (menu == 1) {
						custom_beats();
						AudioMixer_setMode(1);
					} else {
						AudioMixer_setMode(2);
					}
				}
				break;
			default:    //No Movement
				break;
			}
		}
}


void zencape_setMenu(int arg){
	menu=arg%TOTAL_MENUS;
	AudioMixer_setMode(menu);

	if (menu == 0) {
		standard_beats();
	} else if (menu == 1) {
		custom_beats();
	}
}


_Bool zencape_timeDiffGrtThan100ms(int i){
	struct timeval  tv;
	gettimeofday(&tv, NULL);
	double buff=(tv.tv_sec) * 1000 + (tv.tv_usec) / 1000 ;
	buff=(abs(timers[i]-buff));
	timers[i]=(tv.tv_sec) * 1000 + (tv.tv_usec) / 1000 ;
	return (buff>100)?true:false;
}
