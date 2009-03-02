extern int finite(double);

/* undef'ing this makes things look a bit nicer, but slower  */
#define DBL_PIXELS

void
NAME(fzort_ctx *ctx, struct pvertex *v1,
  struct pvertex *v2, struct pvertex *v3)
{
	long x_left, x_right, dxdy_left, dxdy_right;
	struct pvertex *v_min, *v_mid, *v_max;
	struct edge e_maj, e_top, e_bot, *e_left, *e_right;
	float area, inv_area, bf = 1.f;
	long dudy, dudy_left, u_left;
	long dvdy, dvdy_left, v_left;
	int sub_tri, left_to_right, y, lines;
	int xa, xb, adjx;
	PIXEL_TYPE *scanline_ptr;
	PIXEL_TYPE *texture_ptr;
	register int l;
	register long u, v, dudx, dvdx;
#if (defined DBL_PIXELS) && !(defined MONOCHROME)
	register PIXEL_TYPE t;
	register long dudx2, dvdx2, dudx4, dvdx4;
#endif
	register PIXEL_TYPE *pixel_ptr;
#ifdef MONOCHROME
	register unsigned int mask, bits;
	register unsigned int *dither_ptr;
#endif

	texture_ptr = (PIXEL_TYPE *)ctx->texture;

	if (v1->y_scr <= v2->y_scr) {
		if (v2->y_scr <= v3->y_scr) {
			v_min = v1; v_mid = v2; v_max = v3;
		} else if (v3->y_scr <= v1->y_scr) {
			v_min = v3; v_mid = v1; v_max = v2;
		} else {
			v_min = v1; v_mid = v3; v_max = v2;
			bf = -bf;
		}
	} else {
		if (v1->y_scr <= v3->y_scr) {
			v_min = v2; v_mid = v1; v_max = v3;
			bf = -bf;
		} else if (v3->y_scr <= v2->y_scr) {
			v_min = v3; v_mid = v2; v_max = v1;
			bf = -bf;
		} else {
			v_min = v2; v_mid = v3; v_max = v1;
		}
	}

	area = (v_max->x_scr - v_min->x_scr)*(v_mid->y_scr - v_min->y_scr) -
	  (v_mid->x_scr - v_min->x_scr)*(v_max->y_scr - v_min->y_scr);

	if (!finite(area) || area == 0.)
		return;

	if (area*bf < 0.)
		return;

	init_edge(ctx, &e_maj, v_min, v_max);

	if (e_maj.lines == 0)
		return;

	init_edge(ctx, &e_top, v_mid, v_max);
	init_edge(ctx, &e_bot, v_min, v_mid);

	left_to_right = area < 0.; /* inter edge is to the left */

	inv_area = 1./area;

	dudx = inv_area*(e_maj.du*e_bot.dy - e_maj.dy*e_bot.du);
	dudy = inv_area*(e_maj.dx*e_bot.du - e_maj.du*e_bot.dx);

	dvdx = inv_area*(e_maj.dv*e_bot.dy - e_maj.dy*e_bot.dv);
	dvdy = inv_area*(e_maj.dx*e_bot.dv - e_maj.dv*e_bot.dx);

#if (defined DBL_PIXELS) && !(defined MONOCHROME)
	dudx2 = dudx*2;
	dvdx2 = dvdx*2;

	dudx4 = dudx2*2;
	dvdx4 = dvdx2*2;
#endif

	x_left = x_right = dxdy_left = dxdy_right = 0; /* shut up compiler */
	u_left = dudy_left = v_left = dvdy_left = 0;

	for (sub_tri = 0; sub_tri <= 1; sub_tri++) {
		if (sub_tri == 0) {
			if (left_to_right) {
				e_left = &e_maj;
				e_right = &e_bot;
				lines = e_right->lines;
			} else {
				e_left = &e_bot;
				e_right = &e_maj;
				lines = e_left->lines;
			}

			x_left = e_left->sx;
			dxdy_left = e_left->dxdy;

			x_right = e_right->sx;
			dxdy_right = e_right->dxdy;

			dudy_left = fp_mul(dxdy_left, dudx) + dudy;
			u_left = v_min->u_txt;

			dvdy_left = fp_mul(dxdy_left, dvdx) + dvdy;
			v_left = v_min->v_txt;

			if (e_left->adjy) {
				u_left += e_left->adjy*dudy_left;
				v_left += e_left->adjy*dvdy_left;
			}

			y = e_left->sy;
		} else {
			if (left_to_right) {
				e_right = &e_top;
				lines = e_right->lines;
				y = e_right->sy;

				x_right = e_right->sx;
				dxdy_right = e_right->dxdy;
			} else {
				e_left = &e_top;
				lines = e_left->lines;
				y = e_left->sy;

				x_left = e_left->sx;
				dxdy_left = e_left->dxdy;

				dudy_left = fp_mul(dxdy_left, dudx) + dudy;
				u_left = v_mid->u_txt;

				dvdy_left = fp_mul(dxdy_left, dvdx) + dvdy;
				v_left = v_mid->v_txt;

				if (e_left->adjy) {
					u_left += e_left->adjy*dudy_left;
					v_left += e_left->adjy*dvdy_left;
				}
			}
		}

		if (lines > 0) {
			scanline_ptr = (PIXEL_TYPE *)((unsigned char *)ctx->raster.bits +
			  y*ctx->raster.pitch);

			while (lines--) {
				xa = FP_ICEIL(x_left);
				xb = FP_IFLOOR(x_right);

				if (xa < ctx->clip.x_min) {
					adjx = ctx->clip.x_min - xa;
					xa = ctx->clip.x_min;
				} else {
					adjx = 0;
				}

				if (xb > ctx->clip.x_max) {
					xb = ctx->clip.x_max;
				}

				if (xb >= ctx->clip.x_min && xa <= xb) {
					u = u_left; 
					v = v_left;

					if (adjx) {
						u += dudx*adjx;
						v += dvdx*adjx;
					}

#ifdef MONOCHROME
					pixel_ptr = scanline_ptr + (xa>>3);
					dither_ptr = &dither6[y&7][xa&7];
#ifdef LSB_TO_MSB
					mask = 1 << (xa&7);
#else
					mask = 1 << (7 - (xa&7));
#endif
#else
					pixel_ptr = scanline_ptr + xa;
#endif

					l = 1 + xb - xa;

#define TEXTURE_PIXEL texture_ptr[(FP_TO_INT(v)&(TEXTURE_SIZE - 1)) + \
  (FP_TO_INT(u)&(TEXTURE_SIZE - 1))*TEXTURE_SIZE]

#ifdef MONOCHROME
					bits = *pixel_ptr;

					while (l--) {
						if ((TEXTURE_PIXEL>>2) > *dither_ptr++)
							bits |= mask;
						else
							bits &= ~mask;
#ifdef LSB_TO_MSB
						if ((mask <<= 1) == (1<<8)) {
#else
						if (!(mask >>= 1)) {
#endif
							*pixel_ptr++ = bits;
							bits = *pixel_ptr;
							dither_ptr -= 8;
#ifdef LSB_TO_MSB
							mask = 1;
#else
							mask = 1<<7;
#endif
						}
						u += dudx; v += dvdx;
					}

					*pixel_ptr = bits;
#else
					if (l & 1) {
						*pixel_ptr++ = TEXTURE_PIXEL;
						u += dudx; v += dvdx;
					}

					l >>= 1;

					if (l & 1) {
#ifdef DBL_PIXELS
						t = TEXTURE_PIXEL;
						pixel_ptr[0] = t; pixel_ptr[1] = t;
						pixel_ptr += 2;
						u += dudx2; v += dvdx2;
#else
						*pixel_ptr++ = TEXTURE_PIXEL;
						u += dudx; v += dvdx;

						*pixel_ptr++ = TEXTURE_PIXEL;
						u += dudx; v += dvdx;
#endif
					}

					l >>= 1;

					while (l--) {
#ifdef DBL_PIXELS
						t = TEXTURE_PIXEL;
						pixel_ptr[0] = t; pixel_ptr[1] = t;
						pixel_ptr[2] = t; pixel_ptr[3] = t;
						pixel_ptr += 4;
						u += dudx4; v += dvdx4;
#else
						*pixel_ptr++ = TEXTURE_PIXEL;
						u += dudx; v += dvdx;

						*pixel_ptr++ = TEXTURE_PIXEL;
						u += dudx; v += dvdx;

						*pixel_ptr++ = TEXTURE_PIXEL;
						u += dudx; v += dvdx;

						*pixel_ptr++ = TEXTURE_PIXEL;
						u += dudx; v += dvdx;
#endif
					}

#endif /* MONOCHROME */

#undef TEXTURE_PIXEL
				}

				scanline_ptr += ctx->raster.pitch/sizeof(*scanline_ptr);
				x_left += dxdy_left;
				x_right += dxdy_right;
				u_left += dudy_left;
				v_left += dvdy_left;
				y++;
			}
		}
	}
}

#undef LSB_TO_MSB
#undef MONOCHROME
#undef FILL_SCANLINE
#undef PIXEL_TYPE
#undef NAME
