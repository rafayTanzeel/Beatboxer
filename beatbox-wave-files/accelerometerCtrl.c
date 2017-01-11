#include "accelerometerCtrl.h"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <stdint.h>
#include <time.h>
#include <stdbool.h>
#include <sys/time.h>
#include "audioMixer.h"


#define I2CDRV_LINUX_BUS1 "/dev/i2c-1"

#define I2C_DEVICE_ADDRESS 0x1C

#define REG_DIR 0x2A
#define REG_OUT 0x01

#define THRESHOLD_X 12000
#define THRESHOLD_Y 18000
#define THRESHOLD_Z 10


#define BASE_DRUM 0
#define SPLASH 9
#define SNARE 7


static unsigned char OUT_X_MSB = 0x00;
static unsigned char OUT_X_LSB = 0x00;
static unsigned char OUT_Y_MSB = 0x00;
static unsigned char OUT_Y_LSB = 0x00;
static unsigned char OUT_Z_MSB = 0x00;
static unsigned char OUT_Z_LSB = 0x00;

static int i2cFileDesc=0;

int16_t prevX=0;
int16_t prevY=0;
int16_t prevZ=0;

static double timers[]={0,0,0};
_Bool timeDiffGrtThan100ms(int i);

static int initI2cBus(char* bus, int address)
{
	int i2cFileDesc = open(bus, O_RDWR);
	if (i2cFileDesc < 0) {
		printf("I2C DRV: Unable to open bus for read/write (%s)\n", bus);
		perror("Error is:");
		exit(-1);
	}

	int result = ioctl(i2cFileDesc, I2C_SLAVE, address);
	if (result < 0) {
		perror("Unable to set I2C device to slave address.");
		exit(-1);
	}
	return i2cFileDesc;
}


static void writeI2cReg(int i2cFileDesc, unsigned char regAddr, unsigned char value)
{
	unsigned char buff[2] = {regAddr, value};
	int res = write(i2cFileDesc, buff, 2);
	if (res != 2) {
		perror("Unable to write i2c register");
		exit(-1);
	}
}




void accelerometer_init(){
	i2cFileDesc = initI2cBus(I2CDRV_LINUX_BUS1, I2C_DEVICE_ADDRESS);
	writeI2cReg(i2cFileDesc, REG_DIR, REG_OUT);
	update_postionVal();
	close(i2cFileDesc);
}

void update_postionVal(){

	const int noOfBytes=7;
	char value[noOfBytes];
	i2cFileDesc = initI2cBus(I2CDRV_LINUX_BUS1, I2C_DEVICE_ADDRESS);


	int res = read(i2cFileDesc, &value, sizeof(value));
	if (res != sizeof(value)) {
		perror("Unable to read i2c register");
		exit(-1);
	}

	OUT_X_MSB = value[1];
	OUT_X_LSB = value[2];
	OUT_Y_MSB = value[3];
	OUT_Y_LSB = value[4];
	OUT_Z_MSB = value[5];
	OUT_Z_LSB = value[6];

	int16_t x = (OUT_X_MSB << 8) | (OUT_X_LSB);
	int16_t y = (OUT_Y_MSB << 8) | (OUT_Y_LSB);
	int16_t z = (((OUT_Z_MSB << 8) | (OUT_Z_LSB))-16300)/1000;


	if(prevX!=0 && abs(prevX-x)>THRESHOLD_X){
		if(timeDiffGrtThan100ms(0)){
			Audio_playFile(SNARE);
//			puts("X Acc Activated");
//			nanosleep((const struct timespec[]){{0, 1000000000}}, NULL);
		}
	}

	if(prevY!=0 && abs(prevY-y)>THRESHOLD_Y){
		if(timeDiffGrtThan100ms(1)){
			Audio_playFile(SPLASH);
//			puts("Y Acc Activated");
//			nanosleep((const struct timespec[]){{0, 1000000000}}, NULL);
		}
	}

	if(prevZ!=0 && abs(prevZ-z)>THRESHOLD_Z){
		if(timeDiffGrtThan100ms(2)){
			Audio_playFile(BASE_DRUM);
//			puts("Z Acc Activated");
//			nanosleep((const struct timespec[]){{0, 1000000000}}, NULL);
		}
	}

	prevX=abs(x);
	prevY=abs(y);
	prevZ=abs(z);

//	printf("Accelerometer X: %d, Y: %d, Z: %d\n",x,y,z);
//	nanosleep((const struct timespec[]){{0, 100000000}}, NULL);

	close(i2cFileDesc);
}

_Bool timeDiffGrtThan100ms(int i){
	struct timeval  tv;
	gettimeofday(&tv, NULL);
	double buff=(tv.tv_sec) * 1000 + (tv.tv_usec) / 1000 ;
	buff=(abs(timers[i]-buff));
	timers[i]=(tv.tv_sec) * 1000 + (tv.tv_usec) / 1000 ;
	return (buff>100)?true:false;
}
