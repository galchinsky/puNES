/*
 * opengl.c
 *
 *  Created on: 10/dic/2010
 *      Author: fhorse
 */

#include "opengl.h"
#include "gfx.h"
#include "ppu.h"
#include "cfg_file.h"
#include "openGL/no_effect.h"
#define _SHADERS_CODE_
#include "openGL/shaders.h"
#undef  _SHADERS_CODE_

void glsl_print_log(GLuint obj);
char *glsl_file2string(const char *path);

void sdl_init_gl(void) {
	opengl.rotation = 0;
	opengl.surface_gl = NULL;

	opengl.flags = SDL_HWSURFACE | SDL_OPENGL;
	opengl.factor_distance = 0;
	opengl.x_rotate = 0;
	opengl.y_rotate = 0;
	opengl.x_diff = 0;
	opengl.y_diff = 0;

	memset(&opengl.texture, 0, sizeof(_texture));
	memset(&shader, 0, sizeof(shader));

	opengl_init_effect = opengl_init_no_effect;
	opengl_set_effect = opengl_set_no_effect;
	opengl_unset_effect = opengl_unset_no_effect;
	opengl_draw_scene = opengl_draw_scene_no_effect;

	glew_init();
}
void sdl_quit_gl(void) {
	if (opengl.surface_gl) {
		SDL_FreeSurface(opengl.surface_gl);
	}

	if (opengl.texture.data) {
		glDeleteTextures(1, &opengl.texture.data);
	}

	if (shader.text.data) {
		glDeleteTextures(1, &shader.text.data);
	}

	if (opengl_unset_effect) {
		opengl_unset_effect();
	}

	glsl_delete_shaders(&shader);
}
void sdl_create_surface_gl(SDL_Surface *src, WORD width, WORD height, BYTE flags) {
	if (opengl.surface_gl) {
		SDL_FreeSurface(opengl.surface_gl);
		/*
		 * ripristino gli attributi opengl ai valori
		 * iniziali e li salvo nuovamente.
		 */
		glPopAttrib();
	}

	glPushAttrib(GL_ALL_ATTRIB_BITS);

	if (opengl.scale_force) {
		opengl.surface_gl = gfx_create_RGB_surface(src, SCR_ROWS * opengl.scale,
		        SCR_LINES * opengl.scale);
	} else {
		opengl.surface_gl = gfx_create_RGB_surface(src, width, height);
	}

	opengl_create_texture(&opengl.texture, opengl.surface_gl->w, opengl.surface_gl->h,
			opengl.interpolation, POWER_OF_TWO);

	opengl.texture.x = (GLfloat) width  / (opengl.texture.w * opengl.factor);
	opengl.texture.y = (GLfloat) height / (opengl.texture.h * opengl.factor);

	{
		/* aspect ratio */
		opengl.w_texture = src->w;
		opengl.h_texture = src->h;
		opengl.x_texture1 = 0;
		opengl.y_texture1 = 0;
		opengl.x_texture2 = src->w;
		opengl.y_texture2 = src->h;

		/* con flags intendo sia il fullscreen che il futuro resize */
		if (flags && cfg->aspect_ratio) {
			GLfloat ratio_surface = (GLfloat) opengl.w_texture / (GLfloat) opengl.h_texture;
			GLfloat ratio_frame = (GLfloat) width / (GLfloat) height;

			//ratio_frame = (float) 4 / 3;
			//ratio_frame = (float) 16 / 9;

			//fprintf(stderr, "opengl : %f %f\n", ratio_surface, ratio_frame);

			/*
			 * se l'aspect ratio del frame e' maggiore di
			 * quello della superficie allora devo agire
			 * sull'altezza.
			 */
			if (ratio_frame > ratio_surface) {
				GLint centering_factor = 0;

				opengl.h_texture = opengl.w_texture / ratio_frame;
				centering_factor = (src->h - opengl.h_texture) / 2;

				opengl.x_texture1 = 0;
				opengl.y_texture1 = centering_factor;
				opengl.x_texture2 = opengl.w_texture;
				opengl.y_texture2 = opengl.h_texture + centering_factor;
				/*
				 * se l'aspect ratio del frame e' minore di
				 * quello della superficie allora devo agire
				 * sulla larghezza.
				 */
			} else if (ratio_frame < ratio_surface) {
				GLint centering_factor = 0;

				opengl.w_texture = ratio_frame * opengl.h_texture;
				centering_factor = (src->w - opengl.w_texture) / 2;

				opengl.x_texture1 = centering_factor;
				opengl.y_texture1 = 0;
				opengl.x_texture2 = opengl.w_texture + centering_factor;
				opengl.y_texture2 = opengl.h_texture;
			}
		}
	}

	if (opengl.glsl.enabled && opengl.glsl.shader_used) {
		opengl_create_texture(&shader.text, opengl.texture.w * opengl.factor,
				opengl.texture.w * opengl.factor, FALSE, NO_POWER_OF_TWO);

		glsl_shaders_init(&shader);
	}

	opengl_unset_effect();
	opengl_set_effect(src);

	glFinish();
}

void opengl_enable_texture(void) {
	glEnable(GL_TEXTURE_2D);

	if (opengl.glsl.shader_used) {
		glUseProgram(shader.prg);

		if (shader.loc.frame_counter != -1) {
			glUniform1f(shader.loc.frame_counter, (GLfloat) ppu.frames);
		}
	}
}
void opengl_create_texture(_texture *texture, uint32_t width, uint32_t height,
        uint8_t interpolation, uint8_t pow) {
	switch (opengl.surface_gl->format->BitsPerPixel) {
		case 16:
			texture->format_internal = GL_RGB5;
			if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
				texture->format = GL_BGR;
				texture->type = GL_UNSIGNED_SHORT_5_6_5_REV;
			} else {
				texture->format = GL_RGB;
				texture->type = GL_UNSIGNED_SHORT_5_6_5;
			}
			break;
		case 24:
			texture->format_internal = GL_RGB8;
			if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
				texture->format = GL_BGR;
			} else {
				texture->format = GL_RGB;
			}
			texture->type = GL_UNSIGNED_BYTE;
			break;
		case 32:
		default:
			texture->format_internal = GL_RGBA8;
			texture->format = GL_BGRA;
			texture->type = GL_UNSIGNED_BYTE;
			break;
	}

	if (pow) {
		texture->w = opengl_power_of_two(width);
		texture->h = opengl_power_of_two(height);
	} else {
		texture->w = width;
		texture->h = height;
	}

	glEnable(GL_TEXTURE_2D);

	if (texture->data) {
		glDeleteTextures(1, &texture->data);
	}

	glGenTextures(1, &texture->data);
	glBindTexture(GL_TEXTURE_2D, texture->data);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	if (interpolation) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	} else {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}

	if (opengl.glew && !GLEW_VERSION_3_1) {
		glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
	}

	{
		/* per sicurezza creo una superficie piu' grande del necessario */
		SDL_Surface *blank = gfx_create_RGB_surface(opengl.surface_gl, texture->w * 2,
		        texture->h * 2);

		memset(blank->pixels, 0, blank->w * blank->h * blank->format->BytesPerPixel);

		glTexImage2D(GL_TEXTURE_2D, 0, texture->format_internal, texture->w, texture->h, 0,
		        texture->format, texture->type, blank->pixels);

		SDL_FreeSurface(blank);
	}

	if (opengl.glew && GLEW_VERSION_3_1) {
		glGenerateMipmap(GL_TEXTURE_2D);
	}

	glDisable(GL_TEXTURE_2D);
}
void opengl_update_texture(SDL_Surface *surface, uint8_t generate_mipmap) {
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, opengl.texture.data);

	if (generate_mipmap && opengl.glew && !GLEW_VERSION_3_1) {
		glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
	}

	glPixelStorei(GL_UNPACK_ROW_LENGTH, surface->w);

	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, surface->w, surface->h, opengl.texture.format,
	        opengl.texture.type, surface->pixels);

	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

	if (generate_mipmap && opengl.glew && GLEW_VERSION_3_1) {
		glGenerateMipmap(GL_TEXTURE_2D);
	}

	if (opengl.glsl.shader_used && text.on_screen) {
		glBindTexture(GL_TEXTURE_2D, shader.text.data);
	}

	/* disabilito l'uso delle texture */
	glDisable(GL_TEXTURE_2D);
}

void opengl_text_clear(_txt_element *ele) {
	if (!ele->blank) {
		return;
	}

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, shader.text.data);

	glPixelStorei(GL_UNPACK_ROW_LENGTH, ele->w);

	glTexSubImage2D(GL_TEXTURE_2D, 0, ele->x, ele->y, ele->w, ele->h,
	        shader.text.format, shader.text.type, ele->blank->pixels);

	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

	glDisable(GL_TEXTURE_2D);
}
void opengl_text_blit(_txt_element *ele, SDL_Rect *dst_rect) {
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, shader.text.data);

	glPixelStorei(GL_UNPACK_ROW_LENGTH, dst_rect->w);

	glTexSubImage2D(GL_TEXTURE_2D, 0, dst_rect->x, dst_rect->y, dst_rect->w, dst_rect->h,
	        opengl.texture.format, opengl.texture.type, ele->surface->pixels);

	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

	glDisable(GL_TEXTURE_2D);
}

int opengl_flip(SDL_Surface *surface) {
	SDL_GL_SwapBuffers();

	return (0);
}
int opengl_power_of_two(int base) {
	int pot = 1;

	while (pot < base) {
		pot <<= 1;
	}
	return (pot);
}

void glew_init(void) {
	opengl.glsl.compliant = FALSE;
	/* opengl.glsl.enabled e' stato gia' settato dal file di configurazioen o
	 * dalla riga di comando.
	 * opengl.glsl.enabled = FALSE;
	 */
	opengl.glew = FALSE;

	if (opengl.supported){
		GLenum err;

		if ((err = glewInit()) != GLEW_OK) {
			fprintf(stderr, "INFO: %s\n", glewGetErrorString(err));
			opengl.glew = FALSE;
		} else {
			opengl.glew = TRUE;

			if (GLEW_VERSION_2_0) {
				fprintf(stderr, "INFO: OpenGL 2.0 supported. Glsl enabled.\n");
				opengl.glsl.compliant = TRUE;
			} else {
				fprintf(stderr, "INFO: OpenGL 2.0 not supported. Glsl disabled.\n");
				opengl.glsl.compliant = FALSE;
				opengl.glsl.enabled = FALSE;
			}

			if (GLEW_VERSION_3_1) {
				fprintf(stderr, "INFO: OpenGL 3.1 supported.\n");
			} else {
				fprintf(stderr, "INFO: OpenGL 3.1 not supported.\n");
			}
		}
	}
}

void glsl_shaders_init(_shader *shd) {
	shd->code = &shader_code[shd->id];

	/* program */
	shd->prg = glCreateProgram();

	/* vertex */
	if (shd->code->vertex != NULL) {
		shd->vrt = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(shd->vrt, 1, &shd->code->vertex, NULL);
		glCompileShader(shd->vrt);
#ifndef RELEASE
		glsl_print_log(shd->vrt);
#endif
		glAttachShader(shd->prg, shd->vrt);
	}

	/* fragment */
	if (shd->code->fragment != NULL) {
		shd->frg = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(shd->frg, 1, &shd->code->fragment, NULL);
		glCompileShader(shd->frg);
#ifndef RELEASE
		glsl_print_log(shd->frg);
#endif
		glAttachShader(shd->prg, shd->frg);
	}

	glLinkProgram(shd->prg);
#ifndef RELEASE
	glsl_print_log(shd->prg);
	printf("\n");
#endif

	glUseProgram(shd->prg);

	{
		GLfloat sse[2], svm[2], st[2], ft[2];

		sse[0] = (GLfloat) SCR_ROWS;
		sse[1] = (GLfloat) SCR_LINES;
		if ((shd->id == SHADER_CRT) || (shd->id == SHADER_CRT4)) {
			svm[0] = opengl.texture.w;
			svm[1] = opengl.texture.h;
		} else {
			svm[0] = (GLfloat) (opengl.x_texture2 - opengl.x_texture1);
			svm[1] = (GLfloat) (opengl.y_texture2 - opengl.y_texture1);

		}
		st[0] = opengl.texture.w;
		st[1] = opengl.texture.h;
		ft[0] = st[0] / svm[0];
		ft[1] = st[1] / svm[1];

		if ((shd->loc.size.screen_emu = glGetUniformLocation(shd->prg, "size_screen_emu")) >= 0) {
			glUniform2f(shd->loc.size.screen_emu, sse[0], sse[1]);
		}
		if ((shd->loc.size.video_mode = glGetUniformLocation(shd->prg, "size_video_mode")) >= 0) {
			glUniform2f(shd->loc.size.video_mode, svm[0], svm[1]);
		}
		if ((shd->loc.size.texture = glGetUniformLocation(shd->prg, "size_texture")) >= 0) {
			glUniform2f(shd->loc.size.texture,  st[0],  st[1]);
		}
		if ((shd->loc.size.factor = glGetUniformLocation(shd->prg, "factor")) >= 0) {
			glUniform2f(shd->loc.size.factor, ft[0], ft[1]);
		}
	}

	if ((shd->loc.frame_counter = glGetUniformLocation(shd->prg, "frame_counter")) >= 0) {
		glUniform1f(shd->loc.frame_counter, (GLfloat) ppu.frames);
	}

	glEnable(GL_TEXTURE_2D);

	if ((shd->loc.texture.scr = glGetUniformLocation(shd->prg, "texture_scr")) >= 0) {
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, opengl.texture.data);
		glUniform1i(shd->loc.texture.scr, 0);
	}

	if ((shd->loc.texture.txt = glGetUniformLocation(shd->prg, "texture_txt")) >= 0) {
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, shd->text.data);
		glUniform1i(shd->loc.texture.txt, 1);
	}

	glDisable(GL_TEXTURE_2D);

	glUseProgram(0);
}
void glsl_delete_shaders(_shader *shd) {
	/* routine */
	shd->id = SHADER_NONE;
	shd->code = NULL;

	/* vertex */
	if (shd->vrt) {
		glDeleteShader(shd->vrt);
	}
	shd->vrt = 0;

	/* fragment */
	if (shd->frg) {
		glDeleteShader(shd->frg);
	}
	shd->frg = 0;

	/* program */
	if (shd->prg) {
		glDeleteProgram(shd->prg);
	}
	shd->prg = 0;
}
void glsl_print_log(GLuint obj) {
	int info_log_length = 0, max_length;

	if (glIsShader(obj)) {
		glGetShaderiv(obj, GL_INFO_LOG_LENGTH, &max_length);
	} else {
		glGetProgramiv(obj, GL_INFO_LOG_LENGTH, &max_length);
	}

	{
		char info_log[max_length];

		if (glIsShader(obj)) {
			glGetShaderInfoLog(obj, max_length, &info_log_length, info_log);
		} else {
			glGetProgramInfoLog(obj, max_length, &info_log_length, info_log);
		}

		info_log[info_log_length] = 0;

		if (info_log_length > 0) {
			printf("INFO: %s", info_log);
		}
	}
}
char *glsl_file2string(const char *path) {
	FILE *fd;
	long len, r;
	char *str;

	if (!(fd = fopen(path, "r"))) {
		fprintf(stderr, "Can't open file '%s' for reading\n", path);
		return NULL;
	}

	fseek(fd, 0, SEEK_END);
	len = ftell(fd);

	printf("File '%s' is %ld long\n", path, len);

	fseek(fd, 0, SEEK_SET);

	if (!(str = malloc(len * sizeof(char)))) {
		fprintf(stderr, "Can't malloc space for '%s'\n", path);
		return NULL;
	}

	r = fread(str, sizeof(char), len, fd);

	str[r - 1] = '\0'; /* Shader sources have to term with null */

	fclose(fd);

	return str;
}




