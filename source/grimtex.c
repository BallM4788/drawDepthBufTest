#include "grimtex.h"
#include "texdata.h"
#include "swizzle.h"
#include <stdio.h>

#define CONSTRUCT_TEX(t, w, h) \
	C3D_TexInit(&t##tex, w, h, GPU_RGBA8); \
	C3D_TexSetFilter(&t##tex, GPU_LINEAR, GPU_LINEAR); \
	C3D_TexSetWrap(&t##tex, GPU_REPEAT, GPU_REPEAT);\
	void *t##linear = linearAlloc(w * h * 4); \
	char *t##Constructed = (char*)t##linear; \
	makeTex(t##mat, t##Constructed, suitcmp, w, h, false); \
	if (w > 32 && h > 32) { \
		GSPGPU_FlushDataCache(t##linear, w * h * 4); \
		C3D_SyncDisplayTransfer((u32 *)t##Constructed, (u32)GX_BUFFER_DIM(w, h), (u32 *)t##tex.data, (u32)GX_BUFFER_DIM(w, h), 3); \
		C3D_TexFlush(&t##tex); \
	} else { \
		swizzle((u32 *)t##Constructed, (u32 *)t##tex.data, w, h, 0, 0, w, h, 0, 0, w, h, GPU_RGBA8, false, false); \
	} \
	linearFree(t##linear);

//static const struct GrimTexture new(const int w, const int h, const unsigned char *cm, const unsigned char *mat, const C3D_Tex *tex) {
//	return (struct GrimTexture){.width=w, .height=h, .colormap=cm, .material=mat, .c3dtex=tex};
//}
//const struct GrimTextureClass GrimTexture={.new=&new};

C3D_Tex m_s_tiletex;
C3D_Tex m_s_chesttex;
C3D_Tex dflttex;
C3D_Tex m_bonetex;
C3D_Tex m_eyetex;
C3D_Tex m_jaw0tex;
C3D_Tex m_wristtex;
C3D_Tex m_handtex;
C3D_Tex m_fingertex;
C3D_Tex m_s_heeltex;
C3D_Tex m_s_toetex;

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

static void makeTex(const u8 *in, char *out, const u8 *cmap, int w, int h, bool hasAlpha) {
	int bytes = 4;

	char *texdatapos = out;

	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			u8 col = *(const u8 *)(in);
			if (col == 0) {
				memset(texdatapos, 0, bytes); // transparent
				if (!hasAlpha) {
					//texdatapos[3] = '\xff'; // fully opaque
					texdatapos[(w < 32 && h < 32) ? 3 : 0] = '\xff'; // fully opaque
				}
			} else {
				//memcpy(texdatapos, cmap + 3 * (col), 3);
				//texdatapos[3] = '\xff'; // fully opaque
				texdatapos[(w <= 32 && h <= 32) ? 0 : 3] = (cmap + 3 * (col))[0];
				texdatapos[(w <= 32 && h <= 32) ? 1 : 2] = (cmap + 3 * (col))[1];
				texdatapos[(w <= 32 && h <= 32) ? 2 : 1] = (cmap + 3 * (col))[2];
				texdatapos[(w <= 32 && h <= 32) ? 3 : 0] = '\xff'; // fully opaque
			}
			texdatapos += bytes;
			in++;
		}
	}
}

void initC3DTexes(void) {
	CONSTRUCT_TEX(m_s_tile, 32, 32)
	printf("m_s_tiletex created\n");
	CONSTRUCT_TEX(m_s_chest, 128, 128)
	printf("m_s_chesttex created\n");
	CONSTRUCT_TEX(dflt, 32, 32)
	printf("dflttex created\n");
	CONSTRUCT_TEX(m_bone, 32, 32)
	printf("m_bonetex created\n");
	CONSTRUCT_TEX(m_eye, 128, 128)
	printf("m_eyetex created\n");
	CONSTRUCT_TEX(m_jaw0, 64, 64)
	printf("m_jaw0tex created\n");
	CONSTRUCT_TEX(m_wrist, 8, 8)
	printf("m_wristtex created\n");
	CONSTRUCT_TEX(m_hand, 64, 64)
	printf("m_handtex created\n");
	CONSTRUCT_TEX(m_finger, 32, 32)
	printf("m_fingertex created\n");
	CONSTRUCT_TEX(m_s_heel, 128, 64)
	printf("m_s_heeltex created\n");
	CONSTRUCT_TEX(m_s_toe, 128, 64)
	printf("m_s_toetex created\n");
}

void deleteC3DTexes(void) {
	C3D_TexDelete(&m_s_tiletex);
	C3D_TexDelete(&m_s_chesttex);
	C3D_TexDelete(&dflttex);
	C3D_TexDelete(&m_bonetex);
	C3D_TexDelete(&m_eyetex);
	C3D_TexDelete(&m_jaw0tex);
	C3D_TexDelete(&m_wristtex);
	C3D_TexDelete(&m_handtex);
	C3D_TexDelete(&m_fingertex);
	C3D_TexDelete(&m_s_heeltex);
	C3D_TexDelete(&m_s_toetex);
}
