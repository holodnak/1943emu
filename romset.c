#include <stdio.h>
#include "romset.h"
#include "machine.h"

romset_t r_1943 = {
	"1943",
	{
		//main cpu roms
		{ "bme01.12d", 0x00000, 0x08000, 0x55fd447e, CPU0 },
		{ "bme02.13d", 0x10000, 0x10000, 0X073fc57c, CPU0 },
		{ "bme03.14d", 0x20000, 0x10000, 0x835822c2, CPU0 },

		//sound cpu roms
		{"bm04.5h", 0x00000, 0x8000, 0xee2bd2d7, CPU1 },

		//mcu roms
//		{"bm.7k", 0x00000,0x10000 , 0x00000000, CPU2 },

		//graphics roms
		{ "bm05.4k", 0x00000, 0x8000, 0x46cb9d3d, GFX0 },

		{ "bm15.10f", 0x00000, 0x8000, 0x6b1a0443, GFX1 },
		{ "bm16.11f", 0x08000, 0x8000, 0x23c908c2, GFX1 },
		{ "bm17.12f", 0x10000, 0x8000, 0x46bcdd07, GFX1 },
		{ "bm18.14f", 0x18000, 0x8000, 0xe6ae7ba0, GFX1 },
		{ "bm19.10j", 0x20000, 0x8000, 0x868ababc, GFX1 },
		{ "bm20.11j", 0x28000, 0x8000, 0x0917e5d4, GFX1 },
		{ "bm21.12j", 0x30000, 0x8000, 0x9bfb0d89, GFX1 },
		{ "bm22.14j", 0x38000, 0x8000, 0x04f3c274, GFX1 },

		{ "bm24.14k", 0x00000, 0x8000, 0x11134036, GFX2 },
		{ "bm25.14l", 0x08000, 0x8000, 0x092cf9c1, GFX2 },

		{ "bm06.10a", 0x00000, 0x8000, 0x97acc8af, GFX3 },
		{ "bm07.11a", 0x08000, 0x8000, 0xd78f7197, GFX3 },
		{ "bm08.12a", 0x10000, 0x8000, 0x1a626608, GFX3 },
		{ "bm09.14a", 0x18000, 0x8000, 0x92408400, GFX3 },
		{ "bm10.10c", 0x20000, 0x8000, 0x8438a44a, GFX3 },
		{ "bm11.11c", 0x28000, 0x8000, 0x6c69351d, GFX3 },
		{ "bm12.12c", 0x30000, 0x8000, 0x5e7efdb7, GFX3 },
		{ "bm13.14c", 0x38000, 0x8000, 0x1143829a, GFX3 },

		{ "bm14.5f", 0x0000, 0x8000, 0x4d3c6401, GFX4 },
		{ "bm23.8k", 0x8000, 0x8000, 0xa52aecbd, GFX4 },

		{ "bm1.12a",  0x0000, 0x0100, 0x74421f18, PROM0 },
		{ "bm2.13a",  0x0100, 0x0100, 0xac27541f, PROM0 },
		{ "bm3.14a",  0x0200, 0x0100, 0x251fb6ff, PROM0 },
		{ "bm5.7f",   0x0300, 0x0100, 0x206713d0, PROM0 },
		{ "bm10.7l",  0x0400, 0x0100, 0x33c2491c, PROM0 },
		{ "bm9.6l",   0x0500, 0x0100, 0xaeea4af7, PROM0 },
		{ "bm12.12m", 0x0600, 0x0100, 0xc18aa136, PROM0 },
		{ "bm11.12l", 0x0700, 0x0100, 0x405aae37, PROM0 },
		{ "bm8.8c",   0x0800, 0x0100, 0xc2010a9e, PROM0 },
		{ "bm7.7c",   0x0900, 0x0100, 0xb56f30c3, PROM0 },
		{ "bm4.12c",  0x0a00, 0x0100, 0x91a8a2e1, PROM0 },
		{ "bm6.4b",   0x0b00, 0x0100, 0x0eaf5158, PROM0 },
		{ 0,0,0,0,0 }
	}
};

int romset_load(romset_t *romset, char *path)
{
	rom_t *rom;				//currently being loaded rom
	int i, n;
	FILE *fp;
	char fn[1024];
	u8 *ptr;

	for (i = 0;; i++) {
		rom = &romset->roms[i];
		if (rom->filename == 0)
			break;
		sprintf(fn, "%s\\%s", path, rom->filename);
		if ((fp = fopen(fn, "rb")) == 0) {
			printf("error opening rom '%s'\n", fn);
			return(1);
		}
		ptr = get_region(rom->flags);
		fread(ptr + rom->loadaddr, rom->size, 1, fp);
		fclose(fp);
		printf("loaded '%s' at $%05X\n", rom->filename, rom->loadaddr);
	}

	return(0);
}
