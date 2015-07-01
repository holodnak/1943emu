#include <SDL/SDL.h>
#include "machine.h"
#include "system.h"
#include "sound.h"
#include "ay8910/ay8910.h"
#include "../deadz80/deadz80.h"

//1943 hardware
static deadz80_t maincpu, soundcpu;
static u8 rom[0x30000];						//main cpu rom
static u8 soundrom[0x8000];				//sound cpu rom
static u8 chars[0x8000];					//characters
static u8 bgtiles[0x40000];				//bg tiles
static u8 bg2tiles[0x10000];				//bg2 tiles
static u8 sprites[0x40000];				//sprites
static u8 tilemaps[0x10000];				//tilemaps
static u8 prom[0x1000];						//proms
static u8 ram[0x1000];						//main cpu ram
static u8 fgram[0x800];						//fg character ram
static u8 sprram[0x1000];					//sprite ram
static u8 soundram[0x800];					//ram for sound cpu
static u8 bankswitch, *bankptr;			//bankswitch stuff
static u8 palettebank;						//palette bank select
static u16 scrollx, bgscroll;				//scroll registers
static u8 scrolly;
static u8 soundlatch;						//sound latch byte
static u8 input[8];							//input + dip
static u8 bglookup[0x100];
static u8 bg2lookup[0x100];
static u8 sprlookup[0x100];
static u8 charlookup[0x100];
static u8 protect;
static int scanline;
static u8 enable;

void decode_palette()
{
	int i;
	unsigned char b0, b1, b2, b3;
	unsigned char r, g, b;

	//create the palette
	for (i = 0; i<256; i++) {

		b0 = ((prom[i] >> 0) & 1) * 0x0E;
		b1 = ((prom[i] >> 1) & 1) * 0x1F;
		b2 = ((prom[i] >> 2) & 1) * 0x43;
		b3 = ((prom[i] >> 3) & 1) * 0x8F;
		r = b0 + b1 + b2 + b3;

		b0 = ((prom[i + 256] >> 0) & 1) * 0x0E;
		b1 = ((prom[i + 256] >> 1) & 1) * 0x1F;
		b2 = ((prom[i + 256] >> 2) & 1) * 0x43;
		b3 = ((prom[i + 256] >> 3) & 1) * 0x8F;
		g = b0 + b1 + b2 + b3;

		b0 = ((prom[i + 512] >> 0) & 1) * 0x0E;
		b1 = ((prom[i + 512] >> 1) & 1) * 0x1F;
		b2 = ((prom[i + 512] >> 2) & 1) * 0x43;
		b3 = ((prom[i + 512] >> 3) & 1) * 0x8F;
		b = b0 + b1 + b2 + b3;

		palette[i] = (r << 16) | (g << 8) | b;
	}

	for (i = 0; i<256; i++) {
		charlookup[i] = 0x40 | (prom[i + 0x300] & 0x0F);
		bglookup[i] = ((prom[0x500 + i] & 0x03) << 4) | ((prom[0x400 + i] & 0x0F) << 0);
		bg2lookup[i] = ((prom[0x700 + i] & 0x03) << 4) | ((prom[0x600 + i] & 0x0F) << 0);
		sprlookup[i] = 0x80 | ((prom[0x900 + i] & 0x07) << 4) | ((prom[0x800 + i] & 0x0F) << 0);
	}
}

//from mame
static u8 protection_read()
{
	switch (protect)
	{
		// This data comes from a table at $21a containing 64 entries, even is "case", odd is return value.
	case 0x24: return 0x1d;
	case 0x60: return 0xf7;
	case 0x01: return 0xac;
	case 0x55: return 0x50;
	case 0x56: return 0xe2;
	case 0x2a: return 0x58;
	case 0xa8: return 0x13;
	case 0x22: return 0x3e;
	case 0x3b: return 0x5a;
	case 0x1e: return 0x1b;
	case 0xe9: return 0x41;
	case 0x7d: return 0xd5;
	case 0x43: return 0x54;
	case 0x37: return 0x6f;
	case 0x4c: return 0x59;
	case 0x5f: return 0x56;
	case 0x3f: return 0x2f;
	case 0x3e: return 0x3d;
	case 0xfb: return 0x36;
	case 0x1d: return 0x3b;
	case 0x27: return 0xae;
	case 0x26: return 0x39;
	case 0x58: return 0x3c;
	case 0x32: return 0x51;
	case 0x1a: return 0xa8;
	case 0xbc: return 0x33;
	case 0x30: return 0x4a;
	case 0x64: return 0x12;
	case 0x11: return 0x40;
	case 0x33: return 0x35;
	case 0x09: return 0x17;
	case 0x25: return 0x04;
	}
	return(0);
}

u8 maincpu_read(u32 addr)
{
	if (addr < 0x8000) {
		return(rom[addr]);
	}
	if (addr < 0xC000) {
		return(bankptr[addr & 0x3FFF]);
	}
	switch (addr & 0xF000) {
	case 0xC000:
		switch (addr) {
		case 0xC000:
			return(input[addr & 7] | ((scanline >= 240) ? 0 : 8));
		case 0xC001:
		case 0xC002:
		case 0xC003:
		case 0xC004:
			return(input[addr & 7]);
		case 0xC007:
			return(protection_read());
		}
		break;
	case 0xD000:
		if (addr < 0xD800) {
			return(fgram[addr & 0x7FF]);
		}
		break;
	case 0xE000:
		return(ram[addr & 0xFFF]);
	case 0xF000:
		return(sprram[addr & 0xFFF]);
	}
	printf("maincpu_read:  unhandled read at $%04X\n", addr);
	return(0);
}

void maincpu_write(u32 addr, u8 data)
{
	switch (addr & 0xF000) {
	case 0xC000:
		if (addr >= 0xCC00 && addr < 0xCC80) {
			sprram[addr & 0x7F] = data;
			return;
		}
		switch (addr) {
		case 0xC800:
			soundlatch = data;
			return;
		case 0xC804:
			bankswitch = (data & 0x1C) >> 2;
			bankptr = (u8*)rom + 0x10000 + (bankswitch * 0x4000);
			enable = (enable & 0x7F) | (data & 0x80);
			return;
		case 0xC805:
			palettebank = data;
			return;
		case 0xC806:
			bankswitch = data;
			bankptr = (u8*)rom + 0x10000 + (data * 0x4000);
			return;
		case 0xC807:
			protect = data;
			return;
		}
		break;
	case 0xD000:
		if (addr < 0xD800) {
			fgram[addr & 0x7FF] = data;
			return;
		}
		else {
			switch (addr) {
			case 0xD800:
				scrollx = (scrollx & 0xFF00) | data;
				return;
			case 0xD801:
				scrollx = (scrollx & 0x00FF) | (data << 8);
				return;
			case 0xD802:
				scrolly = data;
				return;
			case 0xD803:
				bgscroll = (bgscroll & 0xFF00) | data;
				return;
			case 0xD804:
				bgscroll = (bgscroll & 0x00FF) | (data << 8);
				return;
			case 0xD806:
				enable = (enable & 0x8F) | (data & 0x70);
				return;
			}
		}
			break;
	case 0xE000:
		ram[addr & 0xFFF] = data;
		return;
	case 0xF000:
		sprram[addr & 0xFFF] = data;
		return;
	}
	printf("maincpu_write:  unhandled write at $%04X = $%02X\n", addr, data);
}

u8 soundcpu_read(u32 addr)
{
	if (addr < 0x8000) {
		return(soundrom[addr]);
	}
	if (addr >= 0xC000 && addr < 0xC800) {
		return(soundram[addr & 0x7FF]);
	}
	if (addr == 0xC800) {
		return(soundlatch);
	}
	printf("soundcpu_read:  unhandled read at $%04X\n", addr);
	return(0);
}

void soundcpu_write(u32 addr, u8 data)
{
	if (addr >= 0xC000 && addr < 0xC800) {
		soundram[addr & 0x7FF] = data;
		return;
	}

	switch (addr) {
	case 0xE000:
	case 0xE001:
		AY8910Write(0, addr & 1, data);
		return;
	case 0xE002:
	case 0xE003:
		AY8910Write(1, addr & 1, data);
		return;
	}
	printf("soundcpu_write:  unhandled write at $%04X = $%02X\n", addr, data);
}

u8 mainirq(u8 state)
{
	printf("mainirq\n");
	deadz80_clear_irq(state);
	return(0xCF);
}

u8 soundirq(u8 state)
{
	deadz80_clear_irq(state);
	return(0xFF);
}

u8 *get_region(int flags)
{
	switch (flags & 0xFF) {
	case CPU0: return(rom);
	case CPU1: return(soundrom);
	case GFX0: return(chars);
	case GFX1: return(bgtiles);
	case GFX2: return(bg2tiles);
	case GFX3: return(sprites);
	case GFX4: return(tilemaps);
	case PROM0:return(prom);
	}
	return(0);
}

static void draw_fgtile(unsigned char *dest, unsigned char tile, unsigned char attr)
{
	int x, y;
	int index = (tile | ((attr & 0xE0) << 3)) * 16;
	u8 pixel;
	u8 *tiledata = &chars[index];

	attr = (attr & 0x1F) << 2;
	for (y = 0; y < 8; y++) {
		for (x = 3; x >= 0; x--) {
			pixel = (tiledata[0] >> (x + 4)) & 1;
			pixel |= ((tiledata[0] >> (x + 0)) & 1) << 1;
			if (pixel) {
				dest[(3 - x) + 0] = charlookup[pixel | attr];
			}
			pixel = (tiledata[1] >> (x + 4)) & 1;
			pixel |= ((tiledata[1] >> (x + 0)) & 1) << 1;
			if (pixel) {
				dest[(3 - x) + 4] = charlookup[pixel | attr];
			}
		}
		tiledata += 2;
		dest += 256;
	}
}

static void draw_sprtile(int tile, u8 attr, int sx, int sy)
{
	int x, y;
	u8 *dest = &screen[sx + sy * 256];
	int index = tile * 64;
	u8 pixel;
	u8 *tiledata0 = &sprites[index];
	u8 *tiledata1 = tiledata0 + 0x20000;
	u8 line[16];

	for (y = 0; y < 16; y++) {
		for (x = 3; x >= 0; x--) {
			pixel = ((tiledata0[0] >> (x + 4)) & 1) << 0;
			pixel |= ((tiledata0[0] >> (x + 0)) & 1) << 1;
			pixel |= ((tiledata1[0] >> (x + 4)) & 1) << 2;
			pixel |= ((tiledata1[0] >> (x + 0)) & 1) << 3;
			line[(3 - x) + 0] = pixel | attr;

			pixel = ((tiledata0[1] >> (x + 4)) & 1) << 0;
			pixel |= ((tiledata0[1] >> (x + 0)) & 1) << 1;
			pixel |= ((tiledata1[1] >> (x + 4)) & 1) << 2;
			pixel |= ((tiledata1[1] >> (x + 0)) & 1) << 3;
			line[(3 - x) + 4] = pixel | attr;

			pixel = ((tiledata0[32] >> (x + 4)) & 1) << 0;
			pixel |= ((tiledata0[32] >> (x + 0)) & 1) << 1;
			pixel |= ((tiledata1[32] >> (x + 4)) & 1) << 2;
			pixel |= ((tiledata1[32] >> (x + 0)) & 1) << 3;
			line[(3 - x) + 8] = pixel | attr;

			pixel = ((tiledata0[33] >> (x + 4)) & 1) << 0;
			pixel |= ((tiledata0[33] >> (x + 0)) & 1) << 1;
			pixel |= ((tiledata1[33] >> (x + 4)) & 1) << 2;
			pixel |= ((tiledata1[33] >> (x + 0)) & 1) << 3;
			line[(3 - x) + 12] = pixel | attr;
		}
		for (x = 0; x < 16; x++) {
			if ((sx + x) >= 256 || (sx + x) < 0) {
				continue;
			}
			if ((line[x] & 0xF) != 0x0) {
				dest[x] = sprlookup[line[x]];
			}
		}
		tiledata0 += 2;
		tiledata1 += 2;
		dest += 256;
	}

}

static void draw_bgtile(u8 *dest, u8 *tiledata0, u8 *tiledata1, u8 attr)
{
	int j, x, y, dy, inc;
	u8 pixel;
	u8 flipx, flipy;
	u8 tilebuf[32][32];

	flipy = attr & 0x80;
	flipx = attr & 0x40;

	attr = (attr & 0x3C) << 2;

	if (flipy) {
		dy = 31;
		inc = -1;
	}
	else {
		dy = 0;
		inc = 1;
	}

	for (y = 0; y < 32; y++) {
		for (x = 0; x < 32; x++) {
			tilebuf[y][x] = 0;
		}
	}

	//draw tile to buffer
	for (y = 0; y < 32; y++) {
		for (x = 3; x >= 0; x--) {
			for (j = 0; j < 32; j += 8) {
				pixel = ((tiledata0[0 + j * 8] >> (x + 4)) & 1) << 0;
				pixel |= ((tiledata0[0 + j * 8] >> (x + 0)) & 1) << 1;
				pixel |= ((tiledata1[0 + j * 8] >> (x + 4)) & 1) << 2;
				pixel |= ((tiledata1[0 + j * 8] >> (x + 0)) & 1) << 3;
				tilebuf[dy][(3 - x) + 0 + j] = pixel | attr;

				pixel = ((tiledata0[1 + j * 8] >> (x + 4)) & 1) << 0;
				pixel |= ((tiledata0[1 + j * 8] >> (x + 0)) & 1) << 1;
				pixel |= ((tiledata1[1 + j * 8] >> (x + 4)) & 1) << 2;
				pixel |= ((tiledata1[1 + j * 8] >> (x + 0)) & 1) << 3;
				tilebuf[dy][(3 - x) + 4 + j] = pixel | attr;
			}
		}

		tiledata0 += 2;
		tiledata1 += 2;
		dy += inc;
	}

	//copy tile buffer to screen
	if (flipx) {
		for (y = 0; y < 32; y++) {
			for (x = 0; x < 32; x++) {
				dest[y + x * 256] = tilebuf[y][31 - x];
			}
		}
	}
	else {
		for (y = 0; y < 32; y++) {
			for (x = 0; x < 32; x++) {
				dest[y + x * 256] = tilebuf[y][x];
			}
		}
	}
}

static u8 bgscreen[(32 * 8) * (32 * 2048)];
static u8 bg2screen[(32 * 8) * (32 * 2048)];
static u8 bg3screen[(32 * 8) * (32 * 2048) * 8];

static void draw_fg()
{
	int x, y;
	u8 *src = fgram;

	for (y = 0; y < 32; y++) {
		for (x = 0; x < 32; x++) {
			draw_fgtile(&screen[(x * 8) + (y * 8 * 256)], src[x], src[x + 0x400]);
		}
		src += 32;
	}
}

static void draw_sprites(int priority)
{
	int offs;

	for (offs = 0x1000 - 32; offs >= 0; offs -= 32) {
		u8 attr = sprram[offs + 1];
		u8 color = attr & 0x0F;
		u32 tile = sprram[offs] + ((attr & 0xE0) << 3);
		int sx = sprram[offs + 3] - ((attr & 0x10) << 4);
		int sy = sprram[offs + 2];

		if (priority) {
			if (color != 0x0A && color != 0x0B) {
				draw_sprtile(tile, color << 4, sx, sy);
			}
		}
		else {
			if (color == 0x0A || color == 0x0B) {
				draw_sprtile(tile, color << 4, sx, sy);
			}
		}
	}
}

static void draw_tilemaps()
{
	int x, y, idx, index;
	u8 *tiledata0, *tiledata1;
	u8 *tilemap;
	u32 attr, tile;

	//draw all tiles to temporary screen
	for (idx = 0, y = 0; y < 2048; y++) {
		for (x = 0; x < 8; x++, idx++) {
			index = idx * 2;
			tilemap = (u8*)tilemaps;
			attr = tilemap[index + 1];
			tile = tilemap[index] | ((attr & 1) << 8);
			tiledata0 = &bgtiles[tile * 256];
			tiledata1 = tiledata0 + 0x20000;
			draw_bgtile((u8*)bgscreen + (x * 32) + (y * 256 * 32), tiledata0, tiledata1, attr);
		}
	}

	//draw all tiles to temporary screen
	for (idx = 0, y = 0; y < 2048; y++) {
		for (x = 0; x < 8; x++, idx++) {
			index = idx * 2;
			tilemap = (u8*)tilemaps + 0x8000;
			attr = tilemap[index + 1];
			tile = tilemap[index];
			tiledata0 = &bg2tiles[tile * 256];
			tiledata1 = tiledata0 + 0x8000;
			draw_bgtile((u8*)bg2screen + (x * 32) + (y * 256 * 32), tiledata0, tiledata1, attr);
		}
	}
}

static void draw_bg()
{
	int x, y, sx, sy;
	u8 pixel;

	for (y = 0; y < 256; y++) {
		for (x = 0; x < 256; x++) {
			sx = (scrolly + x) & 0xFF;
			sy = ((y + (scrollx)) & 0xFFFF);
			pixel = bgscreen[sx + sy * 256];
			if (pixel & 0xF) {
				screen[y + x * 256] = bglookup[pixel];
			}
		}
	}

}

static void draw_bg2()
{
	int x, y, sx, sy;

	for (y = 0; y < 256; y++) {
		for (x = 0; x < 256; x++) {
			sx = x;
			sy = ((y + (bgscroll)) & 0xFFFF);
			screen[y + x * 256] = bg2lookup[bg2screen[sx + sy * 256]];
		}
	}
}

static void draw_frame()
{
	//bg2 enabled
	if (enable & 0x20) {
		draw_bg2();
	}
	else {
		memset(screen, 0, 256 * 256);
	}

	//sprites enabled
	if (enable & 0x40) {
		draw_sprites(0);
	}

	//bg enabled
	if (enable & 0x10) {
		draw_bg();
	}

	//sprites enabled
	if (enable & 0x40) {
		draw_sprites(1);
	}

	//characters enabled
	if (enable & 0x80) {
		draw_fg();
	}

}

void machine_reset()
{
	deadz80_setcontext(&maincpu);
	deadz80_reset();
	deadz80_setcontext(&soundcpu);
	deadz80_reset();

	AY8910_reset(0);
	AY8910_reset(1);

	bankswitch = 0;
	bankptr = (u8*)rom + 0x10000;
	scrollx = 0;
	scrolly = 0;
	bgscroll = 0;
	soundlatch = 0;
	enable = 0;

	draw_tilemaps();
}

void machine_init()
{
	int i;

	for (i = 0; i < 5; i++) {
		input[i] = 0xFF;
	}

	//dip switches
	input[3] = 0x0F | 0x20 | 0x40 | 0x80;
	input[4] = 0x57 | 0x80 | 0x08 | 0x20;

	scanline = 0;

	memset(&maincpu, 0, sizeof(deadz80_t));
	memset(&soundcpu, 0, sizeof(deadz80_t));

	strcpy(maincpu.tag, "main");
	strcpy(soundcpu.tag, "sound");
	deadz80_init();

	AY8910_InitClock(1500000);
	AY8910_InitAll(1500000, 44100);	//2x 1.5mhz, 44100hz output

	for (i = 0; i < Z80_NUMPAGES; i++) {
		maincpu.readfuncs[i] = maincpu_read;
		maincpu.writefuncs[i] = maincpu_write;
		soundcpu.readfuncs[i] = soundcpu_read;
		soundcpu.writefuncs[i] = soundcpu_write;
	}
	maincpu.irqfunc = mainirq;
	soundcpu.irqfunc = soundirq;

//	sound_setcallback(machine_processsound);
}

void machine_processsound(void *data,int len)
{
	INT16 *dest = (INT16*)data;
	INT16 b1[3][1024];
	INT16 b2[3][1024];
	int n;

	//	printf("processing %d bytes\n", len);
	AY8910Update(0, (INT16*)b1[0], (INT16*)b1[1], (INT16*)b1[2], len);
	AY8910Update(1, (INT16*)b2[0], (INT16*)b2[1], (INT16*)b2[2], len);

	for (n = 0; n<len; n++)
		dest[n] = (b1[0][n] + b1[1][n] + b1[2][n] + b2[0][n] + b2[1][n] + b2[2][n]) / 6;

	sound_update(dest, len);
}

void machine_frame()
{
	int mainlinecycles = 24000000 / 4 / 60 / 262;
	int soundlinecycles = 24000000 / 8 / 60 / 262;

	input[0] = input[1] = input[2] = 0;

	if (joykeys[SDLK_1])			input[0] |= 0x01;
	if (joykeys[SDLK_2])			input[0] |= 0x02;
	if (joykeys[SDLK_5])			input[0] |= 0x40;
	if (joykeys[SDLK_6])			input[0] |= 0x80;

	if (joykeys[SDLK_RIGHT])	input[1] |= 0x01;
	if (joykeys[SDLK_LEFT])		input[1] |= 0x02;
	if (joykeys[SDLK_DOWN])		input[1] |= 0x04;
	if (joykeys[SDLK_UP])		input[1] |= 0x08;
	if (joykeys[SDLK_LCTRL])	input[1] |= 0x10;
	if (joykeys[SDLK_LALT])		input[1] |= 0x20;

	input[0] ^= 0xFF;
	input[1] ^= 0xFF;
	input[2] ^= 0xFF;

	//run for one frame of 262 scanlines
	for (scanline = 0; scanline < 262; scanline++) {
		deadz80_setcontext(&maincpu);
		deadz80_execute(mainlinecycles);
		if (scanline == 240) {
			deadz80_set_irq(1);
			deadz80_irq();
		}

		deadz80_setcontext(&soundcpu);
		deadz80_execute(soundlinecycles);
//		if (scanline == 256) {
		if (scanline == 1 || scanline == 65 || scanline == 130 || scanline == 195) {
			deadz80_irq();
		}
	}
	draw_frame();
	{
		INT16 data[1024];
		machine_processsound(data,44100 / 60);
	}
}
