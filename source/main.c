#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <3ds.h>
#include <sf2d.h>
#include <sftd.h>
#include <sfil.h>

#include "wav.h"

#include "Roboto_ttf.h"
#include "RobotoThin_ttf.h"
#include "logo_png.h"
#include "record_png.h"
#include "stop_png.h"

#define S_RATE 16360

// Multi threading stuff
Handle threadHandle, threadRequest;

#define STACKSIZE (4 * 1024)

volatile bool threadExit = false;

u32 *sharedmem = NULL, sharedmem_size = 0x30000;
u8 *audiobuf;
u32 audiobuf_size = 0x927c00, audiobuf_pos = 0;
u8 control=0x40;
u8 *nomute_audiobuf = 0;
unsigned long buf_size = 0;

int recording = 0;
int print = 0;

bool touchInCircle(touchPosition touch, int x, int y, int r){
	int tx = touch.px;
	int ty = touch.py;
	u32 kDown = hidKeysDown();
	if (kDown & KEY_TOUCH && (tx - x) * (tx - x) + (ty - y) * (ty - y) <= r * r){
		return true;
	} else {
		return false;
	}
}

// Mic thread
void threadMic(){
	while(1) {
		svcWaitSynchronization(threadRequest, U64_MAX);
		svcClearEvent(threadRequest);		

		if(threadExit) svcExitThread();
		
		touchPosition touch;
		hidTouchRead(&touch);
		u32 kDown = hidKeysDown();

		if((touchInCircle(touch, 85, 120, 35) || kDown & KEY_A) && recording == 0)
		{
			audiobuf_pos = 0;
			MIC_SetRecording(1);
			recording = 1;
		}

		if((recording == 1) && (audiobuf_pos < audiobuf_size))
		{
			audiobuf_pos+= MIC_ReadAudioData(&audiobuf[audiobuf_pos], audiobuf_size-audiobuf_pos, 0);
			if(audiobuf_pos > audiobuf_size)audiobuf_pos = audiobuf_size;
			if(audiobuf_pos >= 32704 && print == 0){
				print = 1;
			}
		}

		if((touchInCircle(touch, 165, 120, 35) || kDown & KEY_B) && recording == 1)
		{
			print = 0;
			MIC_SetRecording(0);
			recording = 2;

			//Prevent first mute second to be allocated in wav struct
			if(audiobuf_pos >= 32704){
				nomute_audiobuf =  (u8*)linearAlloc(audiobuf_pos - 32704);
				memcpy(nomute_audiobuf,&audiobuf[32704],audiobuf_pos - 32704);
				buf_size = (audiobuf_pos - 32704) / 2;
				write_wav("audio.wav", buf_size, (short int *)nomute_audiobuf, S_RATE);
			}

			GSPGPU_FlushDataCache(NULL, nomute_audiobuf, audiobuf_pos);
			recording = 0;
		}
	}
}

int main()
{
	touchPosition touch;

	sf2d_init();
	sftd_init();

	sftd_font *text = sftd_load_font_mem(Roboto_ttf, Roboto_ttf_size);
	sftd_font *title = sftd_load_font_mem(RobotoThin_ttf, RobotoThin_ttf_size);

	sf2d_texture *logo = sfil_load_PNG_buffer(logo_png, SF2D_PLACE_RAM);
	sf2d_texture *record = sfil_load_PNG_buffer(record_png, SF2D_PLACE_RAM);
	sf2d_texture *stop = sfil_load_PNG_buffer(stop_png, SF2D_PLACE_RAM);

	sf2d_set_clear_color(RGBA8(0xFA, 0xFA, 0xFA, 0xFF));

	sharedmem = (u32*)memalign(0x1000, sharedmem_size);
	audiobuf = linearAlloc(audiobuf_size);

	MIC_Initialize(sharedmem, sharedmem_size, control, 0, 3, 1, 1);//See mic.h.

	// Threading stuff
	svcCreateEvent(&threadRequest,0);
	u32 *threadStack = memalign(32, STACKSIZE);
	svcCreateThread(&threadHandle, threadMic, 0, &threadStack[STACKSIZE/4], 0x3f, 0);

	while(aptMainLoop())
	{
		hidScanInput();
		hidTouchRead(&touch);

		u32 kDown = hidKeysDown();

		if (kDown & KEY_START)
			break; // break in order to return to hbmenu

		sf2d_start_frame(GFX_TOP, GFX_LEFT);
			sf2d_draw_texture(logo, 60, 70);
			sftd_draw_text(title, 177, 80, RGBA8(0, 0, 0, 222), 40, "Audio");
			sftd_draw_text(title, 175, 120, RGBA8(0, 0, 0, 222), 40, "Recorder");
		sf2d_end_frame();

		sf2d_start_frame(GFX_BOTTOM, GFX_LEFT);
			sf2d_draw_texture(record, 85, 85);
			sf2d_draw_texture(stop, 165, 85);
		sf2d_end_frame();

		svcSignalEvent(threadRequest);

		if(print == 1)
		{
			sf2d_start_frame(GFX_TOP, GFX_LEFT);
				sf2d_draw_texture(logo, 60, 70);
				sftd_draw_text(title, 177, 80, RGBA8(0, 0, 0, 222), 40, "Audio");
				sftd_draw_text(title, 175, 120, RGBA8(0, 0, 0, 222), 40, "Recorder");
				sftd_draw_text(text, 130, 209, RGBA8(0, 0, 0, 222), 16, "Recording audio...");
			sf2d_end_frame();
		}

		if(recording == 2)
		{
			sf2d_start_frame(GFX_TOP, GFX_LEFT);
				sf2d_draw_texture(logo, 60, 70);
				sftd_draw_text(title, 177, 80, RGBA8(0, 0, 0, 222), 40, "Audio");
				sftd_draw_text(title, 175, 120, RGBA8(0, 0, 0, 222), 40, "Recorder");
				sftd_draw_text(text, 130, 209, RGBA8(0, 0, 0, 222), 16, "Saving audio...");
			sf2d_end_frame();
		}

		sf2d_swapbuffers();
	}

	MIC_Shutdown();

	sftd_free_font(text);
	sftd_free_font(title);
	sf2d_free_texture(logo);
	sf2d_free_texture(record);
	sf2d_free_texture(stop);

	// tell thread to exit
	threadExit = true;

	// signal the thread
	svcSignalEvent(threadRequest);

	// give it time to exit
	svcSleepThread(10000000ULL);

	// close handles and free allocated stack
	svcCloseHandle(threadRequest);
	svcCloseHandle(threadHandle);
	free(threadStack);

	free(sharedmem);
	linearFree(audiobuf);
	linearFree(nomute_audiobuf);

	sf2d_fini();
	sftd_fini();
	return 0;
}
