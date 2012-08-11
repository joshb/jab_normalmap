/*
 * Copyright (C) 2006 Josh A. Beam
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <GL/gl.h>
#include <SDL/SDL.h>

#define WINDOW_WIDTH	640
#define WINDOW_HEIGHT	480

extern void SceneInit(void);
extern void SceneRender(void);
extern void SceneCycle(void);
extern void SceneToggleWireframe(void);

static SDL_Surface *screen = NULL;
static int app_running = 1;

static int
SetVideoMode(void)
{
	screen = SDL_SetVideoMode(WINDOW_WIDTH, WINDOW_HEIGHT, 32, SDL_OPENGL | SDL_GL_DOUBLEBUFFER);
	if(!screen) {
		fprintf(stderr, "Couldn't set SDL video mode: %s\n", SDL_GetError());
		return 1;
	}

	SDL_WM_SetCaption("jab_normalmap - http://joshbeam.com/", NULL);

	return 0;
}

static void
HandleKeyPress(const SDL_Event *event)
{
	switch(event->key.keysym.sym) {
		default:
			break;
		case SDLK_ESCAPE:
			app_running = 0;
			break;
		case SDLK_w:
			SceneToggleWireframe();
			break;
		case SDLK_f:
			SDL_WM_ToggleFullScreen(screen);
			break;
	}
}

static void
CheckEvents(void)
{
	SDL_Event event;

	while(SDL_PollEvent(&event)) {
		switch(event.type) {
			case SDL_QUIT:
				app_running = 0;
				break;
			case SDL_KEYDOWN:
				HandleKeyPress(&event);
				break;
		}
	}
}

static void
SetPerspective(float fov, float aspect, float near, float far)
{
	float f;
	float mat[16];

	f = 1.0f / tanf(fov / 2.0f);

	mat[0] = f / aspect;
	mat[1] = 0.0f;
	mat[2] = 0.0f;
	mat[3] = 0.0f;

	mat[4] = 0.0f;
	mat[5] = f;
	mat[6] = 0.0f;
	mat[7] = 0.0f;

	mat[8] = 0.0f;
	mat[9] = 0.0f;
	mat[10] = (far + near) / (near  - far);
	mat[11] = -1.0f;

	mat[12] = 0.0f;
	mat[13] = 0.0f;
	mat[14] = (2.0f * far * near) / (near - far);
	mat[15] = 0.0f;

	glMultMatrixf(mat);
}

int
main(int argc, char *argv[])
{
	// initialize SDL
	if(SDL_Init(SDL_INIT_VIDEO) != 0) {
		fprintf(stderr, "Couldn't initialize SDL: %s\n", SDL_GetError());
		return 1;
	}

	// set SDL video mode
	if(SetVideoMode() != 0) {
		SDL_Quit();
		return 1;
	}

	// setup initial GL settings
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClearDepth(1.0f);
	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glCullFace(GL_FRONT);

	// set projection matrix
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	SetPerspective(M_PI / 4.0f, (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT, 0.1f, 200.0f);
	glMatrixMode(GL_MODELVIEW);

	SceneInit();

	// render/cycle the scene and check for events while running
	while(app_running) {
		SceneRender();
		SceneCycle();

		CheckEvents();
	}

	SDL_Quit();
	return 0;
}
