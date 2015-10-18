#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <3ds.h>

#include "wav.h"

#define S_RATE 16360

int main()
{
	u8 *framebuf;
	u32 *sharedmem = NULL, sharedmem_size = 0x30000;
	u8 *audiobuf;
	u32 audiobuf_size = 0x927c00, audiobuf_pos = 0;
	u8 control=0x40;
	u8 *nomute_audiobuf = 0;
	unsigned long buf_size = 0;

	gfxInitDefault();
	consoleInit(GFX_BOTTOM, NULL);

	printf("Init success\n");

	sharedmem = (u32*)memalign(0x1000, sharedmem_size);
	audiobuf = linearAlloc(audiobuf_size);

	MIC_Initialize(sharedmem, sharedmem_size, control, 0, 3, 1, 1);//See mic.h.

	while(aptMainLoop())
	{
		hidScanInput();
		gspWaitForVBlank();

		u32 kDown = hidKeysDown();
		if (kDown & KEY_START)
			break; // break in order to return to hbmenu

		framebuf = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);

		if(kDown & KEY_A)
		{
			audiobuf_pos = 0;
			MIC_SetRecording(1);

			memset(framebuf, 0x20, 0x46500);
		}

		if((hidKeysHeld() & KEY_A) && audiobuf_pos < audiobuf_size)
		{
			audiobuf_pos+= MIC_ReadAudioData(&audiobuf[audiobuf_pos], audiobuf_size-audiobuf_pos, 0);
			if(audiobuf_pos > audiobuf_size)audiobuf_pos = audiobuf_size;
			if(audiobuf_pos >= 32704)printf("Now recording\n");
			memset(framebuf, 0x60, 0x46500);
		}

		if(hidKeysUp() & KEY_A)
		{
			printf("Saving the recorded sample\n");
			MIC_SetRecording(0);

			//Prevent first mute second to be allocated in wav struct
			if(audiobuf_pos >= 32704){
				nomute_audiobuf =  (u8*)linearAlloc(audiobuf_pos - 32704);
				memcpy(nomute_audiobuf,&audiobuf[32704],audiobuf_pos - 32704);
				buf_size = (audiobuf_pos - 32704) / 2;
				write_wav("audio.wav", buf_size, (short int *)nomute_audiobuf, S_RATE);
			}

			GSPGPU_FlushDataCache(NULL, nomute_audiobuf, audiobuf_pos);

			memset(framebuf, 0xe0, 0x46500);

			gfxFlushBuffers();
			gfxSwapBuffers();

			framebuf = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
			memset(framebuf, 0xe0, 0x46500);
			printf("Finished\n");
		}

		gfxFlushBuffers();
		gfxSwapBuffers();
	}

	MIC_Shutdown();

	free(sharedmem);
	linearFree(audiobuf);
	linearFree(nomute_audiobuf);

	gfxExit();
	return 0;
}
