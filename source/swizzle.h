#pragma once

static u32 getPixelSizeInBytes(GPU_TEXCOLOR format, u32 *isHalfByte) {
	switch(format) {
		case GPU_RGBA8:
			return 4;
		case GPU_RGB8:
			return 3;
		case GPU_LA8:
		case GPU_HILO8:
		case GPU_RGB565:
		case GPU_RGBA5551:
		case GPU_RGBA4:
			return 2;
		case GPU_A8:
		case GPU_L8:
		case GPU_LA4:
			return 1;
		case GPU_L4:
		case GPU_A4:
			if (!isHalfByte) *isHalfByte = 1;
			return 1;
		default:
			return 0;
	}
}

static u32 mortonInterleave(u32 x, u32 y) {
	static u32 xlut[] = {0x00, 0x01, 0x04, 0x05, 0x10, 0x11, 0x14, 0x15};
	static u32 ylut[] = {0x00, 0x02, 0x08, 0x0a, 0x20, 0x22, 0x28, 0x2a};

	return xlut[x % 8] + ylut[y % 8];
}

static u32 mortonOffset(u32 x, u32 y) {
	u32 i = mortonInterleave(x, y);
	u32 xCoarse = x & ~7;
	u32 offset = xCoarse << 3;

	return (i + offset);
}

static void swizzle(u32 *srcBuf, u32 *dstBuf, int copyWidth, int copyHeight,
             int xSource, int ySource, int wSource,   int hSource,
             int xDest,   int yDest,   int wDest,     int hDest,
             GPU_TEXCOLOR format, bool isBlockSrc, bool isDepthTex) {
	u32 pixSize = 4, halfByte = 0;
	int yFlip   = !isBlockSrc;

	pixSize = getPixelSizeInBytes(format, &halfByte);

	u32 wSrc = wSource;
	u32 hSrc = hSource;
	u32 wDst = wDest;
	u32 hDst = hDest;
	u32 copyW = copyWidth;
	u32 copyH = copyHeight;
	u32 ySrc = ySource;
	u32 xSrc = xSource;
	u32 yDst = yDest;
	u32 xDst = xDest;
	u32 x, y;
	u8 *srcConverted;

	if (!isBlockSrc && !isDepthTex) {
		int idx;
		int size = wSource * hSource * pixSize;
		if (halfByte) size /= 2;

		srcConverted = (u8 *)linearAlloc((size_t)size);

		// some color formats have reversed component positions under the 3DS's native texture format
		u16 *srcBuf16;
		switch (format) {
			case GPU_RGBA8:
				for (idx = 0; idx < wSource * hSource; idx++) {
					((u32 *)srcConverted)[idx] = (((srcBuf[idx] << 24) & 0xff000000) |
					                              ((srcBuf[idx] << 8)  & 0x00ff0000) |
					                              ((srcBuf[idx] >> 8)  & 0x0000ff00) |
					                              ((srcBuf[idx] >> 24) & 0x000000ff));
				}
				break;
			case GPU_RGB8:
				for (idx = 0; idx < wSource * hSource; idx++) {
					int pxl = idx * 3;
					srcConverted[pxl + 2] = ((u8 *)srcBuf)[pxl + 0];
					srcConverted[pxl + 1] = ((u8 *)srcBuf)[pxl + 1];
					srcConverted[pxl + 0] = ((u8 *)srcBuf)[pxl + 2];
				}
				break;
			case GPU_LA8:
			case GPU_HILO8:
				srcBuf16 = (u16 *)srcBuf;
				for (idx = 0; idx < wSource * hSource; idx++) {
					((u16 *)srcConverted)[idx] = (((srcBuf16[idx] << 8) & 0xff00) |
					                              ((srcBuf16[idx] >> 8) & 0x00ff));
				}
				break;
			case GPU_RGB565:
			case GPU_RGBA5551:
			case GPU_RGBA4:
				for (idx = 0; idx < wSource * hSource; idx++)
					((u16 *)srcConverted)[idx] = ((u16 *)srcBuf)[idx];
				break;
			case GPU_A8:
			case GPU_L8:
			case GPU_LA4:
				for (idx = 0; idx < wSource * hSource; idx++)
					srcConverted[idx] = ((u8 *)srcBuf)[idx];
				break;
			case GPU_L4:
			case GPU_A4:
				for (idx = 0; idx < wSource * hSource / 2; idx++)
					srcConverted[idx] = ((u8 *)srcBuf)[idx];
				break;
			case GPU_ETC1:
			case GPU_ETC1A4:
				linearFree(srcConverted);
				return;
		}
	} else
		srcConverted = (u8 *)srcBuf;

	for (y = 0; y < copyH; y++) {
		u32 yOffSrc = (yFlip) ? ySrc + y : hSrc - 1 - (ySrc + y);
		u32 yOffSrcCoarse = yOffSrc & ~7;

		u32 yOffDst = hDst - 1 - (yDst + y);
		u32 yOffDstCoarse = yOffDst & ~7;

		for (x = 0; x < copyW; x++) {
			u32 xOffDst = x + xDst;
			u32 offset = mortonOffset(xOffDst, yOffDst) + yOffDstCoarse * wDst;

			u32 xOffSrc = (xSrc + x);
			u32 locate = (isBlockSrc) ? (mortonOffset(xOffSrc, yOffSrc) + yOffSrcCoarse * wSrc)
			                          : (xOffSrc + yOffSrc * wSource);

			if (offset >= wDst * hDst) continue;
			if (locate >= (u32)(wSource * hSource)) continue;

			u8 *dstByte, *srcByte;

			if (halfByte) {
				srcByte = &srcConverted[(locate * pixSize) >> 1];
				dstByte = &((u8 *)dstBuf)[(offset * pixSize) >> 1];
				if (x & 1)
					*dstByte = (*srcByte & 0xf0) | (*dstByte & 0x0f);
				else
					*dstByte = (*dstByte & 0xf0) | (*srcByte & 0x0f);
			} else {
				srcByte = &srcConverted[locate * pixSize];
				dstByte = &((u8 *)dstBuf)[offset * pixSize];
				for (int i = 0; i < (int)pixSize; i++)
					*dstByte++ = *srcByte++;
			}
		}
	}

	if (!isBlockSrc && !isDepthTex)
		linearFree((void *)srcConverted);

	return;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
