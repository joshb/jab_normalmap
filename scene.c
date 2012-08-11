/*
 * Copyright (C) 2005-2006 Josh A. Beam
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
#include <string.h>
#include <math.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <SDL/SDL.h>

#define PI_OVER_180 0.017453
#define DEG2RAD(a) ((float)(a) * PI_OVER_180)

#ifndef GL_FRAGMENT_PROGRAM_ARB
#define GL_FRAGMENT_PROGRAM_ARB		0x8804
#endif
#ifndef GL_PROGRAM_FORMAT_ASCII_ARB
#define GL_PROGRAM_FORMAT_ASCII_ARB	0x8875
#endif

extern unsigned char *read_pcx(const char *, unsigned int *, unsigned int *);

static void (*my_glGenProgramsARB)(GLuint, GLuint *) = NULL;
static void (*my_glBindProgramARB)(GLuint, GLuint) = NULL;
static void (*my_glProgramStringARB)(GLuint, GLuint, GLint, const GLbyte *) = NULL;

static void (*my_glActiveTextureARB)(GLenum) = NULL;
static void (*my_glMultiTexCoord3fvARB)(GLenum, const GLfloat *) = NULL;

typedef struct {
	float color[4];
	float pos[3];
	float rot;
	float height;
} Light;

#define NUM_LIGHTS 3
static Light lights[NUM_LIGHTS];

typedef struct {
	float pos[3];
	float texcoord[2];
	float matrix[9];
} Vertex;

typedef struct {
	float pos[3];
	Vertex **verts;
	unsigned int num_verts;
} Object;

static float
DotProduct(const float v1[3], const float v2[3])
{
    return (v1[0]*v2[0] + v1[1]*v2[1] + v1[2]*v2[2]);
}

static void
CrossProduct(const float v1[3], const float v2[3], float out[3])
{
    out[0] = v1[1] * v2[2] - v1[2] * v2[1];
    out[1] = v1[2] * v2[0] - v1[0] * v2[2];
    out[2] = v1[0] * v2[1] - v1[1] * v2[0];
}

static void
Normalize(float v[3])
{
    float f;

    f = 1.0f / sqrtf(DotProduct(v, v));
    v[0] *= f;
    v[1] *= f;
    v[2] *= f;
}

static void
TransformVectorByMatrix3x3(const float v[3], const float m[9], float out[3])
{
	out[0] = v[0] * m[0] + v[1] * m[3] + v[2] * m[6];
	out[1] = v[0] * m[1] + v[1] * m[4] + v[2] * m[7];
	out[2] = v[0] * m[2] + v[1] * m[5] + v[2] * m[8];
}

static void *
GetProcAddress(const char *name)
{
	return (void *)SDL_GL_GetProcAddress(name);
}

static void
SetFunctionPointers(void)
{
	my_glGenProgramsARB = (void (*)(GLuint, GLuint *))GetProcAddress("glGenProgramsARB");
	if(!my_glGenProgramsARB) {
		fprintf(stderr, "SetFunctionPointers(): glGenProgramsARB failed\n");
		exit(1);
	}
	my_glBindProgramARB = (void (*)(GLuint, GLuint))GetProcAddress("glBindProgramARB");
	if(!my_glBindProgramARB) {
		fprintf(stderr, "SetFunctionPointers(): glBindProgramARB failed\n");
		exit(1);
	}
	my_glProgramStringARB = (void (*)(GLuint, GLuint, GLint, const GLbyte *))GetProcAddress("glProgramStringARB");
	if(!my_glProgramStringARB) {
		fprintf(stderr, "SetFunctionPointers(): glProgramStringARB failed\n");
		exit(1);
	}
	my_glActiveTextureARB = (void (*)(GLuint))GetProcAddress("glActiveTexture");
	if(!my_glActiveTextureARB) {
		fprintf(stderr, "SetFunctionPointers(): glActiveTextureARB failed\n");
		exit(1);
	}
	my_glMultiTexCoord3fvARB = (void (*)(GLuint, const GLfloat *))GetProcAddress("glMultiTexCoord3fvARB");
	if(!my_glMultiTexCoord3fvARB) {
		fprintf(stderr, "SetFunctionPointers(): glMultiTexCoord3fvARB failed\n");
		exit(1);
	}
}

// Returns a string containing the text in
// a vertex/fragment program code file.
static char *
LoadProgramString(const char *filename)
{
	static char program_string[16384];
	FILE *fp;
	unsigned int len;

	fp = fopen(filename, "r");
	if(!fp)
		return NULL;

	len = fread(program_string, 1, 16384, fp);
	program_string[len] = '\0';
	fclose(fp);

	return program_string;
}

// Loads the vertex/fragment program with the
// given filename. Returns the number used to
// bind the program.
static unsigned int
LoadShader(GLuint type, const char *filename)
{
	unsigned int program_num;
	char *program_string;

	program_string = LoadProgramString(filename);
	if(!program_string) {
		fprintf(stderr, "LoadShader(): Unable to load %s\n", filename);
		return 0;
	}

	glEnable(type);
	my_glGenProgramsARB(1, &program_num);
	my_glBindProgramARB(type, program_num);
	my_glProgramStringARB(type, GL_PROGRAM_FORMAT_ASCII_ARB, strlen(program_string), (const GLbyte *)program_string);
	my_glBindProgramARB(type, 0);
	glDisable(type);

	return program_num;
}

// Loads the texture with the given filename.
// Returns the number used to bind the texture.
static unsigned int
LoadTexture(const char *filename)
{
	unsigned int tex_num;
	unsigned char *data;
	unsigned int width, height;

	data = read_pcx(filename, &width, &height);
	if(!data) {
		fprintf(stderr, "LoadTexture(): Unable to load %s\n", filename);
		return 0;
	}

	glGenTextures(1, &tex_num);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, tex_num);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, 3, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
	glBindTexture(GL_TEXTURE_2D, 0);

	return tex_num;
}

static Object *
ObjectNew(void)
{
	Object *obj;

	obj = malloc(sizeof(Object));
	if(!obj) {
		fprintf(stderr, "ObjectNew(): malloc failed\n");
		exit(1);
	}

	obj->pos[0] = 0.0f;
	obj->pos[1] = 0.0f;
	obj->pos[2] = 0.0f;
	obj->verts = NULL;
	obj->num_verts = 0;

	return obj;
}

static void
ObjectAddVertex(Object *obj, Vertex vert)
{
	Vertex **v;
	Vertex *new_vert;
	unsigned int new_num_verts;
	unsigned int i;

	// re-allocate the vertex pointer array
	new_num_verts = obj->num_verts + 1;
	v = malloc(sizeof(Vertex *) * new_num_verts);
	if(!v) {
		fprintf(stderr, "ObjectAddVertex(): malloc failed\n");
		exit(1);
	}

	// copy pointers from the old array to the new one
	for(i = 0; i < obj->num_verts; i++)
		v[i] = obj->verts[i];

	if(obj->verts)
		free(obj->verts);
	obj->verts = v;
	obj->num_verts = new_num_verts;

	// allocate memory for new vertex
	new_vert = malloc(sizeof(Vertex));
	if(!new_vert) {
		fprintf(stderr, "ObjectAddVertex(): malloc failed\n");
		exit(1);
	}

	// copy vertex argument to the memory allocated for the new vertex
	memcpy(new_vert, &vert, sizeof(Vertex));
	obj->verts[obj->num_verts - 1] = new_vert;
}

// Calculates tangent space matrices for each
// vertex of the given Object.
static void
ObjectCalculateVertexMatrices(Object *obj)
{
	unsigned int i, j;

	if(obj->num_verts % 4) {
		fprintf(stderr, "ObjectCalculateVertexMatrices(): bad obj->num_verts (%u)\n", obj->num_verts);
		exit(1);
	}

	for(i = 0; i < obj->num_verts; i += 4) {
		float s[3];
		float t[3];
		float n[3];

		for(j = 0; j < 3; j++)
			s[j] = obj->verts[i+3]->pos[j] - obj->verts[i+0]->pos[j];
		for(j = 0; j < 3; j++)
			t[j] = obj->verts[i+1]->pos[j] - obj->verts[i+0]->pos[j];

		Normalize(s);
		Normalize(t);
		CrossProduct(s, t, n);

		for(j = 0; j < 2; j++) {
			obj->verts[i+j]->matrix[0] = s[0];
			obj->verts[i+j]->matrix[3] = s[1];
			obj->verts[i+j]->matrix[6] = s[2];
			obj->verts[i+j]->matrix[1] = t[0];
			obj->verts[i+j]->matrix[4] = t[1];
			obj->verts[i+j]->matrix[7] = t[2];
			obj->verts[i+j]->matrix[2] = n[0];
			obj->verts[i+j]->matrix[5] = n[1];
			obj->verts[i+j]->matrix[8] = n[2];
		}
		if(i) {
			for(j = 1; j < 3; j++) {
				obj->verts[i-j]->matrix[0] = s[0];
				obj->verts[i-j]->matrix[3] = s[1];
				obj->verts[i-j]->matrix[6] = s[2];
				obj->verts[i-j]->matrix[1] = t[0];
				obj->verts[i-j]->matrix[4] = t[1];
				obj->verts[i-j]->matrix[7] = t[2];
				obj->verts[i-j]->matrix[2] = n[0];
				obj->verts[i-j]->matrix[5] = n[1];
				obj->verts[i-j]->matrix[8] = n[2];
			}
		}
	}
	memcpy(obj->verts[obj->num_verts - 1]->matrix, obj->verts[0]->matrix, sizeof(float) * 9);
	memcpy(obj->verts[obj->num_verts - 2]->matrix, obj->verts[1]->matrix, sizeof(float) * 9);
}

static void
ObjectRender(Object *obj)
{
	unsigned int i, j;
	float light_vector[3];
	float tmp[3];

	glBegin(GL_QUADS);
	for(i = 0; i < obj->num_verts; i++) {
		for(j = 0; j < NUM_LIGHTS; j++) {
			// create vector from vertex to light
			light_vector[0] = lights[j].pos[0] - obj->verts[i]->pos[0];
			// y direction in the normalmap differs from tangent space,
			// so the y direction in the light vector is flipped here
			light_vector[1] = -(lights[j].pos[1] - obj->verts[i]->pos[1]);
			light_vector[2] = lights[j].pos[2] - obj->verts[i]->pos[2];

			// transform light vector by the vertex tangent space matrix
			TransformVectorByMatrix3x3(light_vector, obj->verts[i]->matrix, tmp);

			// put transformed light vector into texture unit coordinates
			my_glMultiTexCoord3fvARB(GL_TEXTURE1_ARB+j, tmp);
		}

		glTexCoord2fv(obj->verts[i]->texcoord);
		glVertex3fv(obj->verts[i]->pos);
	}
	glEnd();
}

static Object *
CreateCylinder(void)
{
	const int step = 360 / 30;
	const float s_step = (1.0f / 360.0f) * (float)step;
	float s;
	float c1, s1, c2, s2;
	int i;
	Object *obj;

	obj = ObjectNew();

	s = 0.0f;
	for(i = 0; i < 360; i += step) {
		Vertex vert;

		c1 = cosf(DEG2RAD(i));
		s1 = sinf(DEG2RAD(i));
		c2 = cosf(DEG2RAD(i + step));
		s2 = sinf(DEG2RAD(i + step));

		vert.texcoord[0] = s; vert.texcoord[1] = 0.0f;
		vert.pos[0] = c1;
		vert.pos[2] = s1;
		vert.pos[1] = 1.0f;
		ObjectAddVertex(obj, vert);

		vert.texcoord[0] = s; vert.texcoord[1] = 1.0f;
		vert.pos[0] = c1;
		vert.pos[2] = s1;
		vert.pos[1] = -1.0f;
		ObjectAddVertex(obj, vert);

		vert.texcoord[0] = s + s_step; vert.texcoord[1] = 1.0f;
		vert.pos[0] = c2;
		vert.pos[2] = s2;
		vert.pos[1] = -1.0f;
		ObjectAddVertex(obj, vert);

		vert.texcoord[0] = s + s_step; vert.texcoord[1] = 0.0f;
		vert.pos[0] = c2;
		vert.pos[2] = s2;
		vert.pos[1] = 1.0f;
		ObjectAddVertex(obj, vert);

		s += s_step;
	}
	ObjectCalculateVertexMatrices(obj);

	return obj;
}

static unsigned int shader_num;
static unsigned int tex_num;
static unsigned int normalmap_tex_num;

static float camrot[3] = { 0.0f, 0.0f, 0.0f };

void
SceneInit(void)
{
	SetFunctionPointers();
	shader_num = LoadShader(GL_FRAGMENT_PROGRAM_ARB, "shader.pso");
	tex_num = LoadTexture("data/texture.pcx");
	normalmap_tex_num = LoadTexture("data/normalmap.pcx");

	/* setup red light */
	lights[0].color[0] = 1.0f;
	lights[0].color[1] = 0.0f;
	lights[0].color[2] = 0.0f;
	lights[0].color[3] = 1.0f;
	lights[0].pos[0] = 1.5f;
	lights[0].pos[1] = 0.0f;
	lights[0].pos[2] = 0.0f;
	lights[0].rot = 0.0f; 
	lights[0].height = 0.0f;

	/* setup green light */
	lights[1].color[0] = 0.0f;
	lights[1].color[1] = 1.0f;
	lights[1].color[2] = 0.0f;
	lights[1].color[3] = 1.0f;
	lights[1].pos[0] = -1.5f;
	lights[1].pos[1] = 0.0f;
	lights[1].pos[2] = 0.0f;
	lights[1].rot = M_PI * 0.75;
	lights[1].height = M_PI * 0.75;

	/* setup blue light */
	lights[2].color[0] = 0.0f;
	lights[2].color[1] = 0.0f;
	lights[2].color[2] = 1.0f;
	lights[2].color[3] = 1.0f;
	lights[2].pos[0] = -1.5f;
	lights[2].pos[1] = 0.0f;
	lights[2].pos[2] = 0.0f;
	lights[2].rot = M_PI * 1.5;
	lights[2].height = M_PI * 1.5;
}

static void
LightRender(Light *light)
{
	const float ls = 0.025f; // light size
	float *lightpos = light->pos;

	// render light
	glColor3fv(light->color);
	glBegin(GL_QUADS);
		glVertex3f(lightpos[0] - ls, lightpos[1] + ls, lightpos[2] + ls);
		glVertex3f(lightpos[0] - ls, lightpos[1] - ls, lightpos[2] + ls);
		glVertex3f(lightpos[0] + ls, lightpos[1] - ls, lightpos[2] + ls);
		glVertex3f(lightpos[0] + ls, lightpos[1] + ls, lightpos[2] + ls);

		glVertex3f(lightpos[0] - ls, lightpos[1] + ls, lightpos[2] - ls);
		glVertex3f(lightpos[0] - ls, lightpos[1] - ls, lightpos[2] - ls);
		glVertex3f(lightpos[0] + ls, lightpos[1] - ls, lightpos[2] - ls);
		glVertex3f(lightpos[0] + ls, lightpos[1] + ls, lightpos[2] - ls);

		glVertex3f(lightpos[0] - ls, lightpos[1] + ls, lightpos[2] - ls);
		glVertex3f(lightpos[0] - ls, lightpos[1] - ls, lightpos[2] - ls);
		glVertex3f(lightpos[0] - ls, lightpos[1] - ls, lightpos[2] + ls);
		glVertex3f(lightpos[0] - ls, lightpos[1] + ls, lightpos[2] + ls);

		glVertex3f(lightpos[0] + ls, lightpos[1] + ls, lightpos[2] + ls);
		glVertex3f(lightpos[0] + ls, lightpos[1] - ls, lightpos[2] + ls);
		glVertex3f(lightpos[0] + ls, lightpos[1] - ls, lightpos[2] - ls);
		glVertex3f(lightpos[0] + ls, lightpos[1] + ls, lightpos[2] - ls);

		glVertex3f(lightpos[0] - ls, lightpos[1] - ls, lightpos[2] + ls);
		glVertex3f(lightpos[0] - ls, lightpos[1] - ls, lightpos[2] - ls);
		glVertex3f(lightpos[0] + ls, lightpos[1] - ls, lightpos[2] - ls);
		glVertex3f(lightpos[0] + ls, lightpos[1] - ls, lightpos[2] + ls);

		glVertex3f(lightpos[0] + ls, lightpos[1] + ls, lightpos[2] + ls);
		glVertex3f(lightpos[0] + ls, lightpos[1] + ls, lightpos[2] - ls);
		glVertex3f(lightpos[0] - ls, lightpos[1] + ls, lightpos[2] - ls);
		glVertex3f(lightpos[0] - ls, lightpos[1] + ls, lightpos[2] + ls);
	glEnd();
}

void
SceneRender(void)
{
	static Object *cylinder = NULL;
	int i;

	if(!cylinder)
		cylinder = CreateCylinder();

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glLoadIdentity();
	glTranslatef(0.0f, 0.0f, -5.0f);
	glRotatef(camrot[0], 1.0f, 0.0f, 0.0f);
	glRotatef(camrot[1], 0.0f, 1.0f, 0.0f);
	glRotatef(camrot[2], 0.0f, 0.0f, 1.0f);

	// bind texture
	glBindTexture(GL_TEXTURE_2D, tex_num);

	// bind normalmap
	my_glActiveTextureARB(GL_TEXTURE1_ARB);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, normalmap_tex_num);

	// bind fragment program
	glEnable(GL_FRAGMENT_PROGRAM_ARB);
	my_glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, shader_num);

	// render cylinder
	glEnable(GL_CULL_FACE);
	ObjectRender(cylinder);
	glDisable(GL_CULL_FACE);

	// disable fragment program and unbind normalmap/texture
	glDisable(GL_FRAGMENT_PROGRAM_ARB);
	glBindTexture(GL_TEXTURE_2D, 0);
	my_glActiveTextureARB(GL_TEXTURE0_ARB);
	glBindTexture(GL_TEXTURE_2D, 0);

	// render each light
	for(i = 0; i < NUM_LIGHTS; i++)
		LightRender(&lights[i]);

	glColor3f(1.0f, 1.0f, 1.0f);
	SDL_GL_SwapBuffers();
}

void
SceneCycle(void)
{
	static unsigned int prev_ticks = 0;
	unsigned int ticks;
	float time;
	int i;

	if(!prev_ticks)
		prev_ticks = SDL_GetTicks();

	ticks = SDL_GetTicks();
	time = (float)(ticks - prev_ticks) / 100.0f;
	prev_ticks = ticks;

	camrot[1] += 1.0f * time;

	// cycle each light
	for(i = 0; i < NUM_LIGHTS; i++) {
		lights[i].rot += (M_PI / 180.0f) * 5.0f * time;
		lights[i].height += 0.2f * time;
		lights[i].pos[0] = cosf(lights[i].rot) * 1.5f;
		lights[i].pos[1] = sinf(lights[i].height);
		lights[i].pos[2] = sinf(lights[i].rot) * 1.5f;
	}
}

void
SceneToggleWireframe(void)
{
	static char wireframe = 0;

	wireframe ^= 1;
	if(wireframe)
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	else
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}
