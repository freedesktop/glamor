/*
 * Copyright © 2009 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Authors:
 *    Eric Anholt <eric@anholt.net>
 *
 */


/** @file glamor_putaimge.c
 *
 * XPutImage implementation
 */
#include "glamor_priv.h"

void
glamor_init_putimage_shaders(ScreenPtr screen)
{
#if 0
	glamor_screen_private *glamor_priv =
	    glamor_get_screen_private(screen);
	const char *xybitmap_vs =
	    "uniform float x_bias;\n" "uniform float x_scale;\n"
	    "uniform float y_bias;\n" "uniform float y_scale;\n"
	    "varying vec2 bitmap_coords;\n" "void main()\n" "{\n"
	    "	gl_Position = vec4((gl_Vertex.x + x_bias) * x_scale,\n"
	    "			   (gl_Vertex.y + y_bias) * y_scale,\n"
	    "			   0,\n"
	    "			   1);\n"
	    "	bitmap_coords = gl_MultiTexCoord0.xy;\n" "}\n";
	const char *xybitmap_fs =
	    "uniform vec4 fg, bg;\n" "varying vec2 bitmap_coords;\n"
	    "uniform sampler2D bitmap_sampler;\n" "void main()\n" "{\n"
	    "	float bitmap_value = texture2D(bitmap_sampler,\n"
	    "				       bitmap_coords).x;\n"
	    "	gl_FragColor = mix(bg, fg, bitmap_value);\n" "}\n";
	GLint fs_prog, vs_prog, prog;
	GLint sampler_uniform_location;

	if (!GLEW_ARB_fragment_shader)
		return;

	prog = dispatch->glCreateProgram();
	vs_prog = glamor_compile_glsl_prog(GL_VERTEX_SHADER, xybitmap_vs);
	fs_prog =
	    glamor_compile_glsl_prog(GL_FRAGMENT_SHADER, xybitmap_fs);
	dispatch->glAttachShader(prog, vs_prog);
	dispatch->glAttachShader(prog, fs_prog);
	glamor_link_glsl_prog(prog);

	dispatch->glUseProgram(prog);
	sampler_uniform_location =
	    dispatch->glGetUniformLocation(prog, "bitmap_sampler");
	dispatch->glUniform1i(sampler_uniform_location, 0);

	glamor_priv->put_image_xybitmap_fg_uniform_location =
	    dispatch->glGetUniformLocation(prog, "fg");
	glamor_priv->put_image_xybitmap_bg_uniform_location =
	    dispatch->glGetUniformLocation(prog, "bg");
	glamor_get_transform_uniform_locations(prog,
					       &glamor_priv->put_image_xybitmap_transform);
	glamor_priv->put_image_xybitmap_prog = prog;
	dispatch->glUseProgram(0);
#endif
}


/* Do an XYBitmap putimage.  The bits are byte-aligned rows of bitmap
 * data (where each row starts at a bit index of left_pad), and the
 * destination gets filled with the gc's fg color where the bitmap is set
 * and the bg color where the bitmap is unset.
 *
 * Implement this by passing the bitmap right through to GL, and sampling
 * it to choose between fg and bg in the fragment shader.  The driver may
 * be exploding the bitmap up to be an 8-bit alpha texture, in which
 * case we might be better off just doing the fg/bg choosing in the CPU
 * and just draw the resulting texture to the destination.
 */
#if 0

static int
y_flip(PixmapPtr pixmap, int y)
{
	ScreenPtr screen = pixmap->drawable.pScreen;
	PixmapPtr screen_pixmap = screen->GetScreenPixmap(screen);

	if (pixmap == screen_pixmap)
		return (pixmap->drawable.height - 1) - y;
	else
		return y;
}


static void
glamor_put_image_xybitmap(DrawablePtr drawable, GCPtr gc,
			  int x, int y, int w, int h, int left_pad,
			  int image_format, char *bits)
{
	ScreenPtr screen = drawable->pScreen;
	PixmapPtr pixmap = glamor_get_drawable_pixmap(drawable);
	glamor_screen_private *glamor_priv =
	    glamor_get_screen_private(screen);
	float fg[4], bg[4];
	GLuint tex;
	unsigned int stride = PixmapBytePad(1, w + left_pad);
	RegionPtr clip;
	BoxPtr box;
	int nbox;
	float dest_coords[8];
	const float bitmap_coords[8] = {
		0.0, 0.0,
		1.0, 0.0,
		1.0, 1.0,
		0.0, 1.0,
	};
	GLfloat xscale, yscale;
	glamor_pixmap_private *pixmap_priv;

	pixmap_priv = glamor_get_pixmap_private(pixmap);

	pixmap_priv_get_scale(pixmap_priv, &xscale, &yscale);

	glamor_set_normalize_vcoords(xscale, yscale,
				     x, y,
				     x + w, y + h,
				     glamor_priv->yInverted, dest_coords);

	glamor_fallback("glamor_put_image_xybitmap: disabled\n");
	goto fail;

	if (glamor_priv->put_image_xybitmap_prog == 0) {
		ErrorF("no program for xybitmap putimage\n");
		goto fail;
	}

	glamor_set_alu(gc->alu);
	if (!glamor_set_planemask(pixmap, gc->planemask))
		goto fail;

	dispatch->glUseProgram(glamor_priv->put_image_xybitmap_prog);

	glamor_get_color_4f_from_pixel(pixmap, gc->fgPixel, fg);
	dispatch->glUniform4fv
	    (glamor_priv->put_image_xybitmap_fg_uniform_location, 1, fg);
	glamor_get_color_4f_from_pixel(pixmap, gc->bgPixel, bg);
	dispatch->glUniform4fv
	    (glamor_priv->put_image_xybitmap_bg_uniform_location, 1, bg);

	dispatch->glGenTextures(1, &tex);
	dispatch->glActiveTexture(GL_TEXTURE0);
	dispatch->glEnable(GL_TEXTURE_2D);
	dispatch->glBindTexture(GL_TEXTURE_2D, tex);
	dispatch->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
				  GL_NEAREST);
	dispatch->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
				  GL_NEAREST);
	dispatch->glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	dispatch->glPixelStorei(GL_UNPACK_ROW_LENGTH, stride * 8);
	dispatch->glPixelStorei(GL_UNPACK_SKIP_PIXELS, left_pad);
	dispatch->glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA,
			       w, h, 0, GL_COLOR_INDEX, GL_BITMAP, bits);
	dispatch->glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	dispatch->glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
	dispatch->glEnable(GL_TEXTURE_2D);

	/* Now that we've set up our bitmap texture and the shader, shove
	 * the destination rectangle through the cliprects and run the
	 * shader on the resulting fragments.
	 */
	dispatch->glVertexPointer(2, GL_FLOAT, 0, dest_coords);
	dispatch->glEnableClientState(GL_VERTEX_ARRAY);
	dispatch->glClientActiveTexture(GL_TEXTURE0);
	dispatch->glTexCoordPointer(2, GL_FLOAT, 0, bitmap_coords);
	dispatch->glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	dispatch->glEnable(GL_SCISSOR_TEST);
	clip = fbGetCompositeClip(gc);
	for (nbox = REGION_NUM_RECTS(clip),
	     box = REGION_RECTS(clip); nbox--; box++) {
		int x1 = x;
		int y1 = y;
		int x2 = x + w;
		int y2 = y + h;

		if (x1 < box->x1)
			x1 = box->x1;
		if (y1 < box->y1)
			y1 = box->y1;
		if (x2 > box->x2)
			x2 = box->x2;
		if (y2 > box->y2)
			y2 = box->y2;
		if (x1 >= x2 || y1 >= y2)
			continue;

		dispatch->glScissor(box->x1,
				    y_flip(pixmap, box->y1),
				    box->x2 - box->x1, box->y2 - box->y1);
		dispatch->glDrawArrays(GL_QUADS, 0, 4);
	}

	dispatch->glDisable(GL_SCISSOR_TEST);
	glamor_set_alu(GXcopy);
	glamor_set_planemask(pixmap, ~0);
	dispatch->glDeleteTextures(1, &tex);
	dispatch->glDisable(GL_TEXTURE_2D);
	dispatch->glDisableClientState(GL_VERTEX_ARRAY);
	dispatch->glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	return;
	glamor_set_alu(GXcopy);
	glamor_set_planemask(pixmap, ~0);
	glamor_fallback(": to %p (%c)\n",
			drawable, glamor_get_drawable_location(drawable));
fail:
	if (glamor_prepare_access(drawable, GLAMOR_ACCESS_RW)) {
		fbPutImage(drawable, gc, 1, x, y, w, h, left_pad, XYBitmap,
			   bits);
		glamor_finish_access(drawable, GLAMOR_ACCESS_RW);
	}
}
#endif

void
glamor_fini_putimage_shaders(ScreenPtr screen)
{
}


static Bool 
_glamor_put_image(DrawablePtr drawable, GCPtr gc, int depth, int x, int y,
		 int w, int h, int left_pad, int image_format, char *bits, Bool fallback)
{
	glamor_screen_private *glamor_priv =
	    glamor_get_screen_private(drawable->pScreen);
	glamor_gl_dispatch *dispatch;
	PixmapPtr pixmap = glamor_get_drawable_pixmap(drawable);
	glamor_pixmap_private *pixmap_priv =
	    glamor_get_pixmap_private(pixmap);
	GLenum type, format, iformat;
	RegionPtr clip;
	BoxPtr pbox;
	int nbox;
	int src_stride = PixmapBytePad(w, drawable->depth);
	int x_off, y_off;
	float vertices[8], texcoords[8];
	GLfloat xscale, yscale, txscale, tyscale;
	GLuint tex;
	int no_alpha, revert;
	Bool ret = FALSE;
	int swap_rb;
	PixmapPtr temp_pixmap;
	glamor_pixmap_private *temp_pixmap_priv;

	if (image_format == XYBitmap) {
		assert(depth == 1);
		goto fail;
	}

	if (!GLAMOR_PIXMAP_PRIV_HAS_FBO(pixmap_priv)) {
		glamor_fallback("has no fbo.\n");
		goto fail;
	}

	if (image_format != ZPixmap) {
		glamor_fallback("non-ZPixmap\n");
		goto fail;
	}

	if (!glamor_set_planemask(pixmap, gc->planemask)) {
		goto fail;
	}

	if (glamor_get_tex_format_type_from_pixmap(pixmap,
						   &format,
						   &type, &no_alpha,
						   &revert,
						   &swap_rb,
						   1)) {
		glamor_fallback("unknown depth. %d \n", drawable->depth);
		goto fail;
	}

	/* create a temporary pixmap and upload the bits to that
	 * pixmap, then apply clip copy it to the destination pixmap.*/

	temp_pixmap = glamor_create_pixmap(drawable->pScreen, w, h, depth, 0);

	if (temp_pixmap == NULL)
		goto fail;
	temp_pixmap_priv = glamor_get_pixmap_private(temp_pixmap);

	if (temp_pixmap_priv->fbo == NULL) {
		glamor_destroy_pixmap(temp_pixmap);
		goto fail;
	}

	if (!glamor_upload_bits_to_pixmap_texture(temp_pixmap, format, type,
						  no_alpha, revert, swap_rb, bits)) {
		glamor_destroy_pixmap(temp_pixmap);
		goto fail;
	}


	dispatch = glamor_get_dispatch(glamor_priv);
	if (!glamor_set_alu(dispatch, gc->alu)) {
		glamor_put_dispatch(glamor_priv);
		goto fail;
	}

	glamor_set_destination_pixmap_priv_nc(pixmap_priv);
	glamor_validate_pixmap(pixmap);
	dispatch->glVertexAttribPointer(GLAMOR_VERTEX_POS, 2, GL_FLOAT,
					GL_FALSE, 2 * sizeof(float),
					vertices);
	dispatch->glEnableVertexAttribArray(GLAMOR_VERTEX_POS);

	dispatch->glVertexAttribPointer(GLAMOR_VERTEX_SOURCE, 2, GL_FLOAT,
					GL_FALSE, 2 * sizeof(float),
					texcoords);
	dispatch->glEnableVertexAttribArray(GLAMOR_VERTEX_SOURCE);
	dispatch->glActiveTexture(GL_TEXTURE0);
	dispatch->glBindTexture(GL_TEXTURE_2D, temp_pixmap_priv->fbo->tex);

#ifndef GLAMOR_GLES2
	dispatch->glEnable(GL_TEXTURE_2D);
#endif
	dispatch->glUseProgram(glamor_priv->finish_access_prog[0]);
	dispatch->glUniform1i(glamor_priv->
			      finish_access_revert[no_alpha],
			      REVERT_NONE);
	dispatch->glUniform1i(glamor_priv->finish_access_swap_rb[no_alpha],
			      SWAP_NONE_UPLOADING);

	x += drawable->x;
	y += drawable->y;

	glamor_get_drawable_deltas(drawable, pixmap, &x_off, &y_off);
	clip = fbGetCompositeClip(gc);

	pixmap_priv_get_scale(temp_pixmap_priv, &txscale, &tyscale);
	pixmap_priv_get_scale(pixmap_priv, &xscale, &yscale);

	for (nbox = REGION_NUM_RECTS(clip),
	     pbox = REGION_RECTS(clip); nbox--; pbox++) {
		int x1 = x;
		int y1 = y;
		int x2 = x + w;
		int y2 = y + h;

		if (x1 < pbox->x1)
			x1 = pbox->x1;
		if (y1 < pbox->y1)
			y1 = pbox->y1;
		if (x2 > pbox->x2)
			x2 = pbox->x2;
		if (y2 > pbox->y2)
			y2 = pbox->y2;
		if (x1 >= x2 || y1 >= y2)
			continue;

		glamor_set_normalize_tcoords(txscale, tyscale,
					     x1 - x, y1 - y,
					     x2 - x, y2 - y, 1, texcoords);

		glamor_set_normalize_vcoords(xscale, yscale,
					     x1 + x_off, y1 + y_off,
					     x2 + x_off, y2 + y_off,
					     glamor_priv->yInverted,
					     vertices);

		dispatch->glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	}

#ifndef GLAMOR_GLES2
	dispatch->glDisable(GL_TEXTURE_2D);
#endif
	dispatch->glUseProgram(0);
	dispatch->glDisableVertexAttribArray(GLAMOR_VERTEX_POS);
	dispatch->glDisableVertexAttribArray(GLAMOR_VERTEX_SOURCE);

	dispatch->glDeleteTextures(1, &tex);
	if (glamor_priv->gl_flavor == GLAMOR_GL_DESKTOP)
		dispatch->glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	glamor_set_alu(dispatch, GXcopy);
	glamor_set_planemask(pixmap, ~0);
	glamor_put_dispatch(glamor_priv);

	glamor_destroy_pixmap(temp_pixmap);
	ret = TRUE;
	goto done;

fail:
	glamor_set_planemask(pixmap, ~0);

	if (!fallback
	    && glamor_ddx_fallback_check_pixmap(&pixmap->drawable))
		goto done;

	glamor_fallback("to %p (%c)\n",
			drawable, glamor_get_drawable_location(drawable));
	if (glamor_prepare_access(&pixmap->drawable, GLAMOR_ACCESS_RW)) {
		fbPutImage(drawable, gc, depth, x, y, w, h,
			   left_pad, image_format, bits);
		glamor_finish_access(&pixmap->drawable, GLAMOR_ACCESS_RW);
	}
	ret = TRUE;

done:
	return ret;
}

void
glamor_put_image(DrawablePtr drawable, GCPtr gc, int depth, int x, int y,
		 int w, int h, int left_pad, int image_format, char *bits)
{
	_glamor_put_image(drawable, gc, depth, x, y, w, h, 
			left_pad, image_format, bits, TRUE);
}

Bool
glamor_put_image_nf(DrawablePtr drawable, GCPtr gc, int depth, int x, int y,
		 int w, int h, int left_pad, int image_format, char *bits)
{
	return _glamor_put_image(drawable, gc, depth, x, y, w, h, 
				left_pad, image_format, bits, FALSE);
}

