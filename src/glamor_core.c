/*
 * Copyright © 2001 Keith Packard
 * Copyright © 2008 Intel Corporation
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

/** @file glamor_core.c
 *
 * This file covers core X rendering in glamor.
 */

#ifdef HAVE_DIX_CONFIG_H
#include <dix-config.h>
#endif

#include <stdlib.h>

#include "glamor_priv.h"

const Bool
glamor_get_drawable_location(const DrawablePtr drawable)
{
	PixmapPtr pixmap = glamor_get_drawable_pixmap(drawable);
	glamor_pixmap_private *pixmap_priv =
	    glamor_get_pixmap_private(pixmap);
	glamor_screen_private *glamor_priv =
	    glamor_get_screen_private(drawable->pScreen);
	if (pixmap_priv == NULL || pixmap_priv->gl_fbo == 0)
		return 'm';
	if (pixmap_priv->fb == glamor_priv->screen_fbo)
		return 's';
	else
		return 'f';
}

GLint
glamor_compile_glsl_prog(glamor_gl_dispatch * dispatch, GLenum type,
			 const char *source)
{
	GLint ok;
	GLint prog;

	prog = dispatch->glCreateShader(type);
	dispatch->glShaderSource(prog, 1, (const GLchar **) &source, NULL);
	dispatch->glCompileShader(prog);
	dispatch->glGetShaderiv(prog, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		GLchar *info;
		GLint size;

		dispatch->glGetShaderiv(prog, GL_INFO_LOG_LENGTH, &size);
		info = malloc(size);

		dispatch->glGetShaderInfoLog(prog, size, NULL, info);
		ErrorF("Failed to compile %s: %s\n",
		       type == GL_FRAGMENT_SHADER ? "FS" : "VS", info);
		ErrorF("Program source:\n%s", source);
		FatalError("GLSL compile failure\n");
	}

	return prog;
}

void
glamor_link_glsl_prog(glamor_gl_dispatch * dispatch, GLint prog)
{
	GLint ok;

	dispatch->glLinkProgram(prog);
	dispatch->glGetProgramiv(prog, GL_LINK_STATUS, &ok);
	if (!ok) {
		GLchar *info;
		GLint size;

		dispatch->glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &size);
		info = malloc(size);

		dispatch->glGetProgramInfoLog(prog, size, NULL, info);
		ErrorF("Failed to link: %s\n", info);
		FatalError("GLSL link failure\n");
	}
}


Bool
glamor_prepare_access(DrawablePtr drawable, glamor_access_t access)
{
	PixmapPtr pixmap = glamor_get_drawable_pixmap(drawable);
	return glamor_download_pixmap_to_cpu(pixmap, access);
}

void
glamor_init_finish_access_shaders(ScreenPtr screen)
{
	glamor_screen_private *glamor_priv =
	    glamor_get_screen_private(screen);
	glamor_gl_dispatch *dispatch = &glamor_priv->dispatch;
	const char *vs_source =
	    "attribute vec4 v_position;\n"
	    "attribute vec4 v_texcoord0;\n"
	    "varying vec2 source_texture;\n"
	    "void main()\n"
	    "{\n"
	    "	gl_Position = v_position;\n"
	    "	source_texture = v_texcoord0.xy;\n" "}\n";

	const char *fs_source =
	    GLAMOR_DEFAULT_PRECISION
	    "varying vec2 source_texture;\n"
	    "uniform sampler2D sampler;\n"
	    "uniform int no_revert;\n"
	    "uniform int swap_rb;\n"
	    "void main()\n"
	    "{\n"
	    "   if (no_revert == 1) \n"
	    "    { \n"
	    "     if (swap_rb == 1)   \n"
	    "	  gl_FragColor = texture2D(sampler, source_texture).bgra;\n"
	    "     else \n"
	    "	  gl_FragColor = texture2D(sampler, source_texture).rgba;\n"
	    "    } \n"
	    "   else \n"
	    "    { \n"
	    "     if (swap_rb == 1)   \n"
	    "	    gl_FragColor = texture2D(sampler, source_texture).argb;\n"
	    "     else \n"
	    "	    gl_FragColor = texture2D(sampler, source_texture).abgr;\n"
	    "    } \n" "}\n";

	const char *set_alpha_source =
	    GLAMOR_DEFAULT_PRECISION
	    "varying vec2 source_texture;\n"
	    "uniform sampler2D sampler;\n"
	    "uniform int no_revert;\n"
	    "uniform int swap_rb;\n"
	    "void main()\n"
	    "{\n"
	    "   if (no_revert == 1) \n"
	    "    { \n"
	    "     if (swap_rb == 1)   \n"
	    "	  gl_FragColor = vec4(texture2D(sampler, source_texture).bgr, 1);\n"
	    "     else \n"
	    "	  gl_FragColor = vec4(texture2D(sampler, source_texture).rgb, 1);\n"
	    "    } \n"
	    "   else \n"
	    "    { \n"
	    "     if (swap_rb == 1)   \n"
	    "	  gl_FragColor = vec4(1,  texture2D(sampler, source_texture).rgb);\n"
	    "     else \n"
	    "	  gl_FragColor = vec4(1, texture2D(sampler, source_texture).bgr);\n"
	    "    } \n" "}\n";
	GLint fs_prog, vs_prog, avs_prog, set_alpha_prog;
	GLint sampler_uniform_location;

	glamor_priv->finish_access_prog[0] = dispatch->glCreateProgram();
	glamor_priv->finish_access_prog[1] = dispatch->glCreateProgram();

	vs_prog =
	    glamor_compile_glsl_prog(dispatch, GL_VERTEX_SHADER,
				     vs_source);
	fs_prog =
	    glamor_compile_glsl_prog(dispatch, GL_FRAGMENT_SHADER,
				     fs_source);
	dispatch->glAttachShader(glamor_priv->finish_access_prog[0],
				 vs_prog);
	dispatch->glAttachShader(glamor_priv->finish_access_prog[0],
				 fs_prog);

	avs_prog =
	    glamor_compile_glsl_prog(dispatch, GL_VERTEX_SHADER,
				     vs_source);
	set_alpha_prog =
	    glamor_compile_glsl_prog(dispatch, GL_FRAGMENT_SHADER,
				     set_alpha_source);
	dispatch->glAttachShader(glamor_priv->finish_access_prog[1],
				 avs_prog);
	dispatch->glAttachShader(glamor_priv->finish_access_prog[1],
				 set_alpha_prog);

	dispatch->glBindAttribLocation(glamor_priv->finish_access_prog[0],
				       GLAMOR_VERTEX_POS, "v_position");
	dispatch->glBindAttribLocation(glamor_priv->finish_access_prog[0],
				       GLAMOR_VERTEX_SOURCE,
				       "v_texcoord0");
	glamor_link_glsl_prog(dispatch,
			      glamor_priv->finish_access_prog[0]);

	dispatch->glBindAttribLocation(glamor_priv->finish_access_prog[1],
				       GLAMOR_VERTEX_POS, "v_position");
	dispatch->glBindAttribLocation(glamor_priv->finish_access_prog[1],
				       GLAMOR_VERTEX_SOURCE,
				       "v_texcoord0");
	glamor_link_glsl_prog(dispatch,
			      glamor_priv->finish_access_prog[1]);

	glamor_priv->finish_access_no_revert[0] =
	    dispatch->
	    glGetUniformLocation(glamor_priv->finish_access_prog[0],
				 "no_revert");

	glamor_priv->finish_access_swap_rb[0] =
	    dispatch->
	    glGetUniformLocation(glamor_priv->finish_access_prog[0],
				 "swap_rb");
	sampler_uniform_location =
	    dispatch->
	    glGetUniformLocation(glamor_priv->finish_access_prog[0],
				 "sampler");
	dispatch->glUseProgram(glamor_priv->finish_access_prog[0]);
	dispatch->glUniform1i(sampler_uniform_location, 0);
	dispatch->glUniform1i(glamor_priv->finish_access_no_revert[0], 1);
	dispatch->glUniform1i(glamor_priv->finish_access_swap_rb[0], 0);
	dispatch->glUseProgram(0);

	glamor_priv->finish_access_no_revert[1] =
	    dispatch->
	    glGetUniformLocation(glamor_priv->finish_access_prog[1],
				 "no_revert");
	glamor_priv->finish_access_swap_rb[1] =
	    dispatch->
	    glGetUniformLocation(glamor_priv->finish_access_prog[1],
				 "swap_rb");
	sampler_uniform_location =
	    dispatch->
	    glGetUniformLocation(glamor_priv->finish_access_prog[1],
				 "sampler");
	dispatch->glUseProgram(glamor_priv->finish_access_prog[1]);
	dispatch->glUniform1i(glamor_priv->finish_access_no_revert[1], 1);
	dispatch->glUniform1i(sampler_uniform_location, 0);
	dispatch->glUniform1i(glamor_priv->finish_access_swap_rb[1], 0);
	dispatch->glUseProgram(0);

}

void
glamor_finish_access(DrawablePtr drawable, glamor_access_t access_mode)
{
	PixmapPtr pixmap = glamor_get_drawable_pixmap(drawable);
	glamor_pixmap_private *pixmap_priv =
	    glamor_get_pixmap_private(pixmap);
	glamor_screen_private *glamor_priv =
	    glamor_get_screen_private(drawable->pScreen);
	glamor_gl_dispatch *dispatch = &glamor_priv->dispatch;

	if (!GLAMOR_PIXMAP_PRIV_HAS_FBO(pixmap_priv))
		return;

	if (access_mode != GLAMOR_ACCESS_RO) {
		glamor_restore_pixmap_to_texture(pixmap);
	}

	if (pixmap_priv->pbo != 0 && pixmap_priv->pbo_valid) {
		assert(glamor_priv->gl_flavor == GLAMOR_GL_DESKTOP);
		dispatch->glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
		dispatch->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
		pixmap_priv->pbo_valid = FALSE;
		dispatch->glDeleteBuffers(1, &pixmap_priv->pbo);
		pixmap_priv->pbo = 0;
	} else {
		free(pixmap->devPrivate.ptr);
	}

	pixmap->devPrivate.ptr = NULL;
}


/**
 * Calls uxa_prepare_access with UXA_PREPARE_SRC for the tile, if that is the
 * current fill style.
 *
 * Solid doesn't use an extra pixmap source, so we don't worry about them.
 * Stippled/OpaqueStippled are 1bpp and can be in fb, so we should worry
 * about them.
 */
Bool
glamor_prepare_access_gc(GCPtr gc)
{
	if (gc->stipple) {
		if (!glamor_prepare_access
		    (&gc->stipple->drawable, GLAMOR_ACCESS_RO))
			return FALSE;
	}
	if (gc->fillStyle == FillTiled) {
		if (!glamor_prepare_access(&gc->tile.pixmap->drawable,
					   GLAMOR_ACCESS_RO)) {
			if (gc->stipple)
				glamor_finish_access(&gc->
						     stipple->drawable, 
						     GLAMOR_ACCESS_RO);
			return FALSE;
		}
	}
	return TRUE;
}

/**
 * Finishes access to the tile in the GC, if used.
 */
void
glamor_finish_access_gc(GCPtr gc)
{
	if (gc->fillStyle == FillTiled)
		glamor_finish_access(&gc->tile.pixmap->drawable, GLAMOR_ACCESS_RO);
	if (gc->stipple)
		glamor_finish_access(&gc->stipple->drawable, GLAMOR_ACCESS_RO);
}

Bool
glamor_stipple(PixmapPtr pixmap, PixmapPtr stipple,
	       int x, int y, int width, int height,
	       unsigned char alu, unsigned long planemask,
	       unsigned long fg_pixel, unsigned long bg_pixel,
	       int stipple_x, int stipple_y)
{
	glamor_fallback("stubbed out stipple depth %d\n",
			pixmap->drawable.depth);
	return FALSE;
}

GCOps glamor_gc_ops = {
	.FillSpans = glamor_fill_spans,
	.SetSpans = glamor_set_spans,
	.PutImage = glamor_put_image,
	.CopyArea = glamor_copy_area,
	.CopyPlane = glamor_copy_plane,
	.PolyPoint = glamor_poly_point,
	.Polylines = glamor_poly_lines,
	.PolySegment = glamor_poly_segment,
	.PolyRectangle = miPolyRectangle,
	.PolyArc = miPolyArc,
	.FillPolygon = miFillPolygon,
	.PolyFillRect = glamor_poly_fill_rect,
	.PolyFillArc = miPolyFillArc,
	.PolyText8 = miPolyText8,
	.PolyText16 = miPolyText16,
	.ImageText8 = miImageText8,
	.ImageText16 = miImageText16,
	.ImageGlyphBlt = glamor_image_glyph_blt, //miImageGlyphBlt,
	.PolyGlyphBlt = glamor_poly_glyph_blt, //miPolyGlyphBlt,
	.PushPixels = glamor_push_pixels, //miPushPixels,
};

/**
 * uxa_validate_gc() sets the ops to glamor's implementations, which may be
 * accelerated or may sync the card and fall back to fb.
 */
void
glamor_validate_gc(GCPtr gc, unsigned long changes, DrawablePtr drawable)
{
	/* fbValidateGC will do direct access to pixmaps if the tiling has changed.
	 * Preempt fbValidateGC by doing its work and masking the change out, so
	 * that we can do the Prepare/finish_access.
	 */
#ifdef FB_24_32BIT
	if ((changes & GCTile) && fbGetRotatedPixmap(gc)) {
		gc->pScreen->DestroyPixmap(fbGetRotatedPixmap(gc));
		fbGetRotatedPixmap(gc) = 0;
	}

	if (gc->fillStyle == FillTiled) {
		PixmapPtr old_tile, new_tile;

		old_tile = gc->tile.pixmap;
		if (old_tile->drawable.bitsPerPixel !=
		    drawable->bitsPerPixel) {
			new_tile = fbGetRotatedPixmap(gc);
			if (!new_tile ||
			    new_tile->drawable.bitsPerPixel !=
			    drawable->bitsPerPixel) {
				if (new_tile)
					gc->pScreen->DestroyPixmap
					    (new_tile);
				/* fb24_32ReformatTile will do direct access of a newly-
				 * allocated pixmap.
				 */
				glamor_fallback
				    ("GC %p tile FB_24_32 transformat %p.\n",
				     gc, old_tile);

				if (glamor_prepare_access
				    (&old_tile->drawable,
				     GLAMOR_ACCESS_RO)) {
					new_tile =
					    fb24_32ReformatTile
					    (old_tile,
					     drawable->bitsPerPixel);
					glamor_finish_access
					    (&old_tile->drawable, GLAMOR_ACCESS_RO);
				}
			}
			if (new_tile) {
				fbGetRotatedPixmap(gc) = old_tile;
				gc->tile.pixmap = new_tile;
				changes |= GCTile;
			}
		}
	}
#endif
	if (changes & GCTile) {
		if (!gc->tileIsPixel) {
			glamor_pixmap_private *pixmap_priv =
			    glamor_get_pixmap_private(gc->tile.pixmap);
			if ((!GLAMOR_PIXMAP_PRIV_HAS_FBO(pixmap_priv))
			    && FbEvenTile(gc->tile.pixmap->drawable.width *
					  drawable->bitsPerPixel)) {
				glamor_fallback
				    ("GC %p tile changed %p.\n", gc,
				     gc->tile.pixmap);
				if (glamor_prepare_access
				    (&gc->tile.pixmap->drawable,
				     GLAMOR_ACCESS_RW)) {
					fbPadPixmap(gc->tile.pixmap);
					glamor_finish_access
					    (&gc->tile.pixmap->drawable, GLAMOR_ACCESS_RW);
				}
			}
		}
		/* Mask out the GCTile change notification, now that we've done FB's
		 * job for it.
		 */
		changes &= ~GCTile;
	}

	if (changes & GCStipple && gc->stipple) {
		/* We can't inline stipple handling like we do for GCTile because
		 * it sets fbgc privates.
		 */
		if (glamor_prepare_access
		    (&gc->stipple->drawable, GLAMOR_ACCESS_RW)) {
			fbValidateGC(gc, changes, drawable);
			glamor_finish_access(&gc->stipple->drawable, GLAMOR_ACCESS_RW);
		}
	} else {
		fbValidateGC(gc, changes, drawable);
	}

	gc->ops = &glamor_gc_ops;
}

static GCFuncs glamor_gc_funcs = {
	glamor_validate_gc,
	miChangeGC,
	miCopyGC,
	miDestroyGC,
	miChangeClip,
	miDestroyClip,
	miCopyClip
};

/**
 * exaCreateGC makes a new GC and hooks up its funcs handler, so that
 * exaValidateGC() will get called.
 */
int
glamor_create_gc(GCPtr gc)
{
	if (!fbCreateGC(gc))
		return FALSE;

	gc->funcs = &glamor_gc_funcs;

	return TRUE;
}

RegionPtr
glamor_bitmap_to_region(PixmapPtr pixmap)
{
	RegionPtr ret;
	glamor_fallback("pixmap %p \n", pixmap);
	if (!glamor_prepare_access(&pixmap->drawable, GLAMOR_ACCESS_RO))
		return NULL;
	ret = fbPixmapToRegion(pixmap);
	glamor_finish_access(&pixmap->drawable, GLAMOR_ACCESS_RO);
	return ret;
}

/* Borrow from cairo. */
Bool
glamor_gl_has_extension(char *extension)
{
	const char *gl_extensions;
	char *pext;
	int ext_len;
	ext_len = strlen(extension);

	gl_extensions = (const char *) glGetString(GL_EXTENSIONS);
	pext = (char *) gl_extensions;

	if (pext == NULL || extension == NULL)
		return FALSE;

	while ((pext = strstr(pext, extension)) != NULL) {
		if (pext[ext_len] == ' ' || pext[ext_len] == '\0')
			return TRUE;
		pext += ext_len;
	}
	return FALSE;
}

int
glamor_gl_get_version(void)
{
	int major, minor;
	const char *version = (const char *) glGetString(GL_VERSION);
	const char *dot = version == NULL ? NULL : strchr(version, '.');
	const char *major_start = dot;

	/* Sanity check */
	if (dot == NULL || dot == version || *(dot + 1) == '\0') {
		major = 0;
		minor = 0;
	} else {
		/* Find the start of the major version in the string */
		while (major_start > version && *major_start != ' ')
			--major_start;
		major = strtol(major_start, NULL, 10);
		minor = strtol(dot + 1, NULL, 10);
	}

	return GLAMOR_GL_VERSION_ENCODE(major, minor);
}
