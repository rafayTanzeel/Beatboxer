#include <alsa/asoundlib.h>
#include <stdbool.h>
#include "zencape_inputCtrl.h"
#include "udp_listener.h"
#include "audioMixer.h"

int main(void)
{
	zencape_init();
	UDP_Listener_init();
	printf("Done!\n");
	return 0;
}


