#ifndef DESMUME_ENCODINGS_UTF_H
#define DESMUME_ENCODINGS_UTF_H

#include <stddef.h>

#include "../types.h"

static INLINE size_t utf8len(const char* src)
{
	if (src == nullptr) return 0;

	size_t count = 0;
	const unsigned char* p = (const unsigned char*)src;
	while (*p != '\0')
	{
		if ((*p & 0x80) == 0x00) p += 1;
		else if ((*p & 0xE0) == 0xC0 && p[1] != '\0') p += 2;
		else if ((*p & 0xF0) == 0xE0 && p[1] != '\0' && p[2] != '\0') p += 3;
		else if ((*p & 0xF8) == 0xF0 && p[1] != '\0' && p[2] != '\0' && p[3] != '\0') p += 4;
		else p += 1;
		count++;
	}

	return count;
}

static INLINE void utf8_conv_utf32(u32* dst, size_t dst_size, const char* src, size_t)
{
	if (dst == nullptr || dst_size == 0) return;

	size_t di = 0;
	const unsigned char* p = (const unsigned char*)src;
	while (p != nullptr && *p != '\0' && di + 1 < dst_size)
	{
		u32 codepoint = 0xFFFD;
		if ((*p & 0x80) == 0x00)
		{
			codepoint = *p++;
		}
		else if ((*p & 0xE0) == 0xC0 && p[1] != '\0')
		{
			codepoint = ((u32)(p[0] & 0x1F) << 6) | (u32)(p[1] & 0x3F);
			p += 2;
		}
		else if ((*p & 0xF0) == 0xE0 && p[1] != '\0' && p[2] != '\0')
		{
			codepoint = ((u32)(p[0] & 0x0F) << 12) | ((u32)(p[1] & 0x3F) << 6) | (u32)(p[2] & 0x3F);
			p += 3;
		}
		else if ((*p & 0xF8) == 0xF0 && p[1] != '\0' && p[2] != '\0' && p[3] != '\0')
		{
			codepoint = ((u32)(p[0] & 0x07) << 18) | ((u32)(p[1] & 0x3F) << 12) | ((u32)(p[2] & 0x3F) << 6) | (u32)(p[3] & 0x3F);
			p += 4;
		}
		else
		{
			p += 1;
		}

		dst[di++] = codepoint;
	}

	dst[di] = 0;
}

static INLINE void utf16_to_char_string(const u16* src, char* dst, size_t dst_size)
{
	if (dst == nullptr || dst_size == 0)
		return;

	if (src == nullptr)
	{
		dst[0] = '\0';
		return;
	}

	size_t di = 0;
	for (size_t si = 0; src[si] != 0 && di + 1 < dst_size; ++si)
	{
		u32 ch = src[si];
		if (ch <= 0x7F)
		{
			if (di + 1 >= dst_size) break;
			dst[di++] = (char)ch;
		}
		else if (ch <= 0x7FF)
		{
			if (di + 2 >= dst_size) break;
			dst[di++] = (char)(0xC0 | (ch >> 6));
			dst[di++] = (char)(0x80 | (ch & 0x3F));
		}
		else
		{
			if (di + 3 >= dst_size) break;
			dst[di++] = (char)(0xE0 | (ch >> 12));
			dst[di++] = (char)(0x80 | ((ch >> 6) & 0x3F));
			dst[di++] = (char)(0x80 | (ch & 0x3F));
		}
	}

	dst[di] = '\0';
}

#endif
