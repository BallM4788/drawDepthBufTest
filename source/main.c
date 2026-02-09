#include <3ds.h>
#include <citro3d.h>
#include <tex3ds.h>
#include <stdio.h>
#include <inttypes.h>
#include "outer_shbin.h"
#include "innerVxcolor_shbin.h"
#include "innerTexture_shbin.h"
#include "manny_shbin.h"
#include "backgroundvals.h"
#include "depthvals.h"
#include "meshes.h"
#include "grimtex.h"
#include "swizzle.h"

#define DISPLAY_TRANSFER_FLAGS \
	(GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) | \
	GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGB8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | \
	GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

u32 __stacksize__ = 10 * 1024 * 1024;

C3D_Tex *derp = &m_s_tiletex;

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

static DVLB_s							*outer_dvlb;
static shaderProgram_s					 outer_program;
static int								 outer_uLoc_modelView,
										 outer_uLoc_projection;
static C3D_Mtx							 outer_mtx_modelView,
										 outer_mtx_projection;
static C3D_AttrInfo						 outer_attrInfo;
static C3D_BufInfo						 outer_bufInfo;
static C3D_TexEnv						 outer_texEnv;
static C3D_Tex							 outer_texture;
static void								*outer_VBO;

static void initOuter(void) {
	// load shader, create program
	outer_dvlb = DVLB_ParseFile((u32 *)outer_shbin, outer_shbin_size);
	shaderProgramInit(&outer_program);
	shaderProgramSetVsh(&outer_program, &outer_dvlb->DVLE[0]);

	// get uniform locations
	outer_uLoc_modelView = shaderInstanceGetUniformLocation(outer_program.vertexShader, "modelView");
	outer_uLoc_projection = shaderInstanceGetUniformLocation(outer_program.vertexShader, "projection");

	// set modelView matrix
	Mtx_Identity(&outer_mtx_modelView);
	Mtx_Scale(&outer_mtx_modelView, 0.5f, 0.5f, 1.f);
	//Mtx_Translate(&outer_mtx_modelView, 0, 0, 0, true);

	// set projection matrix
	Mtx_Identity(&outer_mtx_projection);
	Mtx_OrthoTilt(&outer_mtx_projection, 0.0, 320.0, 240.0, 0.0, 0.0, 1.0, true);

	// set attrinfo
	AttrInfo_Init(&outer_attrInfo);
	AttrInfo_AddLoader(&outer_attrInfo, 0 /*position*/, GPU_FLOAT, 3);
	AttrInfo_AddLoader(&outer_attrInfo, 1 /*texcoord*/, GPU_FLOAT, 2);

	// make VBO
	float outer_vertices[20] = {
	//	    X      Y     Z    S       T
		  0.f,   0.f, 0.5f, 0.f,    0.f,
		640.f,   0.f, 0.5f, 0.625f, 0.f,
		  0.f, 480.f, 0.5f, 0.f,    0.9375f,
		640.f, 480.f, 0.5f, 0.625f, 0.9375f
	};
	outer_VBO = linearAlloc(sizeof(outer_vertices));
	memcpy(outer_VBO, outer_vertices, sizeof(outer_vertices));

	// set bufinfo
	BufInfo_Init(&outer_bufInfo);
	BufInfo_Add(&outer_bufInfo, outer_VBO, sizeof(float) * 5, 2, 0x10);

	// configure texenv
	C3D_TexEnvInit(&outer_texEnv);
	C3D_TexEnvSrc(&outer_texEnv, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
	C3D_TexEnvOpRgb(&outer_texEnv, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR);
	C3D_TexEnvFunc(&outer_texEnv, C3D_Both, GPU_REPLACE);

	// create game screen texture
	C3D_TexInitVRAM(&outer_texture, 1024, 512, GPU_RGBA8);
	C3D_TexSetFilter(&outer_texture, GPU_LINEAR, GPU_LINEAR);

	//// SAVE THESE FOR RENDERING
	//C3D_DepthTest(false, GPU_GEQUAL, GPU_WRITE_ALL);
	//C3D_CullFace(GPU_CULL_NONE);
}

static void exitOuter(void) {
	C3D_TexDelete(&outer_texture);
	linearFree(outer_VBO);
	shaderProgramFree(&outer_program);
	DVLB_Free(outer_dvlb);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

static DVLB_s							*inner_vxcolor_dvlb;
static shaderProgram_s					 inner_vxcolor_program;
static int								 inner_vxcolor_uLoc_zPosition;
       float							 inner_vxcolor_flt_zPosition;
       float							 inner_vxcolor_flt_zInterval;
static C3D_AttrInfo						 inner_vxcolor_attrInfo;
static C3D_BufInfo						 inner_vxcolor_bufInfo;
static C3D_TexEnv						 inner_vxcolor_texEnv;
static void								*inner_vxcolor_VBO;

static void inner_vxcolor_init(void) {
	// load shader, create program
	inner_vxcolor_dvlb = DVLB_ParseFile((u32 *)innerVxcolor_shbin, innerVxcolor_shbin_size);
	shaderProgramInit(&inner_vxcolor_program);
	shaderProgramSetVsh(&inner_vxcolor_program, &inner_vxcolor_dvlb->DVLE[0]);

	// get uniform locations
	inner_vxcolor_uLoc_zPosition = shaderInstanceGetUniformLocation(inner_vxcolor_program.vertexShader, "zPosition");

	// set initial value for zPosition float and interval
	inner_vxcolor_flt_zPosition = 1.f;
	inner_vxcolor_flt_zInterval = 12.8f / 65535;

	// set attrinfo
	AttrInfo_Init(&inner_vxcolor_attrInfo);
	AttrInfo_AddLoader(&inner_vxcolor_attrInfo, 0 /*position*/, GPU_FLOAT, 2);
	AttrInfo_AddFixed(&inner_vxcolor_attrInfo, 1 /*color*/);

	// make VBO
	float inner_vxcolor_vertices[8] = {
	//	  X    Y
		-1.f, -1.f,
		 1.f, -1.f,
		 1.f,  1.f,
		-1.f,  1.f,
	};
	inner_vxcolor_VBO = linearAlloc(sizeof(inner_vxcolor_vertices));
	memcpy(inner_vxcolor_VBO, inner_vxcolor_vertices, sizeof(inner_vxcolor_vertices));

	// set bufinfo
	BufInfo_Init(&inner_vxcolor_bufInfo);
	BufInfo_Add(&inner_vxcolor_bufInfo, inner_vxcolor_VBO, sizeof(float) * 2, 1, 0x0);

	// config vxcolor texenv
	C3D_TexEnvInit(&inner_vxcolor_texEnv);
	C3D_TexEnvFunc(&inner_vxcolor_texEnv, C3D_Both, GPU_REPLACE);
	C3D_TexEnvSrc(&inner_vxcolor_texEnv, C3D_Both, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
	C3D_TexEnvOpRgb(&inner_vxcolor_texEnv, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_ALPHA);
	C3D_TexEnvOpAlpha(&inner_vxcolor_texEnv, GPU_TEVOP_A_SRC_ALPHA, GPU_TEVOP_A_SRC_ALPHA, GPU_TEVOP_A_SRC_ALPHA);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

static DVLB_s							*inner_texture_dvlb;
static shaderProgram_s					 inner_texture_program;
static int								 inner_texture_uLoc_texcrop,
										 inner_texture_uLoc_scale,
										 inner_texture_uLoc_offset;
static C3D_AttrInfo						 inner_texture_attrInfo;
static C3D_BufInfo						 inner_texture_bufInfo;
static C3D_TexEnv						 inner_texture_texEnv;
static void								*inner_texture_VBO;
static void								*whiteRectangle;

static void inner_texture_init(void) {
	// load shader, create program
	inner_texture_dvlb = DVLB_ParseFile((u32 *)innerTexture_shbin, innerTexture_shbin_size);
	shaderProgramInit(&inner_texture_program);
	shaderProgramSetVsh(&inner_texture_program, &inner_texture_dvlb->DVLE[0]);

	// get uniform locations
	inner_texture_uLoc_texcrop = shaderInstanceGetUniformLocation(inner_texture_program.vertexShader, "texcrop");
	inner_texture_uLoc_scale = shaderInstanceGetUniformLocation(inner_texture_program.vertexShader, "scale");
	inner_texture_uLoc_offset = shaderInstanceGetUniformLocation(inner_texture_program.vertexShader, "offset");

	// set attrinfo
	AttrInfo_Init(&inner_texture_attrInfo);
	AttrInfo_AddLoader(&inner_texture_attrInfo, 0 /*position*/, GPU_FLOAT, 2);
	AttrInfo_AddLoader(&inner_texture_attrInfo, 1 /*texcoord*/, GPU_FLOAT, 2);

	// make VBO
	float inner_texture_vertices[16] = {
	//	  X    Y    S    T
		0.f, 0.f, 0.f, 0.f,
		1.f, 0.f, 1.f, 0.f,
		1.f, 1.f, 1.f, 1.f,
		0.f, 1.f, 0.f, 1.f,
	};
	inner_texture_VBO = linearAlloc(sizeof(inner_texture_vertices));
	memcpy(inner_texture_VBO, inner_texture_vertices, sizeof(inner_texture_vertices));

	// set bufinfo
	BufInfo_Init(&inner_texture_bufInfo);
	BufInfo_Add(&inner_texture_bufInfo, inner_texture_VBO, sizeof(float) * 4, 2, 0x10);

	// config texture texenv
	C3D_TexEnvInit(&inner_texture_texEnv);
	C3D_TexEnvFunc(&inner_texture_texEnv, C3D_Both, GPU_REPLACE);
	C3D_TexEnvSrc(&inner_texture_texEnv, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
	C3D_TexEnvOpRgb(&inner_texture_texEnv, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_ALPHA);
	C3D_TexEnvOpAlpha(&inner_texture_texEnv, GPU_TEVOP_A_SRC_ALPHA, GPU_TEVOP_A_SRC_ALPHA, GPU_TEVOP_A_SRC_ALPHA);

	whiteRectangle = linearAlloc(32 * 64 * 4);
	memset(whiteRectangle, 0xffffff, 32 * 64 * 4);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

static void								*inner_depthSwizzled;
static void								*inner_EBO;
static C3D_Tex							 inner_C3DTex;
static C3D_Tex							 tube_can_C3DTex;
static u32								 background_depthAdjusted[BACKGROUND_DEPTH_LENGTH];
static u32								 tube_can_depthAdjusted[TUBE_CAN_DEPTH_LENGTH];

static void adjustDepths(unsigned char *vals, int length, u32 *valsAdjusted) {
	u16 *valsPtr = (u16 *)vals;
	for (int i = 0; i < length; i++) {
		u32 val = (u32)((float)valsPtr[i] / 0xffff * 0xffffff);
		if (val == 0xf81ff7) {
			val = 0;
		}
		//valsAdjusted[i] = 0xFFFFFF - val;
		//scummvm compresses the depth values in the grim engine's opengl renderers, not sure if it'll be needed then
		valsAdjusted[i] = 0xffffff - ((u64)val) * 0x1000000 / 100 / (0x1000000 - val);
	}
}

static void initInner(void) {
	inner_vxcolor_init();
	inner_texture_init();

	inner_EBO = linearAlloc(sizeof(unsigned short) * 6);
	unsigned short *eboShort = (unsigned short *)inner_EBO;
	eboShort[0] = eboShort[3] = 0;
	eboShort[1] = 1;
	eboShort[2] = eboShort[4] = 2;
	eboShort[5] = 3;
	// create background texture
	C3D_TexInit(&inner_C3DTex, 1024, 512, GPU_RGBA8);
	C3D_TexInit(&tube_can_C3DTex, 512, 256, GPU_RGBA8);
	// swizzle values into texture; can't use tex3ds to textures
	unsigned char background_color[] = { BACKGROUND_COLOR };
	unsigned char tube_can_color[] = { TUBE_CAN_COLOR };
	swizzle((u32 *)background_color, (u32 *)inner_C3DTex.data, BACKGROUND_COLOR_WIDTH, BACKGROUND_COLOR_HEIGHT,
		0, 0, BACKGROUND_COLOR_WIDTH, BACKGROUND_COLOR_HEIGHT,
		0, 0, 1024, 512,
		GPU_RGBA8, false, false);
	swizzle((u32 *)tube_can_color, (u32 *)tube_can_C3DTex.data, TUBE_CAN_COLOR_WIDTH, TUBE_CAN_COLOR_HEIGHT,
		0, 0, TUBE_CAN_COLOR_WIDTH, TUBE_CAN_COLOR_HEIGHT,
		0, 0, 512, 256,
		GPU_RGBA8, false, false);
	C3D_TexSetFilter(&inner_C3DTex, GPU_NEAREST, GPU_NEAREST);
	C3D_TexSetWrap(&inner_C3DTex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
	C3D_TexSetFilter(&tube_can_C3DTex, GPU_NEAREST, GPU_NEAREST);
	C3D_TexSetWrap(&tube_can_C3DTex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);

	inner_depthSwizzled = linearAlloc(1024 * 512 * sizeof(u32));

	unsigned char background_depth[] = { BACKGROUND_DEPTH };
	unsigned char tube_can_depth[] = { TUBE_CAN_DEPTH };

	adjustDepths(background_depth, BACKGROUND_DEPTH_LENGTH, background_depthAdjusted);
	adjustDepths(tube_can_depth, TUBE_CAN_DEPTH_LENGTH, tube_can_depthAdjusted);

//	u16 *background_depthPtr = (u16 *)background_depth;
//	for (int i = 0; i < BACKGROUND_DEPTH_LENGTH; i++) {
//		u16 val = background_depthPtr[i];
////		if (val == 0xf81f) {
////			val = 0;
////		}
//		background_depthAdjusted[i] = 0xFFFFFF - (u32)((float)val / 0xFFFF * 0xFFFFFF);
//		//scummvm compresses the depth values in the grim engine's opengl renderers, not sure if it'll be needed then
//		//background_depthAdjusted[i] = 0xffffff - ((u64)val << 8) * 0x1000000 / 100 / (0x1000000 - (val << 8));
//	}
}

static void exitInner(void) {
	linearFree(inner_depthSwizzled);
	C3D_TexDelete(&tube_can_C3DTex);
	C3D_TexDelete(&inner_C3DTex);
	linearFree(inner_EBO);

	linearFree(inner_texture_VBO);
	shaderProgramFree(&inner_texture_program);
	DVLB_Free(inner_texture_dvlb);
	linearFree(inner_vxcolor_VBO);
	shaderProgramFree(&inner_vxcolor_program);
	DVLB_Free(inner_vxcolor_dvlb);

	linearFree(whiteRectangle);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

static DVLB_s							*manny_dvlb;
static shaderProgram_s					 manny_program;
static int							 manny_uLoc_modelMatrix,
								 manny_uLoc_viewMatrix,
								 manny_uLoc_projMatrix,
								 manny_uLoc_extraMatrix,
								 manny_uLoc_texScale;
static C3D_Mtx						 manny_mtx_extraMatrix,
								 manny_mtx_modelMatrix,
								 manny_mtx_viewMatrix,
								 manny_mtx_projMatrix;
static C3D_FVec						 manny_vec_texScale;
static C3D_AttrInfo						 manny_attrInfo;
static C3D_BufInfo						 manny_bufInfo;
static C3D_TexEnv						 manny_texEnv;
//static C3D_Tex						 manny_texture;
static void							*manny_VBO;

static void constructMannyMatrices(void) 
{

// model matrix
	// manny start pos: 1.32737005,  1.59880996, 0
	// manny start rot: 0, 50.3818016, 0
	// manny tube pos:  0.741680026, 2.09709001, 0
	// manny tube rot:  0, 11.3445997, 0
	
	C3D_FVec posVec = FVec3_New(0.741680026f, 2.09709001f, 0.f);		// tube
	//C3D_FVec posVec = FVec3_New(1.32737005f, 1.59880996f, 0.f);		// start

	// actor rotation into actorRotQuat
	C3D_Mtx actorRotMtx;
	Mtx_Identity(&actorRotMtx);
	Mtx_RotateZ(&actorRotMtx, C3D_AngleFromDegrees(11.3445997f), true);	// tube
	//Mtx_RotateZ(&actorRotMtx, C3D_AngleFromDegrees(50.3818016f), true);	// start
	C3D_FQuat actorRotQuat = Quat_Inverse(Quat_FromMtx(&actorRotMtx));
	Mtx_FromQuat(&manny_mtx_modelMatrix, actorRotQuat);
	Mtx_Transpose(&manny_mtx_modelMatrix);
	manny_mtx_modelMatrix.r[0].w = posVec.x;
	manny_mtx_modelMatrix.r[1].w = posVec.y;
	manny_mtx_modelMatrix.r[2].w = posVec.z;
	// 0.637669       -0.770311       0.000000        1.327370
	// 0.770311        0.637669       0.000000        1.598810
	// 0.000000        0.000000       1.000000        0.000000
	// 0.000000        0.000000       0.000000        1.000000

// view matrix
	//camera position: 0.571882, 2.461178, 0.362781
	//camera interest: 1.089404, 1.127796, 0.217699
	//camera roll:     0.000000
	C3D_FVec camPositionVec = FVec3_New(0.571882f, 2.461178f, 0.362781f);
	C3D_FVec camInterestVec = FVec3_New(1.089404f, 1.127796f, 0.217699f);
	float    camRoll____Flt = 0.f;

	// base view matrix
	Mtx_Identity(&manny_mtx_viewMatrix);
	C3D_FVec viewAxisVec = FVec3_New(0.f, 0.f, 1.f);
	Mtx_Rotate(&manny_mtx_viewMatrix, viewAxisVec, camRoll____Flt, true);

	// look matrix
	C3D_FVec camLookinUpVec = FVec3_New(0.f, 0.f, 1.f);
	C3D_Mtx lookMtx;
	Mtx_LookAt(&lookMtx, camPositionVec, camInterestVec, camLookinUpVec, false);
	Mtx_Transpose(&lookMtx);

	// final view matrix
	Mtx_Multiply(&manny_mtx_viewMatrix, &manny_mtx_viewMatrix, &lookMtx);
	Mtx_Transpose(&manny_mtx_viewMatrix);
	//-0.932244       -0.361829       0.000000        1.423661
	// 0.036515       -0.094080       0.994895       -0.150264
	//-0.359982        0.927485       0.100917       -2.113449
	// 0.000000        0.000000       0.000000        1.000000

//proj matrix
	// scene fov: 75.178040
	// scene nclip: 0.01
	// scene fclip: 3276.8
	float fov = 75.178040f;
	float nclip = 0.01f;
	float fclip = 3276.8f;

	float right = nclip * tan(fov / 2 * ((float)M_PI / 180));
	float top = right * 0.75;
//	manny_mtx_projMatrix.r[0].x = (2.0f * nclip) / (right - left);
	manny_mtx_projMatrix.r[0].x = (2.0f * nclip) / (right * 2);
//	manny_mtx_projMatrix.r[0].z = (right + left) / (right - left);
	manny_mtx_projMatrix.r[0].z = 0.f;
//	manny_mtx_projMatrix.r[1].y = (2.0f * nclip) / (top - bottom);
	manny_mtx_projMatrix.r[1].y = (2.0f * nclip) / (-(top * 2));
//	manny_mtx_projMatrix.r[1].z = (top + bottom) / (top - bottom);
//	manny_mtx_projMatrix.r[1].z = (bottom + top) / (bottom - top);
	manny_mtx_projMatrix.r[1].z = 0.f;
//	manny_mtx_projMatrix.r[2].z = -(fclip + nclip) / (fclip - nclip);
	manny_mtx_projMatrix.r[2].z = -(nclip) / (fclip - nclip);
//	manny_mtx_projMatrix.r[2].w = -(2.0f * fclip * nclip) / (fclip - nclip);
	manny_mtx_projMatrix.r[2].w = -(fclip * nclip) / (fclip - nclip);
	manny_mtx_projMatrix.r[3].z = -1.0f;
	manny_mtx_projMatrix.r[3].w = 0.0f;
	// 1.299041        0.000000       0.000000        0.000000
	// 0.000000       -1.732055       0.000000        0.000000
	// 0.000000        0.000000      -0.000003       -0.010000
	// 0.000000        0.000000      -1.000000        0.000000

// extra matrix setup
	float fade = 1.f;
	C3D_FVec pos = FVec3_New(0.007626f, -0.013232f, 0.152443f);
	C3D_FVec nodeDotPos = FVec3_New(0.f, -0.006436f, 0.152753f);
	C3D_FVec nodeDotAnimPos = nodeDotPos;
	C3D_FQuat nodeDotRot = FVec4_New(0.0f, 0.0f, 0.0f, 1.0f);
	C3D_FQuat nodeDotAnimRot = nodeDotRot;

	C3D_FVec nodePivot = FVec3_New(0.000303f, 0.005207f, 0.031834f);

	float yaw = -4.538835f;
	float pitch = 0.4467493f;
	float roll = -0.63673663f;

	C3D_FVec tmpvec = FVec3_Subtract(pos, nodeDotPos);
	tmpvec = FVec3_Scale(tmpvec, fade);
	nodeDotAnimPos = FVec3_Add(nodeDotAnimPos, tmpvec);

	C3D_FQuat rotQuat;
	// Z X Y (3-1-2) ORDER
	C3D_Mtx rotZ, rotX, rotY, rotMtx;
	Mtx_Identity(&rotZ);
	Mtx_Identity(&rotX);
	Mtx_Identity(&rotY);
	Mtx_RotateZ(&rotZ, C3D_AngleFromDegrees(yaw), true);
	Mtx_RotateX(&rotX, C3D_AngleFromDegrees(pitch), true);
	Mtx_RotateY(&rotY, C3D_AngleFromDegrees(roll), true);
	Mtx_Multiply(&rotMtx, &rotZ, &rotX);
	Mtx_Multiply(&rotMtx, &rotMtx, &rotY);
	rotQuat = Quat_FromMtx(&rotMtx);

	C3D_FQuat nodeDotRotInv = Quat_Inverse(nodeDotRot);
	C3D_FQuat rotQuatTemp = Quat_Multiply(nodeDotAnimRot, nodeDotRotInv);
	rotQuat = Quat_Multiply(rotQuatTemp, rotQuat);
	//printf("rotQuat\n");
	//printf("%f %f %f %f\n", rotQuat.x, rotQuat.y, rotQuat.z, rotQuat.w);
	// 0.003675       -0.005707       -0.039619        0.999192

	// node._animRot = node._animRot.slerpQuat(rotQuat, fade);
	float scale0, scale1;
	float flip = 1.0f;
	float angle = Quat_Dot(nodeDotAnimRot, rotQuat);

	if (angle < 0.0f) {
		angle = -angle;
		flip = -1.0f;
	}

	if (angle < 1.0f - (float) 1E-6f) {
		float theta = acosf(angle);
		float invSineTheta = 1.0f / sinf(theta);

		scale0 = sinf((1.0f - fade) * theta) * invSineTheta;
		scale1 = (sinf(fade * theta) * invSineTheta) * flip;
	} else {
		scale0 = 1.0f - fade;
		scale1 = fade * flip;
	}

	C3D_FQuat dst0 = Quat_Scale(nodeDotAnimRot, scale0);
	C3D_FQuat dst1 = Quat_Scale(rotQuat, scale1);
	nodeDotAnimRot = Quat_Add(dst0, dst1);
	//printf("nodeDotAnimRot\n");
	//printf("%f %f %f %f\n", nodeDotAnimRot.x, nodeDotAnimRot.y, nodeDotAnimRot.z, nodeDotAnimRot.w);

// extra matrix proper
	C3D_Mtx matrixStackTop, nodeDotAnimRotMtx, pivotMtx;
	Mtx_Identity(&matrixStackTop);
	Mtx_Translate(&matrixStackTop, nodeDotAnimPos.x, nodeDotAnimPos.y, nodeDotAnimPos.z, true);

	Mtx_FromQuat(&nodeDotAnimRotMtx, nodeDotAnimRot);
	Mtx_Multiply(&matrixStackTop, &matrixStackTop, &nodeDotAnimRotMtx);

	Mtx_Identity(&pivotMtx);
	Mtx_Translate(&pivotMtx, nodePivot.x, nodePivot.y, nodePivot.z, true);
	Mtx_Multiply(&manny_mtx_extraMatrix, &matrixStackTop, &pivotMtx);
}

static void initManny(void) {
	// load shader, create program
	manny_dvlb = DVLB_ParseFile((u32 *)manny_shbin, manny_shbin_size);
	shaderProgramInit(&manny_program);
	shaderProgramSetVsh(&manny_program, &manny_dvlb->DVLE[0]);

	// get uniform locations
	manny_uLoc_extraMatrix = shaderInstanceGetUniformLocation(manny_program.vertexShader, "extraMatrix");
	manny_uLoc_modelMatrix = shaderInstanceGetUniformLocation(manny_program.vertexShader, "modelMatrix");
	manny_uLoc_viewMatrix = shaderInstanceGetUniformLocation(manny_program.vertexShader, "viewMatrix");
	manny_uLoc_projMatrix = shaderInstanceGetUniformLocation(manny_program.vertexShader, "projMatrix");
	manny_uLoc_texScale = shaderInstanceGetUniformLocation(manny_program.vertexShader, "texScale");

	// set matrices
	constructMannyMatrices();

	// set attrinfo
	AttrInfo_Init(&manny_attrInfo);
	AttrInfo_AddLoader(&manny_attrInfo, 0 /*position*/, GPU_FLOAT, 3);
	AttrInfo_AddLoader(&manny_attrInfo, 1 /*texcoord*/, GPU_FLOAT, 2);
	AttrInfo_AddLoader(&manny_attrInfo, 2 /* normal */, GPU_FLOAT, 3);

	// make VBO
	u8 manny_vertices8bit[] = { M_SKIRT1_MESH };
	//float *manny_vertices = (float *)manny_vertices8bit;
	manny_VBO = linearAlloc(sizeof(manny_vertices8bit));
	memcpy(manny_VBO, manny_vertices8bit, sizeof(manny_vertices8bit));

	// set bufinfo
	BufInfo_Init(&manny_bufInfo);
	BufInfo_Add(&manny_bufInfo, manny_VBO, sizeof(float) * 8, 3, 0x210);

	// configure texenv
	C3D_TexEnvInit(&manny_texEnv);
	C3D_TexEnvSrc(&manny_texEnv, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
	C3D_TexEnvOpRgb(&manny_texEnv, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR);
	C3D_TexEnvOpAlpha(&manny_texEnv, GPU_TEVOP_A_SRC_ALPHA, GPU_TEVOP_A_SRC_ALPHA, GPU_TEVOP_A_SRC_ALPHA);
	C3D_TexEnvFunc(&manny_texEnv, C3D_Both, GPU_MODULATE);

	// create manny texture
	initC3DTexes();
}

static void exitManny(void) {
	deleteC3DTexes();
	linearFree(manny_VBO);
	shaderProgramFree(&manny_program);
	DVLB_Free(manny_dvlb);
}




static C3D_RenderTarget					*outer_renderTarget;
static C3D_RenderTarget					*inner_renderTarget;

int main() {
	// Initialize graphics
	gfxInitDefault();
	consoleInit(GFX_TOP, NULL);
	osSetSpeedupEnable(true);
	C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);


	// Initialize the outer render target
	outer_renderTarget = C3D_RenderTargetCreate(240, 320, GPU_RB_RGB8, -1);
	C3D_RenderTargetSetOutput(outer_renderTarget, GFX_BOTTOM, GFX_LEFT, DISPLAY_TRANSFER_FLAGS);

	// Initialize the outer scene
	initOuter();

	// Initialize the inner render target
	inner_renderTarget = C3D_RenderTargetCreateFromTex(&outer_texture, GPU_TEXFACE_2D, 0, GPU_RB_DEPTH24_STENCIL8);

	// Initialize the inner scene
	initInner();

	initManny();
	printf("manny_mtx_extraMatrix\n");
	for (int i = 0; i < 4; i++) {
		printf("%f %f %f %f\n", manny_mtx_extraMatrix.r[i].x, manny_mtx_extraMatrix.r[i].y, manny_mtx_extraMatrix.r[i].z, manny_mtx_extraMatrix.r[i].w);
	}
	printf("manny_mtx_modelMatrix\n");
	for (int i = 0; i < 4; i++) {
		printf("%f %f %f %f\n", manny_mtx_modelMatrix.r[i].x, manny_mtx_modelMatrix.r[i].y, manny_mtx_modelMatrix.r[i].z, manny_mtx_modelMatrix.r[i].w);
	}
	printf("manny_mtx_viewMatrix\n");
	for (int i = 0; i < 4; i++) {
		printf("%f %f %f %f\n", manny_mtx_viewMatrix.r[i].x, manny_mtx_viewMatrix.r[i].y, manny_mtx_viewMatrix.r[i].z, manny_mtx_viewMatrix.r[i].w);
	}
	printf("manny_mtx_projMatrix\n");
	for (int i = 0; i < 4; i++) {
		printf("%f %f %f %f\n", manny_mtx_projMatrix.r[i].x, manny_mtx_projMatrix.r[i].y, manny_mtx_projMatrix.r[i].z, manny_mtx_projMatrix.r[i].w);
	}

	// apply needed effects that are different from citro3d's starting effects
	C3D_CullFace(GPU_CULL_NONE);


	// Main loop
	while (aptMainLoop())
	{
		hidScanInput();

		// Respond to user input
		u32 kDown = hidKeysDown();
		u32 kHeld = hidKeysHeld();
		if (kDown & KEY_START)
			break; // break in order to return to hbmenu

		if ((kDown & KEY_DUP) | (kHeld & KEY_DUP)) {
			if (inner_vxcolor_flt_zPosition < 1.f) {
				inner_vxcolor_flt_zPosition = inner_vxcolor_flt_zPosition + inner_vxcolor_flt_zInterval;
				printf("%f\n", inner_vxcolor_flt_zPosition);
			}
		}

		if ((kDown & KEY_DDOWN) | (kHeld & KEY_DDOWN)) {
			if (inner_vxcolor_flt_zPosition > 0.f) {
				inner_vxcolor_flt_zPosition = inner_vxcolor_flt_zPosition - inner_vxcolor_flt_zInterval;
				printf("%f\n", inner_vxcolor_flt_zPosition);
			}
		}


		C3D_RenderTargetClear(inner_renderTarget, C3D_CLEAR_ALL, 0x00000000, 0x00FFFFFF);

		C3D_DepthMap(true, 1.0f, 1.0f);

		// swizzle background depths into inner_depthSwizzled, DMA inner_depthSwizzled into depth buffer
		swizzle(background_depthAdjusted, (u32 *)inner_depthSwizzled, BACKGROUND_DEPTH_WIDTH, BACKGROUND_DEPTH_HEIGHT,
			0, 0, BACKGROUND_DEPTH_WIDTH, BACKGROUND_DEPTH_HEIGHT,
			BACKGROUND_DEPTH_OFFX, BACKGROUND_DEPTH_OFFY, 1024, 512,
			GPU_RGBA8, false, true);
		GX_RequestDma((u32 *)inner_depthSwizzled, (u32 *)inner_renderTarget->frameBuf.depthBuf, 1024 * 512 * sizeof(u32));

		// draw background
		C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
			C3D_FrameDrawOn(inner_renderTarget);
			C3D_SetViewport(0, 0, 640, 480);
			C3D_SetTexEnv(0, &inner_texture_texEnv);
			// blend disabled
			C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO);
			// depth test disabled, depth func = LESS, depth mask disabled
			C3D_DepthTest(false, GPU_LESS, GPU_WRITE_COLOR);

			C3D_BindProgram(&inner_texture_program);
			C3D_SetAttrInfo(&inner_texture_attrInfo);
			C3D_SetBufInfo(&inner_texture_bufInfo);
			C3D_FVUnifSet(GPU_VERTEX_SHADER, inner_texture_uLoc_texcrop, 0.625f, 0.9375f, 0.f, 0.f);
			C3D_FVUnifSet(GPU_VERTEX_SHADER, inner_texture_uLoc_scale, 1.f, 1.f, 0.f, 0.f);
			C3D_FVUnifSet(GPU_VERTEX_SHADER, inner_texture_uLoc_offset, 0.f, 0.f, 0.f, 0.f);
			C3D_TexBind(0, &inner_C3DTex);
			C3D_DrawElements(GPU_TRIANGLES, 6, C3D_UNSIGNED_SHORT, inner_EBO);
			// blend disabled
			C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO);
			// depth test enabled, depth func = LESS, depth mask enabled
			C3D_DepthTest(true, GPU_LESS, GPU_WRITE_ALL);
		C3D_FrameEnd(0);

		// draw tube can
		C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
			C3D_FrameDrawOn(inner_renderTarget);
			C3D_SetViewport(0, 0, 640, 480);
			C3D_SetTexEnv(0, &inner_texture_texEnv);
			// blend enabled, sfactor = GPU_SRC_ALPHA, dfactor = GPU_ONE_MINUS_SRC_ALPHA
			C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
			// depth test disabled, depth func = LESS, depth mask disabled
			C3D_DepthTest(false, GPU_LESS, GPU_WRITE_COLOR);

			C3D_BindProgram(&inner_texture_program);
			C3D_SetAttrInfo(&inner_texture_attrInfo);
			C3D_SetBufInfo(&inner_texture_bufInfo);
			C3D_FVUnifSet(GPU_VERTEX_SHADER, inner_texture_uLoc_texcrop, TUBE_CAN_COLOR_WIDTH / 512.f, TUBE_CAN_COLOR_HEIGHT / 256.f, 0.f, 0.f);
			C3D_FVUnifSet(GPU_VERTEX_SHADER, inner_texture_uLoc_scale, TUBE_CAN_COLOR_WIDTH / 640.f, TUBE_CAN_COLOR_HEIGHT / 480.f, 0.f, 0.f);
			C3D_FVUnifSet(GPU_VERTEX_SHADER, inner_texture_uLoc_offset, TUBE_CAN_COLOR_OFFX / 640.f, TUBE_CAN_COLOR_OFFY / 480.f, 0.f, 0.f);
			C3D_TexBind(0, &tube_can_C3DTex);
			C3D_DrawElements(GPU_TRIANGLES, 6, C3D_UNSIGNED_SHORT, inner_EBO);
			// blend disabled
			C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO);
			// depth test enabled, depth func = LESS, depth mask enabled
			C3D_DepthTest(true, GPU_LESS, GPU_WRITE_ALL);
		C3D_FrameEnd(0);

		// swizzle tube can depths into inner_depthSwizzled, DMA inner_depthSwizzled into depth buffer
		swizzle(tube_can_depthAdjusted, (u32 *)inner_depthSwizzled, TUBE_CAN_DEPTH_WIDTH, TUBE_CAN_DEPTH_HEIGHT,
			0, 0, TUBE_CAN_DEPTH_WIDTH, TUBE_CAN_DEPTH_HEIGHT,
			TUBE_CAN_DEPTH_OFFX, TUBE_CAN_DEPTH_OFFY, 1024, 512,
			GPU_RGBA8, false, true);
		GX_RequestDma((u32 *)inner_depthSwizzled, (u32 *)inner_renderTarget->frameBuf.depthBuf, 1024 * 512 * sizeof(u32));

		// draw manny skirt
		C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
			C3D_FrameDrawOn(inner_renderTarget);
			C3D_SetViewport(0, 0, 640, 480);
			C3D_SetTexEnv(0, &manny_texEnv);
			// depth test enabled
			C3D_DepthTest(true, GPU_LESS, GPU_WRITE_ALL);
			// alpha test enabled, alpha func = GPU_GREATER, ref value is 0.5f -> 128
			C3D_AlphaTest(true, GPU_GREATER, 128);


			C3D_BindProgram(&manny_program);
			C3D_SetAttrInfo(&manny_attrInfo);
			C3D_SetBufInfo(&manny_bufInfo);
			C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, manny_uLoc_extraMatrix, &manny_mtx_extraMatrix);
			C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, manny_uLoc_modelMatrix, &manny_mtx_modelMatrix);
			C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, manny_uLoc_viewMatrix, &manny_mtx_viewMatrix);
			C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, manny_uLoc_projMatrix, &manny_mtx_projMatrix);
			C3D_FVUnifSet(GPU_VERTEX_SHADER, manny_uLoc_texScale, 32.0f, 32.0f, 0.0f, 0.0f);
			C3D_TexBind(0, &m_s_tiletex);
			//TODO: IN SCUMMVM, MAKE SURE BACKGROUND DEPTHS ARE DRAWN BEFORE ACTORS!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
			C3D_DrawArrays(GPU_TRIANGLES, 0, 102);
			// alpha test disabled, alpha func = GPU_GREATER, ref value is 0.5f -> 128
			C3D_AlphaTest(false, GPU_GREATER, 128);
		C3D_FrameEnd(0);

		// draw colored plane
		C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
			C3D_FrameDrawOn(inner_renderTarget);
			C3D_SetViewport(0, 0, 640, 480);
			C3D_SetTexEnv(0, &inner_vxcolor_texEnv);
			// blend enabled, sfactor = GPU_SRC_ALPHA, dfactor = GPU_ONE_MINUS_SRC_ALPHA
			C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
			// depth test disabled, depth func = LESS, depth mask disabled
			C3D_DepthTest(true, GPU_LESS, GPU_WRITE_ALL);

			C3D_BindProgram(&inner_vxcolor_program);
			C3D_SetAttrInfo(&inner_vxcolor_attrInfo);
			C3D_SetBufInfo(&inner_vxcolor_bufInfo);
			C3D_FVUnifSet(GPU_VERTEX_SHADER, inner_vxcolor_uLoc_zPosition, inner_vxcolor_flt_zPosition, 0.f, 0.f, 0.f);
			C3D_FixedAttribSet(1, 0.0, 1.0, 0.0, 1.0);
			C3D_DrawElements(GPU_TRIANGLES, 6, C3D_UNSIGNED_SHORT, inner_EBO);
		C3D_FrameEnd(0);


		// draw to screen
		C3D_DepthMap(true, -1.0f, 0.0f);
		C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
			C3D_FrameDrawOn(outer_renderTarget);
			C3D_RenderTargetClear(outer_renderTarget, C3D_CLEAR_ALL, /*RGB, no A in target*/ 0x000000, 0);
			C3D_SetTexEnv(0, &outer_texEnv);
			C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
			C3D_DepthTest(false, GPU_GEQUAL, GPU_WRITE_ALL);

			C3D_BindProgram(&outer_program);
			C3D_SetAttrInfo(&outer_attrInfo);
			C3D_SetBufInfo(&outer_bufInfo);
			C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, outer_uLoc_modelView, &outer_mtx_modelView);
			C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, outer_uLoc_projection, &outer_mtx_projection);
			C3D_TexBind(0, &outer_texture);
			C3D_DrawArrays(GPU_TRIANGLE_STRIP, 0, 4);
		C3D_FrameEnd(0);

	}

	exitManny();

	// Deinitialize the inner scene
	exitInner();
	C3D_RenderTargetDelete(inner_renderTarget);

	// Deinitialize the outer scene
	exitOuter();
	C3D_RenderTargetDelete(outer_renderTarget);

	// Deinitialize graphics
	C3D_Fini();
	gfxExit();
	return 0;
}
