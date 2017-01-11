#include "udp_listener.h"

#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>			// for strncmp()
#include <unistd.h>			// for close()
#include "audioMixer.h"
#include "zencape_inputCtrl.h"
#include "joystick_ctrl.h"
#include <sys/time.h>
#define MSG_MAX_LEN 1024
#define PORT 12345
void returnPacket(char* message);
void UDP_Listener_init();
void get_uptime(char* uptime);

char uptime[MSG_MAX_LEN];

void UDP_Listener_init(){



	printf("UDP Listener on port %d:\n", PORT);

	// Buffer to hold packet data:
		char message[MSG_MAX_LEN];

		// Address
		struct sockaddr_in sin;
		memset(&sin, 0, sizeof(sin));
		sin.sin_family = AF_INET;                   // Connection may be from network
		sin.sin_addr.s_addr = htonl(INADDR_ANY);    // Host to Network long
		sin.sin_port = htons(PORT);                 // Host to Network short

		// Create the socket for UDP
		int socketDescriptor = socket(PF_INET, SOCK_DGRAM, 0);

		// Bind the socket to the port (PORT) that we specify
		bind (socketDescriptor, (struct sockaddr*) &sin, sizeof(sin));


		while (1) {
			unsigned int sin_len = sizeof(sin);
			int bytesRx = recvfrom(socketDescriptor, message, MSG_MAX_LEN, 0, (struct sockaddr *) &sin, &sin_len);

			int upt =strncmp(message, "uptime", 6);
			message[bytesRx] = 0;
			if(upt)
				printf("Message received (%d bytes): \n\n'%s'\n", bytesRx, message);

			returnPacket(message);

			get_uptime(uptime);

			sprintf(message,"%s",uptime);

			// Transmit a reply:
			sin_len = sizeof(sin);
			sendto( socketDescriptor,
					message, strlen(message),
					0,
					(struct sockaddr *) &sin, sin_len);
		}

		// Close
		close(socketDescriptor);
}

void returnPacket(char* message){
	int value=0;
	char str[1024];
	int n=sscanf(message, "%s %d" ,str, &value);

	if( n==2){

		if(!strncmp(str, "T", 1)){
			AudioMixer_setBPM(value);
		}
		if(!strncmp(str, "V", 1)){
			AudioMixer_setVolume(value);
		}
	}
	else if(n==1){
		if(!strncmp(str, "None", 4)){
			zencape_setMenu(2);
		}
		else if(!strncmp(str, "R1B", 3)){
			zencape_setMenu(0);
		}
		else if(!strncmp(str, "R2B", 3)){
			zencape_setMenu(1);
		}
		else if(!strncmp(str, "Hit_hat", 7)){
			puts("HI");
			Audio_playFile(2);
		}
		else if(!strncmp(str, "Snare", 5)){
			Audio_playFile(8);
		}
		else if(!strncmp(str, "Base", 4)){
			Audio_playFile(0);
		}
	}
	else {
		sprintf(message, "Unknown command\n");
	}

}


void get_uptime(char* uptime){
	float uptime_t1=0;
	float uptime_t2=0;
	float totalUptime=0;
	char utime[MSG_MAX_LEN];

	readFile("/proc/uptime",utime,MSG_MAX_LEN);

	sscanf(utime, "%f %f" ,&uptime_t1, &uptime_t2);

	totalUptime=uptime_t1+uptime_t2;

	int hr=(int)(totalUptime/3600);
	int min=(int)((totalUptime-hr*3600)/60);
	int sec=(int)(totalUptime-min*60-hr*3600);

	sprintf(uptime, "%d:%d:%d", hr, min, sec);

}
