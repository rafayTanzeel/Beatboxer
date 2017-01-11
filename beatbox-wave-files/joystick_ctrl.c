#include "joystick_ctrl.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>


//Constants to store the joystick up(gpio 26) path of file
#define EXPORT_FILE_PATH "/sys/class/gpio/export"
#define JSUP_FILE_PATH "/sys/class/gpio/gpio26"
#define JSUP_FILE_PATH_VALUE "/sys/class/gpio/gpio26/value"

#define JSUP "26" //constant no. 26 is joystick up gpio number
#define JSDN "46" //constant no. 46 is joystick down gpio number
#define JSRT "47" //constant no. 47 is joystick right gpio number
#define JSLFT "65" //constant no. 65 is joystick left gpio number
#define JSPB "27" //constant no. 27 is joystick pushdown gpio number

static pthread_t joystickThreadId;

//Movement Mask
//00000 No Movement
//00001 Up Movement
//00010 Dn Movement
//00100 Lft Movement
//01000 Rt Movement
//10000 Pb Movement

static int movement_mask=00000;

int joystick_getMovement(){
	return movement_mask;
}


void joystick_init(void){
	writeFile(EXPORT_FILE_PATH, JSUP); //Inputting joystick up gpio into export file of gpio
	writeFile(EXPORT_FILE_PATH, JSDN); //Inputting joystick down gpio into export file of gpio
	writeFile(EXPORT_FILE_PATH, JSRT); //Inputting joystick right gpio into export file of gpio
	writeFile(EXPORT_FILE_PATH, JSLFT); //Inputting joystick left gpio into export file of gpio
	writeFile(EXPORT_FILE_PATH, JSPB); //Inputting joystick pushdown gpio into export file of gpio


	char input_direction[1024];

	sprintf(input_direction, "/sys/class/gpio/gpio%s/direction", JSUP);
	writeFile(input_direction, "in"); //Inputting in value to get input of joystick when pulled up

	sprintf(input_direction, "/sys/class/gpio/gpio%s/direction", JSDN);
	writeFile(input_direction, "in"); //Inputting in value to get input of joystick when pulled up

	sprintf(input_direction, "/sys/class/gpio/gpio%s/direction", JSRT);
	writeFile(input_direction, "in"); //Inputting in value to get input of joystick when pulled up

	sprintf(input_direction, "/sys/class/gpio/gpio%s/direction", JSLFT);
	writeFile(input_direction, "in"); //Inputting in value to get input of joystick when pulled up

	sprintf(input_direction, "/sys/class/gpio/gpio%s/direction", JSPB);
	writeFile(input_direction, "in"); //Inputting in value to get input of joystick when pulled up


	// Launch playback thread:
	pthread_create(&joystickThreadId, NULL, joystickThread, NULL);


}


void* joystickThread(void* arg){

	const int max_length = 1024;
	char buff[max_length];
	char input_value[max_length];

	char value;

	while(true){

		sprintf(input_value, "/sys/class/gpio/gpio%s/value", JSUP);
		readFile(input_value, buff, max_length); //reading joystick value from gpio
		value=buff[0]; //char extracted rom joystick up gpio file
		movement_mask=(value=='0')?00001:00000;

//		printf("JS UP %c\n", value);

		sprintf(input_value, "/sys/class/gpio/gpio%s/value", JSDN);
		readFile(input_value, buff, max_length); //reading joystick value from gpio
		value = buff[0]; //char extracted rom joystick up gpio file
		movement_mask=(value=='0')?00010:00000;
//		printf("JS Down %c\n", value);

		sprintf(input_value, "/sys/class/gpio/gpio%s/value", JSRT);
		readFile(input_value, buff, max_length); //reading joystick value from gpio
		value = buff[0]; //char extracted rom joystick up gpio file
		movement_mask=(value=='0')?01000:00000;
//		printf("JS Right %c\n", value);

		sprintf(input_value, "/sys/class/gpio/gpio%s/value", JSLFT);
		readFile(input_value, buff, max_length); //reading joystick value from gpio
		value = buff[0]; //char extracted rom joystick up gpio file
		movement_mask=(value=='0')?00100:00000;
//		printf("JS Left %c\n", value);

		sprintf(input_value, "/sys/class/gpio/gpio%s/value", JSPB);
		readFile(input_value, buff, max_length); //reading joystick value from gpio
		value = buff[0]; //char extracted rom joystick up gpio file
		movement_mask=(value=='0')?10000:00000;
//		printf("JS Centre %c\n", value);

	}

}






//function for output to any file
void writeFile(char* filePath, char* input){
	FILE *pfile = fopen(filePath, "w");
	if(pfile == NULL){
		printf("Error: Unable to open file\n");
		exit(1);
	}

	fprintf(pfile, "%s", input);
	fclose(pfile);
}

//function for getting input from any file
void readFile(char* filePath, char* buffer, const int size){
	FILE *pfile = fopen(filePath, "r");
	if(pfile == NULL){
		printf("Error: Unable to open file (%s) for read", filePath);
		exit(1);
	}

	fgets(buffer, size, pfile);
	fclose(pfile);
}
