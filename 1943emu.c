#include <SDL/SDL.h>
#include "romset.h"
#include "machine.h"
#include "system.h"
#include "../deadz80/deadz80.h"

int main(int argc, char *argv[])
{
	screenw = 256;
	screenh = 256;
	screenbpp = 32;

	system_init();
	video_init();

	machine_init();

	if (romset_load(&r_1943, ".\\1943") != 0) {
		system("pause");
		return(0);
	}

	decode_palette();

	machine_reset();

	while (quit == 0) {
		system_checkevents();
		system_poll();
		machine_frame();
		system_frame();
	}

	video_kill();
	system_kill();

	system("pause");
	return(0);
}
