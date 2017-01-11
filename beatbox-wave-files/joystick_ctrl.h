
#ifndef JOYSTICK_CTRL_H_
#define JOYSTICK_CTRL_H_

void readFile(char* filePath, char* buffer, const int size); //Reads content of file
void writeFile(char* filePath, char* input); //Writes content to file

void joystick_init();
void* joystickThread(void* arg);
int joystick_getMovement();

void readFile(char* filePath, char* buffer, const int size);






#endif /* JOYSTICK_CTRL_H_ */
