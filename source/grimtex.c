#include "grimtex.h"
#include "texdata.h"
#include "swizzle.h"

#define CONSTRUCT_TEX(t, w, h) \
	C3D_TexInit(&t##tex, w, h, GPU_RGBA8); \
	C3D_TexSetFilter(&t##tex, GPU_LINEAR, GPU_LINEAR); \
	C3D_TexSetWrap(&t##tex, GPU_REPEAT, GPU_REPEAT);\
	char t##Constructed[w * h * 4]; \
	makeTex(t##mat, t##Constructed, suitcmp, w, h, false); \
	swizzle((u32 *)t##Constructed, (u32 *)t##tex.data, w, h, 0, 0, w, h, 0, 0, w, h, GPU_RGBA8, false, false);
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
					texdatapos[3] = '\xff'; // fully opaque
				}
			} else {
				memcpy(texdatapos, cmap + 3 * (col), 3);
				texdatapos[3] = '\xff'; // fully opaque
			}
			texdatapos += bytes;
			in++;
		}
	}
}

void initC3DTexes(void) {
	CONSTRUCT_TEX(m_s_tile, 32, 32)
	CONSTRUCT_TEX(m_s_chest, 128, 128)
	CONSTRUCT_TEX(dflt, 32, 32)
	CONSTRUCT_TEX(m_bone, 32, 32)
	CONSTRUCT_TEX(m_eye, 128, 128)
	CONSTRUCT_TEX(m_jaw0, 64, 64)
	CONSTRUCT_TEX(m_wrist, 8, 8)
	CONSTRUCT_TEX(m_hand, 64, 64)
	CONSTRUCT_TEX(m_finger, 32, 32)
	CONSTRUCT_TEX(m_s_heel, 128, 64)
	CONSTRUCT_TEX(m_s_toe, 128, 64)
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
