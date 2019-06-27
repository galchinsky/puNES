/*
 *  Copyright (C) 2010-2019 Fabio Cavallo (aka FHorse)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include "video/filters/ntsc.h"
#include "ppu.h"
#include "overscan.h"

nes_ntsc_t *ntsc;
nes_ntsc_setup_t format[3];
int merge_fields = 1;
int burst_phase = 0;

/*
 * cio' che non utilizzo in questa funzione
 * sono i parametri WORD **screen_index e
 * Uint32 *palette.
 */
void ntsc_surface(void) {
	int y;

	if (gfx.filter.data.palette == NULL) {
		gfx.filter.data.palette = (void *)ntsc;
	}

	nes_ntsc_blit((nes_ntsc_t *)gfx.filter.data.palette, screen.rd_left->data, SCR_ROWS, burst_phase, SCR_ROWS, SCR_LINES,
		gfx.filter.data.pix_left, gfx.filter.data.pitch);
	nes_ntsc_blit((nes_ntsc_t *)gfx.filter.data.palette, screen.rd_right->data, SCR_ROWS, burst_phase, SCR_ROWS, SCR_LINES,
		gfx.filter.data.pix_right, gfx.filter.data.pitch);


	for (y = ((gfx.filter.data.height / gfx.filter.factor) - 1); --y >= 0;) {
		unsigned char const *in_left = ((const unsigned char *)gfx.filter.data.pix_left) + (y * gfx.filter.data.pitch);
		unsigned char const *in_right = ((const unsigned char *)gfx.filter.data.pix_right) + (y * gfx.filter.data.pitch);
		unsigned char *out_left = ((unsigned char *)gfx.filter.data.pix_left) + ((y * gfx.filter.factor) * gfx.filter.data.pitch);
		unsigned char *out_right = ((unsigned char *)gfx.filter.data.pix_right) + ((y * gfx.filter.factor) * gfx.filter.data.pitch);

		int n;

		for (n = gfx.filter.data.width; n; --n) {
			unsigned prev_left = *(uint32_t *)in_left;
			unsigned prev_right = *(uint32_t *)in_right;
			unsigned next_left = *(uint32_t *)(in_left + gfx.filter.data.pitch);
			unsigned next_right = *(uint32_t *)(in_right + gfx.filter.data.pitch);
			/* mix rgb without losing low bits */
			unsigned mixed_left = prev_left + next_left + ((prev_left ^ next_left) & 0x00010101);
			unsigned mixed_right = prev_right + next_right + ((prev_right ^ next_right) & 0x00010101);

			/* darken by 12% */
			*(uint32_t *)out_left = prev_left | 0xFF000000;
			*(uint32_t *)out_right = prev_right | 0xFF000000;
			*(uint32_t *)(out_left + gfx.filter.data.pitch) = ((mixed_left >> 1) - (mixed_left >> 4 & 0x00030703)) | 0xFF000000;
			*(uint32_t *)(out_right + gfx.filter.data.pitch) = ((mixed_right >> 1) - (mixed_right >> 4 & 0x00030703)) | 0xFF000000;

			in_left += NES_NTSC_OUT_DEPTH / 8;
			out_left += NES_NTSC_OUT_DEPTH / 8;
			in_right += NES_NTSC_OUT_DEPTH / 8;
			out_right += NES_NTSC_OUT_DEPTH / 8;
		}
	}
}

BYTE ntsc_init(BYTE effect, BYTE color, BYTE *palette_base, BYTE *palette_in, BYTE *palette_out) {
	format[COMPOSITE] = nes_ntsc_composite;
	format[SVIDEO] = nes_ntsc_svideo;
	format[RGBMODE] = nes_ntsc_rgb;

	if (!(ntsc = (nes_ntsc_t *)malloc(sizeof(nes_ntsc_t)))) {
		fprintf(stderr, "Out of memory\n");
		return (EXIT_ERROR);
	}
	ntsc_set(NULL, effect, color, palette_base, palette_in, palette_out);
	return (EXIT_OK);
}
void ntsc_quit(void) {
	free(ntsc);
}
void ntsc_set(nes_ntsc_t *ntsc_in, BYTE effect, BYTE color, BYTE *palette_base, BYTE *palette_in, BYTE *palette_out) {
	if (!ntsc_in) {
		ntsc_in = ntsc;
	}

	if (palette_base) {
		format[effect].base_palette = (unsigned char *)palette_base;
	} else {
		format[effect].base_palette = 0;
	}

	if (palette_in) {
		format[effect].palette = (unsigned char *)palette_in;
	} else {
		format[effect].palette = 0;
	}

	if (palette_out) {
		format[effect].palette_out = (unsigned char *)palette_out;
	} else {
		format[effect].palette_out = 0;
	}

	//burst_phase ^= 1;
	/*if ( setup.merge_fields ) {
	 burst_phase = 0;
	 }*/
	//format[effect].merge_fields = merge_fields;
	format[effect].decoder_matrix = 0;
	format[effect].saturation = 0;
	format[effect].swapped = 0;

	if (color) {
		switch (color) {
			/* Sony CXA2025AS US */
			case PALETTE_SONY: {
				static float matrix[6] = { 1.630, 0.317, -0.378, -0.466, -1.089, 1.677 };

				format[effect].decoder_matrix = matrix;
				break;
			}
			case PALETTE_MONO:
				format[effect].saturation = -1;
				break;
		}
	}

	nes_ntsc_init(ntsc_in, &format[effect]);

	// creo la paletta swappata
	if (format[effect].palette_out) {
		format[effect].swapped = 1;
		format[effect].palette_out = (unsigned char *)palette_RGB.swapped;
		nes_ntsc_init(ntsc_in, &format[effect]);
	}
}
