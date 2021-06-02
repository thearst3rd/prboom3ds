#include <3ds.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/unistd.h>
#include <stdio.h>
#include "config.h"
#include "doomtype.h"
#include "doomstat.h"
#include "i_system.h"
#include "i_sound.h"
#include "lprintf.h"
#include "r_fps.h"
#include "d_event.h"
#include "v_video.h"
#include "m_argv.h"
#include "r_draw.h"
#include "st_stuff.h"
#include "w_wad.h"
#include "keyboard.h"
#include "d_main.h"

#ifdef DISABLE_DOUBLEBUFFER
int use_doublebuffer = 0;
#else
int use_doublebuffer = 1; // Included not to break m_misc, but not relevant to SDL
#endif
int use_fullscreen;
int desired_fullscreen;
// static SDL_Surface *screen;

extern boolean onscreen_keyboard_enabled;

unsigned char* out_buffer = NULL;

void AM_ZoomOut();
void AM_ZoomIn();

touchPosition	g_lastTouch = { 0, 0 };
touchPosition	g_currentTouch = { 0, 0 };

//0=DS Bit,1=game key, 2=menu key
int keys3ds[32][3] = {
	{ KEY_A,		KEYD_RCTRL,		KEYD_ENTER }, //bit 00
	{ KEY_B,		' ',			KEYD_ESCAPE }, //bit 01
	{ KEY_SELECT,	KEYD_ENTER,		0 }, //bit 02
	{ KEY_START,	KEYD_ESCAPE,	0 }, //bit 03
	{ KEY_DRIGHT,	KEYD_RIGHTARROW,0 }, //bit 04
	{ KEY_DLEFT,	KEYD_LEFTARROW, 0 }, //bit 05
	{ KEY_DUP,		KEYD_UPARROW,	0 }, //bit 06
	{ KEY_DDOWN,	KEYD_DOWNARROW, 0 }, //bit 07
	{ KEY_R,		'.',			0 }, //bit 08
	{ KEY_L,		',',			0 }, //bit 09
	{ KEY_X,		'x',			0 }, //bit 10
	{ KEY_Y,		'y',			'y' }, //bit 11
	{ 0, 0, 0 }, //bit 12
	{ 0, 0, 0 }, //bit 13
	{ KEY_ZL,		'[',			0 }, //bit 14
	{ KEY_ZR,		']',			0 }, //bit 15
	{ 0, 0, 0 }, //bit 16
	{ 0, 0, 0 }, //bit 17
	{ 0, 0, 0 }, //bit 18
	{ 0, 0, 0 }, //bit 19
	{ 0, 0, 0 }, //bit 20
	{ 0, 0, 0 }, //bit 21
	{ 0, 0, 0 }, //bit 22
	{ 0, 0, 0 }, //bit 23
	{ KEY_CSTICK_RIGHT,	KEYD_CSTICK_RIGHT,	0 }, //bit 24
	{ KEY_CSTICK_LEFT,	KEYD_CSTICK_LEFT,	0 }, //bit 25
	{ KEY_CSTICK_UP,	KEYD_CSTICK_UP,		0 }, //bit 26
	{ KEY_CSTICK_DOWN,	KEYD_CSTICK_DOWN,	0 }, //bit 27
	{ KEY_CPAD_RIGHT,	KEYD_CPAD_RIGHT,	0 }, //bit 28
	{ KEY_CPAD_LEFT,	KEYD_CPAD_LEFT,		0 }, //bit 29
	{ KEY_CPAD_UP,		KEYD_CPAD_UP,		0 }, //bit 30
	{ KEY_CPAD_DOWN,	KEYD_CPAD_DOWN,		0 }, //bit 31
};

int weapon_cycle(int next) {
	bool good = false;
	int weapon_index = players[displayplayer].readyweapon;
	int dir = next ? 1 : -1;
	int count = 0;
	int max_index = NUMWEAPONS - 2;

	//change to the shotgun fro easier cycling
	if (weapon_index == wp_supershotgun) {
		weapon_index = wp_shotgun;
	}

	//printf("%d ", weapon_index);

	while (!good)
	{
		weapon_index += dir;

		if (weapon_index < 0) weapon_index = max_index;
		if (weapon_index > max_index) weapon_index = 0;
		if (players[displayplayer].weaponowned[weapon_index] && P_GetAmmoLevel(&players[displayplayer], weapon_index)) good = true;
		if (count++ > NUMWEAPONS) break;
		//if (players[displayplayer].weaponowned[weapon_index]) good = true;
	}

	//change to the super shotgun if owned
	if (weapon_index == wp_shotgun && players[displayplayer].weaponowned[wp_supershotgun]) {
		weapon_index = wp_supershotgun;
	}

	//printf("-> %d %d\n", weapon_index, max_index);
	return weapon_index;
}

extern boolean setup_select; // changing an item

void DS_Controls(void) {
	touchPosition touch;

	scanKeys();	// Do DS input housekeeping
	u32 keys = keysDown();
	u32 held = keysHeld();
	u32 up = keysUp();
	int i;
	boolean altmode = menuactive && !setup_select;

	if (held & KEY_TOUCH) {
		touchRead(&touch);
	}
#if 1
	for(i=0;i<32;i++) {
		//send key down
		if (keys3ds[i][0] & keys) {
			event_t ev;
			ev.type = ev_keydown;
			ev.data1 = keys3ds[i][(altmode && keys3ds[i][2]) ? 2 : 1];
			D_PostEvent(&ev);
		}
		//send key up
		if (keys3ds[i][0] & up) {
			event_t ev;
			ev.type = ev_keyup;
			ev.data1 = keys3ds[i][(altmode && keys3ds[i][2]) ? 2 : 1];
			D_PostEvent(&ev);
		}
	}

#else
	/*if (held & KEY_B) {
		if (held & KEY_L) {
			AM_ZoomIn();
		}
		if (held & KEY_R) {
			AM_ZoomOut();
		}
	}*/

	if (keys & KEY_UP /* || ((held & KEY_TOUCH) && touch.py < 80)*/)
	{
		event_t event;
		event.type = ev_keydown;
		event.data1 = KEYD_UPARROW;
		D_PostEvent(&event);
	}

	if (keys & KEY_LEFT)
	{
		event_t event;
		event.type = ev_keydown;
		event.data1 = KEYD_LEFTARROW;
		D_PostEvent(&event);
	}

	if (keys & KEY_DOWN /* || ((held & KEY_TOUCH) && touch.py > 120)*/)
	{
		event_t event;
		event.type = ev_keydown;
		event.data1 = KEYD_DOWNARROW;
		D_PostEvent(&event);
	}

	if (keys & KEY_RIGHT)
	{
		event_t event;
		event.type = ev_keydown;
		event.data1 = KEYD_RIGHTARROW;
		D_PostEvent(&event);
	}

	if (keys & KEY_START)
	{
		event_t event;
		event.type = ev_keydown;
		event.data1 = KEYD_ESCAPE;
		D_PostEvent(&event);
	}

	if (keys & KEY_SELECT) {
		event_t event;
		event.type = ev_keydown;
		event.data1 = KEYD_ENTER;
		D_PostEvent(&event);
	}

	if (keys & KEY_A)
	{
		event_t event;
		event.type = ev_keydown;
		if (menuactive) event.data1 = KEYD_ENTER;
		else event.data1 = KEYD_RCTRL;
		D_PostEvent(&event);
	}

	if (keys & KEY_B)
	{
		event_t event;
		event.type = ev_keydown;
		if (menuactive) event.data1 = KEYD_ESCAPE;
		else event.data1 = ' ';
		D_PostEvent(&event);
	}

	if (keys & KEY_X)
	{
		event_t event;
		event.type = ev_keydown;
		event.data1 = KEYD_RSHIFT;
		D_PostEvent(&event);
	}

	if (keys & KEY_Y)
	{
		bool good = false;

		while (!good)
		{
			// Jefklak 23/11/06 - regular shotgun in DOOMII please
			if (gamemission == doom2 && weapon_index == 2)
			{
				if (!weapon_shotgun_cycled)
					weapon_shotgun_cycled = true;
				else
				{
					weapon_index++;
					weapon_shotgun_cycled = true;
				}
			}
			else
			{
				weapon_index++;
				weapon_shotgun_cycled = false;
			}

			if (weapon_index >= NUMWEAPONS) weapon_index = 0;
			if (players[displayplayer].weaponowned[weapon_index]) good = true;
		}

		event_t event;
		event.type = ev_keydown;
		event.data1 = weapons[weapon_index];
		D_PostEvent(&event);
		event.data1 = 'y';
		D_PostEvent(&event);
	}

	if (keys & KEY_R) {
		if (!(held & KEY_B)) {
			event_t event;
			event.type = ev_keydown;
			event.data1 = '.';
			D_PostEvent(&event);
		}
	}

	if (keys & KEY_L)
	{
		if (!(held & KEY_B)) {
			event_t event;
			event.type = ev_keydown;
			event.data1 = ',';
			D_PostEvent(&event);
		}
	}

	keys = keysUp();

	if (keys & KEY_UP /* || (keys & KEY_TOUCH) || ((held & KEY_TOUCH) && touch.py >= 80)*/)
	{
		event_t event;
		event.type = ev_keyup;
		event.data1 = KEYD_UPARROW;
		D_PostEvent(&event);
	}

	if (keys & KEY_LEFT)
	{
		event_t event;
		event.type = ev_keyup;
		event.data1 = KEYD_LEFTARROW;
		D_PostEvent(&event);
	}

	if (keys & KEY_DOWN /* || (keys & KEY_TOUCH) || ((held & KEY_TOUCH) && touch.py <= 120)*/)
	{
		event_t event;
		event.type = ev_keyup;
		event.data1 = KEYD_DOWNARROW;
		D_PostEvent(&event);
	}

	if (keys & KEY_RIGHT)
	{
		event_t event;
		event.type = ev_keyup;
		event.data1 = KEYD_RIGHTARROW;
		D_PostEvent(&event);
	}

	if (keys & KEY_START)
	{
		event_t event;
		event.type = ev_keyup;
		event.data1 = KEYD_ESCAPE;
		D_PostEvent(&event);
	}

	if (keys & KEY_SELECT)
	{
		event_t event;
		event.type = ev_keyup;
		event.data1 = KEYD_ENTER;
		D_PostEvent(&event);
	}

	if (keys & KEY_A)
	{
		event_t event;
		event.type = ev_keyup;
		event.data1 = KEYD_RCTRL;
		D_PostEvent(&event);
	}

	if (keys & KEY_B)
	{
		event_t event;
		event.type = ev_keyup;
		event.data1 = ' ';
		D_PostEvent(&event);
	}

	if (keys & KEY_X)
	{
		event_t event;
		event.type = ev_keyup;
		event.data1 = KEYD_RSHIFT;
		D_PostEvent(&event);
	}

	if (keys & KEY_Y)
	{
		event_t event;
		event.type = ev_keyup;
		event.data1 = weapons[weapon_index];
		D_PostEvent(&event);
		event.data1 = 'y';
		D_PostEvent(&event);
	}

	if (keys & KEY_R)
	{
		event_t event;
		event.type = ev_keyup;
		event.data1 = '.';
		D_PostEvent(&event);
	}

	if (keys & KEY_L)
	{
		event_t event;
		event.type = ev_keyup;
		event.data1 = ',';
		D_PostEvent(&event);
	}

#endif
	if (keysDown() & KEY_TOUCH)
	{
		touchRead(&g_lastTouch);
		g_lastTouch.px <<= 7;
		g_lastTouch.py <<= 7;
	}

	if (keysHeld() & KEY_TOUCH) // this is only for x axis
	{
		int dx, dy;
		event_t event;

		touchRead(&g_currentTouch);// = touchReadXY();
		// let's use some fixed point magic to improve touch smoothing accuracy
		g_currentTouch.px <<= 7;
		g_currentTouch.py <<= 7;

		dx = (g_currentTouch.px - g_lastTouch.px) >> 6;
		dy = (g_currentTouch.py - g_lastTouch.py) >> 6;

		event.type = ev_mouse;
		//event.data1 = I_SDLtoDoomMouseState(Event->motion.state);
		event.data1 = 0;
		event.data2 = dx<<5;// ((touch.px - 128) / 3) << 5;
		//event.data3 = (-(touch.py - 96) / 8) << 5;
		event.data3 = 0;// dy << 4;// (0) << 5;
		D_PostEvent(&event);
		
		g_lastTouch.px = (g_lastTouch.px + g_currentTouch.px) / 2;
		g_lastTouch.py = (g_lastTouch.py + g_currentTouch.py) / 2;
	}

}

/**
* 23/11/06 - Powersave mode added (thanks to Mr. Snowflake)
* in DOOM II, regular shotgun added to weapon index pool
**/
extern int saveStringEnter;

void I_StartTic(void) {
	if (saveStringEnter) {
#ifdef ARM9
		int key = keyboardUpdate();
		scanKeys();
		u16 keys = keysDown();
		if (keys & KEY_A) key = 10;
		if (keys & KEY_B) key = KEYD_ESCAPE;
		event_t event;
		event.type = ev_keydown;
		event.data1 = key;
		D_PostEvent(&event);
#endif
		u16 key = 0;
		u16 keys;
		
		scanKeys();
		keys = keysDown();
		if (keys & KEY_A) key = 10;
		if (keys & KEY_B) key = KEYD_ESCAPE;
		if (key) {
			event_t event;
			event.type = ev_keydown;
			event.data1 = key;
			D_PostEvent(&event);
		}
		keyboard_input();
	}
	else {
		DS_Controls();
		if (onscreen_keyboard_enabled)
			keyboard_input();
	}
}

#define	automapactive ((automapmode & am_active) != 0)

//
// I_StartFrame
//
void I_StartFrame(void)
{
	extern int      viewheight;
	extern int keyboard_visible_last;
	extern int hud_displayed;
	byte *subscreen = screens[5].data;
	static viewheight_last = 0;
	static hud_displayed_last = 0;

#define HU_HEIGHT 57
#define HU_TOP (240 - HU_HEIGHT - 1)
#define HU_ROWS (240 - HU_TOP)

	//if the automap is not active then erase the top few lines
	if (!automapactive) {
		memset(subscreen, 0, screens[5].width * 16);
	}

	//erase the bottom if the viewsize changes or the hud is active
	if (hud_displayed || 
		hud_displayed_last != hud_displayed ||
		viewheight_last > viewheight) {
		memset(subscreen + (screens[5].width * HU_TOP), 0, screens[5].width * HU_ROWS);
	}

	viewheight_last = viewheight;
	hud_displayed_last = hud_displayed;
}


void I_UpdateVideoMode(void)
{
	video_mode_t mode;
	int i;

	lprintf(LO_INFO, "I_UpdateVideoMode: %dx%d\n", SCREENWIDTH, SCREENHEIGHT);
	
	mode = I_GetModeFromString(default_videomode);
	if ((i = M_CheckParm("-vidmode")) && i<myargc - 1) {
		mode = I_GetModeFromString(myargv[i + 1]);
	}

	V_InitMode(mode);
	V_DestroyUnusedTrueColorPalettes();
	V_FreeScreens();

	I_SetRes();
	
	V_AllocScreens();

	R_InitBuffer(SCREENWIDTH, SCREENHEIGHT);
}

int I_ScreenShot(const char *fname) {
}

static int newpal = 0;
#define NO_PALETTE_CHANGE 1000

//
// I_SetPalette
//
void I_SetPalette(int pal)
{
	newpal = pal;
}

void I_CalculateRes(unsigned int width, unsigned int height)
{
	SCREENWIDTH = width;
	SCREENHEIGHT = height;
	SCREENPITCH = SCREENWIDTH;
}

// CPhipps -
// I_SetRes
// Sets the screen resolution
void I_SetRes(void)
{
	int i;

	I_CalculateRes(SCREENWIDTH, SCREENHEIGHT);

	// set first three to standard values
	for (i = 0; i<3; i++) {
		screens[i].width = SCREENWIDTH;
		screens[i].height = SCREENHEIGHT;
		screens[i].byte_pitch = 800;
		screens[i].short_pitch = 800 / V_GetModePixelDepth(VID_MODE16);
		screens[i].int_pitch = 800 / V_GetModePixelDepth(VID_MODE32);
	}

	// statusbar
	screens[4].width = SCREENWIDTH;
	screens[4].height = (ST_SCALED_HEIGHT + 1);
	screens[4].byte_pitch = 800;
	screens[4].short_pitch = 800 / V_GetModePixelDepth(VID_MODE16);
	screens[4].int_pitch = 800 / V_GetModePixelDepth(VID_MODE32);

	// subscreen
	screens[5].width = 320;
	screens[5].height = 240;
	screens[5].byte_pitch = 320;
	screens[5].short_pitch = 320 / V_GetModePixelDepth(VID_MODE16);
	screens[5].int_pitch = 320 / V_GetModePixelDepth(VID_MODE32);

	lprintf(LO_INFO, "I_SetRes: Using resolution %dx%d\n", SCREENWIDTH, SCREENHEIGHT);
}

int I_GetModeFromString(const char *modestr)
{
	video_mode_t mode;

	if (!stricmp(modestr, "15")) {
		mode = VID_MODE15;
	}
	else if (!stricmp(modestr, "15bit")) {
		mode = VID_MODE15;
	}
	else if (!stricmp(modestr, "16")) {
		mode = VID_MODE16;
	}
	else if (!stricmp(modestr, "16bit")) {
		mode = VID_MODE16;
	}
	else if (!stricmp(modestr, "32")) {
		mode = VID_MODE32;
	}
	else if (!stricmp(modestr, "32bit")) {
		mode = VID_MODE32;
	}
	else if (!stricmp(modestr, "gl")) {
		mode = VID_MODEGL;
	}
	else if (!stricmp(modestr, "OpenGL")) {
		mode = VID_MODEGL;
	}
	else if (!stricmp(modestr, "Wide")) {
		mode = VID_MODE8_WIDE;
	}
	else {
		mode = VID_MODE8;
	}
	return mode;
}

//
// I_UpdateNoBlit
//
void I_UpdateNoBlit(void)
{
}

typedef struct
{
	u8 r;
	u8 g;
	u8 b;
	u8 unused;
} Color_t;

#ifdef DS3D
uint32 ds_texture_pal;
u16 ds_palette[256];
int ds_pal_init = 1;

uint32 nextPBlock = (uint32)0;


//---------------------------------------------------------------------------------
inline uint32 aalignVal(uint32 val, uint32 to) {
	return (val & (to - 1)) ? (val & ~(to - 1)) + to : val;
}

//---------------------------------------------------------------------------------
int ndsgetNextPaletteSlot(u16 count, uint8 format) {
	//---------------------------------------------------------------------------------
	// ensure the result aligns on a palette block for this format
	uint32 result = aalignVal(nextPBlock, 1 << (4 - (format == GL_RGB4)));

	// convert count to bytes and align to next (smallest format) palette block
	count = aalignVal(count << 1, 1 << 3);

	// ensure that end is within palette video mem
	if (result + count > 0x10000)   // VRAM_F - VRAM_E
		return -1;

	nextPBlock = result + count;
	return (int)result;
}

//---------------------------------------------------------------------------------
void ndsTexLoadPalVRAM(const u16* pal, u16 count, u32 addr) {
	//---------------------------------------------------------------------------------
	vramSetBankF(VRAM_F_LCD);
	//swiCopy( pal, &VRAM_F[addr>>1] , count / 2 | COPY_MODE_WORD);
	dmaCopyWords(3, (uint32*)pal, (uint32*)&VRAM_F[addr >> 1], count << 1);
	vramSetBankF(VRAM_F_TEX_PALETTE);
}

//---------------------------------------------------------------------------------
uint32 ndsTexLoadPal(const u16* pal, u16 count, uint8 format) {
	//---------------------------------------------------------------------------------
	uint32 addr = ndsgetNextPaletteSlot(count, format);
	if (addr >= 0)
		ndsTexLoadPalVRAM(pal, count, (u32)addr);

	return addr;
}
void glColorTable(uint8 format, uint32 addr) {
	GFX_PAL_FORMAT = addr >> (4 - (format == GL_RGB4));
}
#endif

byte pal3ds[256 * 3];

static void I_UploadNewPalette(int pal)
{
	static Color_t* colours;
	static int cachedgamma;
	static size_t num_pals;

	if (V_GetMode() == VID_MODEGL)
		return;

	if ((colours == 0) || (cachedgamma != usegamma)) {
		int pplump = W_GetNumForName("PLAYPAL");
		int gtlump = (W_CheckNumForName)("GAMMATBL",ns_prboom);
		register const byte * palette = W_CacheLumpNum(pplump);
		register const byte * const gtable = (const byte *)W_CacheLumpNum(gtlump) + 256*(cachedgamma = usegamma);
		register int i;

		num_pals = W_LumpLength(pplump) / (3 * 256);
		num_pals *= 256;

		if (!colours) {
			// First call - allocate and prepare colour array
			colours = malloc(sizeof(*colours)*num_pals);
		}

		// set the colormap entries
		for (i = 0; (size_t)i<num_pals; i++) {
			colours[i].r = gtable[palette[0]];
			colours[i].g = gtable[palette[1]];
			colours[i].b = gtable[palette[2]];
			palette += 3;
		}

		W_UnlockLumpNum(pplump);
		W_UnlockLumpNum(gtlump);
		num_pals /= 256;
	}

#ifdef RANGECHECK
	if ((size_t)pal >= num_pals)
		I_Error("I_UploadNewPalette: Palette number out of range (%d>=%d)",
		pal, num_pals);
#endif

	// store the colors to the current display
	//SDL_SetColors(SDL_GetVideoSurface(), colours+256*pal, 0, 256);
	u32 i;
	u16 c;

	for (i = 0; i < 256; i++)
	{
		u8 r, g, b;

		r = (u8)colours[i + 256 * pal].r;
		g = (u8)colours[i + 256 * pal].g;
		b = (u8)colours[i + 256 * pal].b;

		pal3ds[i * 3 + 0] = b;
		pal3ds[i * 3 + 1] = g;
		pal3ds[i * 3 + 2] = r;

#ifdef DS3D
		ds_palette[i] = (1 << 15) | RGB8(r, g, b);
#endif

		// Jefklak 20/11/06 - also update lower screen palette
		//BG_PALETTE[i]=RGB8(r,g,b);
		//if(!gen_console_enable)
			//BG_PALETTE_SUB[i] = RGB8(r, g, b);
	}
#ifdef DS3D
	c = ds_palette[255];
	ds_palette[255] = ds_palette[0];
	ds_palette[0] = c;
	if (ds_pal_init) {
		ds_texture_pal = ndsTexLoadPal(ds_palette, 256, GL_RGB256);
		glColorTable(GL_RGB256, ds_texture_pal);
		ds_pal_init = 0;
	}
	else {
		DC_FlushAll();
		vramSetBankF(VRAM_F_LCD);
		dmaCopyWords(0, (uint32*)ds_palette, (uint32*)&VRAM_F[ds_texture_pal >> 1], 256 * 2);
		vramSetBankF(VRAM_F_TEX_PALETTE);
	}
#endif
}

void copy_screen(int side) {
	u16 width, height;
	u8* bufAdr = gfxGetFramebuffer(GFX_TOP, side, &height, &width);
	byte *src;
	int w, h, pitch;
	src = screens[0].data;
	pitch = screens[0].byte_pitch;

	/*if (gfxIsWide()) {
		printf("WIDE %d %d - %d %d\n", width, height, SCREENWIDTH, SCREENHEIGHT);
	}
	else {
		printf("not wide %d %d - %d %d\n", width, height, SCREENWIDTH, SCREENHEIGHT);
	}*/

	for (w = 0; w<width; w++)
	{
		for (h = 0; h<height; h++)
		{
			u32 v = (w*height + h) * 3;
			u32 v1 = ((height - 1 - h) * pitch + w);
			bufAdr[v] = pal3ds[src[v1] * 3 + 0];
			bufAdr[v + 1] = pal3ds[src[v1] * 3 + 1];
			bufAdr[v + 2] = pal3ds[src[v1] * 3 + 2];
		}
	}
}

extern enum automapmode_e automapmode; // Mode that the automap is in

#define	automapactive ((automapmode & am_active) != 0)
extern boolean automapontop;

int keyboard_top();

void copy_subscreen(int side) {
	u16 screen_width, screen_height;
	u16* bufAdr = (u16*)gfxGetFramebuffer(GFX_BOTTOM, side, &screen_height, &screen_width);
	byte *src = screens[5].data;
	int w, h;
	int sw, sh;
	int rows_to_copy;
	int row_start;
	u32 c;

	sw = screens[5].width;
	sh = screens[5].height;

	//if (automapactive && !automapontop) {
		rows_to_copy = 240;
		row_start = 0;
	//}
	//else {
	//	rows_to_copy = 66;
	//	row_start = 0;
	//}

	for (w = 0; w<320; w++)
	{
		u32 v1 = (rows_to_copy-1) * sw + w;
		u32 v = (w * 240) + row_start;
		for (h = 0; h<rows_to_copy; h++)
		{
			c = src[v1] * 3;
			switch (c) {
			case 0:
				bufAdr[v] = 0;
				break;
			default:
				bufAdr[v] = RGB8_to_565(pal3ds[c + 2], pal3ds[c + 1], pal3ds[c + 0]);
				break;
			}
			v1 -= sw;
			v++;
		}
	}
}

extern int renderframe;

void I_FinishUpdate(void)
{

#ifdef GL_DOOM
	if (V_GetMode() == VID_MODEGL) {
		// proff 04/05/2000: swap OpenGL buffers
		gld_Finish();
		return;
	}
#endif

	gfxFlushBuffers();
	gspWaitForVBlank();
	gfxSwapBuffers();
#if 0
	{
		int h = 192;
		int w = 256;
		int step = 256;
		byte *src;
		byte *dest;

		dest = (unsigned char *)BG_GFX;
		src = screens[0].data;
		for (; h>0; h--)
		{
			dmaCopy(src, dest, w);
			dest += step;
			src += screens[0].byte_pitch;
		}
	}
#endif
	/* Update the display buffer (flipping video pages if supported)
	* If we need to change palette, that implicitely does a flip */
	if (newpal != NO_PALETTE_CHANGE) {
		I_UploadNewPalette(newpal);
		newpal = NO_PALETTE_CHANGE;
	}
}

void I_ShutdownGraphics(void)
{
}

static void I_InitInputs(void)
{
	int nomouse_parm = M_CheckParm("-nomouse");

	//I_InitJoystick();
}

#ifdef DS3D
void ds_stopGL() {
	powerOff(POWER_3D_CORE | POWER_MATRIX);	// enable 3D core & geometry engine
	videoBgDisable(0);
}

void ds_initGL() {
	int x;

	glInit();
	glEnable(GL_TEXTURE_2D);
	glClearColor(0, 0, 0, 31); // BG must be opaque for AA to work
	glClearPolyID(63); // BG must have a unique polygon ID for AA to work

	glCutoffDepth(0x7FFF);
	glEnable(GL_BLEND);
	glSetToonTableRange(0, 15, RGB15(8, 8, 8));
	glSetToonTableRange(16, 31, RGB15(24, 24, 24));
	glColor3f(1, 1, 1);

	glViewport(0, 0, 255, 191);


	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
	glScalef(16.0f, 16.0f, 16.0f);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(73.74, 256.0 / 192.0, 0.005, 40.0);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glEnable(GL_FOG);
	glFogShift(10);
	glFogColor(0, 0, 0, 31);
	glFogDensity(0, 8);
	for (x = 1; x<16; x++)
		glFogDensity(x, x * 8);
	for (; x<32; x++)
		glFogDensity(x, 127);
	glFogOffset(0);
	videoBgEnable(0);
}
#endif

void I_InitGraphics(void)
{
	char titlebuffer[2048];
	static int    firsttime = 1;

	if (firsttime)
	{
		firsttime = 0;

		atexit(I_ShutdownGraphics);
		lprintf(LO_INFO, "I_InitGraphics: %dx%d\n", SCREENWIDTH, SCREENHEIGHT);

		/* Set the video mode */
		I_UpdateVideoMode();

		/* Initialize the input system */
		I_InitInputs();
	}
}
