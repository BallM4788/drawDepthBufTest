#pragma once
#include <citro3d.h>

struct GrimTexture {
	const int width;
	const int height;
	const unsigned char *colormap;
	const unsigned char *material;
	const C3D_Tex *c3dtex;
};

//extern const struct GrimTextureClass {
//	struct GrimTexture (*new)(const int w, const int h, const unsigned char *cm, const unsigned char *mat, const C3D_Tex *tex);
//} GrimTexture;

extern const unsigned char suitcmp[0x300];
extern const unsigned char m_s_tilemat[0x400];
extern const unsigned char m_s_chestmat[0x4000];
extern const unsigned char dfltmat[0x400];
extern const unsigned char m_bonemat[0x400];
extern const unsigned char m_eyemat[0x4000];
extern const unsigned char m_jaw0mat[0x1000];
extern const unsigned char m_wristmat[0x40];
extern const unsigned char m_handmat[0x1000];
extern const unsigned char m_fingermat[0x400];
extern const unsigned char m_s_heelmat[0x2000];
extern const unsigned char m_s_toemat[0x2000];

extern C3D_Tex m_s_tiletex;
extern C3D_Tex m_s_chesttex;
extern C3D_Tex dflttex;
extern C3D_Tex m_bonetex;
extern C3D_Tex m_eyetex;
extern C3D_Tex m_jaw0tex;
extern C3D_Tex m_wristtex;
extern C3D_Tex m_handtex;
extern C3D_Tex m_fingertex;
extern C3D_Tex m_s_heeltex;
extern C3D_Tex m_s_toetex;

void initC3DTexes(void);
void deleteC3DTexes(void);

//struct GrimTexture m_s_tile = GrimTexture.new(32, 32, ptrcmp, ptrmat, ptrtex);
