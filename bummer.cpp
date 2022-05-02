#include <SDL2/SDL.h>
#include <cassert>
#include <cmath>
#include <vector>

#define PI 3.14159

class SDLInstance {
public:
	SDL_Renderer *renderer;
	SDL_Window *win;
	const Uint8 *keyboard;
	std::vector<SDL_Texture*> textures;

	SDLInstance() {
		assert(SDL_Init(SDL_INIT_EVERYTHING) >= 0);
		win = SDL_CreateWindow("bummer",
				SDL_WINDOWPOS_UNDEFINED,
				SDL_WINDOWPOS_UNDEFINED,
				640, 480, SDL_WINDOW_RESIZABLE);
		assert(win != nullptr);
		renderer = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
		assert(renderer != nullptr);
		keyboard = SDL_GetKeyboardState(nullptr);
	}

	~SDLInstance() {
		for(SDL_Texture *t : textures)
			SDL_DestroyTexture(t);
		textures.clear();

		SDL_DestroyRenderer(renderer);
		SDL_DestroyWindow(win);
		SDL_Quit();
	}

	SDL_Texture *loadTexture(const char *filename) {
		SDL_Surface *surf = SDL_LoadBMP(filename);
		assert(surf != nullptr);
		SDL_SetColorKey(surf, SDL_TRUE, SDL_MapRGB(surf->format,
					0xff, 0, 0xff));
		SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer,
				surf);
		SDL_FreeSurface(surf);
		assert(tex != nullptr);
		textures.push_back(tex);
		return tex;
	}
};

float cross(float x1, float y1, float x2, float y2) {
	return x1*y2 - x2*y1;
}

void intersect(float x1, float y1, float x2, float y2,
		float x3, float y3, float x4, float y4,
		float *xp, float *yp) {
	float x = cross(x1, y1, x2, y2);
	float y = cross(x3, y3, x4, y4);
	float d = cross(x1-x2, y1-y2, x3-x4, y3-y4);
	x = cross(x, x1-x2, y, x3-x4) / d;
	y = cross(x, y1-y2, y, y3-y4) / d;
	*xp = x;
	*yp = y;
}

struct Camera {
	float x, y;
	float a;

	void forward(float d) {
		float dx = x - cosf(a-PI/2)*d;
		float dy = y + sinf(a-PI/2)*d;
		moveTo(dx, dy);
	}

	void strafe(float d) {
		float dx = x - cosf(a)*d;
		float dy = y + sinf(a)*d;
		moveTo(dx, dy);
	}

	void moveTo(float x, float y);
};

struct Wall {
	float x1, y1, x2, y2;
	unsigned char tint;

	float wx1, wx2, wh1, wh2;
	float z;

	Wall(float x1, float y1, float x2, float y2) {
		Wall::x1 = x1;
		Wall::y1 = y1;
		Wall::x2 = x2;
		Wall::y2 = y2;
		tint = 0xff;
		z = 0;
	}

	void project(SDLInstance &inst, Camera &cam) {
		float rx1 = x1+cam.x;
		float ry1 = y1+cam.y;
		float tx = cosf(cam.a)*rx1 - sinf(cam.a)*ry1;
		ry1 = sinf(cam.a)*rx1 + cosf(cam.a)*ry1;
		rx1 = tx;
		float rx2 = x2+cam.x;
		float ry2 = y2+cam.y;
		tx = cosf(cam.a)*rx2 - sinf(cam.a)*ry2;
		ry2 = sinf(cam.a)*rx2 + cosf(cam.a)*ry2;
		rx2 = tx;

		z = (ry1+ry2)/2;

		if(ry1 < 0 && ry2 < 0)
			return;
		else if(z < 0)
			z = 0;

		int w, h;
		SDL_GetWindowSize(inst.win, &w, &h);

		float ix1, iy1, ix2, iy2;
		intersect(rx1, ry1, rx2, ry2, 0, 0, -w/2, 20, &ix1, &iy1);
		intersect(rx1, ry1, rx2, ry2, 0, 0, w/2, 20, &ix2, &iy2);
		float ix = (ix1 > 0) ? ix1 : ix2;
		float iy = (iy1 > 0) ? iy1 : iy2;

		if(ry1 < 0) {
			rx1 = ix;
			ry1 = iy;
		}
		if(ry2 < 0) {
			rx2 = ix;
			ry2 = iy;
		}

		wh1 = (h)/-ry1;
		wh2 = (h)/-ry2;
		wx1 = rx1*256 / ry1 + w/2;
		wx2 = rx2*256 / ry2 + w/2;
		if(wx1 > wx2) {
			float f = wx1;
			wx1 = wx2;
			wx2 = f;
			f = wh1;
			wh1 = wh2;
			wh2 = f;
		}
	}

	void draw(SDLInstance &inst, SDL_Texture *tex) {
		int w, h;
		SDL_GetWindowSize(inst.win, &w, &h);

		int tw, th;
		SDL_QueryTexture(tex, nullptr, nullptr, &tw, &th);
		SDL_SetTextureColorMod(tex, tint, tint, tint);

		for(int x = wx1; x <= wx2; x++) {
		//for(float m = 0; m < 1; m += 0.01) {
			float m = (float)(x-wx1)/(float)(wx2-wx1);
			//int x = wx1 + (wx2-wx1)*m;
			int wh = (float)wh1 + (float)(wh2-wh1)*m;
			/*SDL_RenderDrawLine(inst.renderer, x, h/2-wh,
					x, h/2+wh);*/
			SDL_Rect src = (SDL_Rect){(int)(m*tw), 0, 1, th};
			SDL_Rect dst = (SDL_Rect){x, h/2+wh/2, 1, -wh};
			SDL_RenderCopy(inst.renderer, tex, &src, &dst);
		}
	}
};

std::vector<Wall> walls;

void Camera::moveTo(float dx, float dy) {
	x = dx;
	y = dy;
}

void addWall(float x1, float y1, float x2, float y2) {
	float a = atan2(y1-y2, x1-x2);
	if(a < 0)
		a += PI*2;
	if(a > PI*2)
		a -= PI*2;
	Wall w(x1, y1, x2, y2);
	w.tint = 0xff - (a/(PI))*(float)0x4f;
	walls.push_back(w);
}

void drawWalls(SDLInstance &inst, Camera &cam, SDL_Texture *tex) {
	int *order = new int[walls.size()];

	for(int i = 0; i < walls.size(); i++)
		order[i] = i;

	for(Wall &w : walls)
		w.project(inst, cam);

	while(true) {
		int f = -1;
		for(int i = 1; i < walls.size(); i++) {
			Wall &w1 = walls.at(order[i-1]);
			Wall &w2 = walls.at(order[i]);

			if(w1.z < w2.z) {
				f = order[i-1];
				order[i-1] = order[i];
				order[i] = f;
			}
		}
		if(f == -1)
			break;
	}

	for(int i = 0; i < walls.size(); i++)
		walls.at(order[i]).draw(inst, tex);

	delete order;
}

int main() {
	SDLInstance inst;
	bool quit = false;
	addWall(-5, 3, 5, 3);
	addWall(5, 3, 8, -1);
	addWall(-5, 3, -5, -7);
	addWall(-3, -5, -3, -9);
	addWall(2, -5, 2, -9);
	addWall(-3, -9, 2, -9);
	Camera cam = (Camera){0, 0, 0};
	int lastUpdate = SDL_GetTicks();
	SDL_Texture *wallTex = inst.loadTexture("img/wall1.bmp");

	while(!quit) {
		SDL_Event ev;
		while(SDL_PollEvent(&ev))
			switch(ev.type) {
			case SDL_QUIT:
				quit = true;
				break;
			}

		/* update */
		int currentTime = SDL_GetTicks();
		int diff = currentTime-lastUpdate;
		lastUpdate = currentTime;

		if(inst.keyboard[SDL_SCANCODE_LEFT]) {
			if(inst.keyboard[SDL_SCANCODE_LALT])
				cam.strafe(-0.01*diff);
			else
				cam.a -= 0.002*diff;
		}
		else if(inst.keyboard[SDL_SCANCODE_RIGHT]) {
			if(inst.keyboard[SDL_SCANCODE_LALT])
				cam.strafe(0.01*diff);
			else
				cam.a += 0.002*diff;
		}
		if(inst.keyboard[SDL_SCANCODE_UP])
			//cam.y -= 0.01*diff;
			cam.forward(0.01*diff);
		else if(inst.keyboard[SDL_SCANCODE_DOWN])
			cam.forward(-0.01*diff);

		/* draw */
		SDL_SetRenderDrawColor(inst.renderer, 0, 0, 0, 0xff);
		SDL_RenderClear(inst.renderer);

		SDL_SetRenderDrawColor(inst.renderer, 10, 10, 10, 0xff);
		int w, h;
		SDL_GetWindowSize(inst.win, &w, &h);
		SDL_Rect r = (SDL_Rect){0, h/2, w, h/2};
		SDL_RenderFillRect(inst.renderer, &r);

		drawWalls(inst, cam, wallTex);

		SDL_RenderPresent(inst.renderer);
	}
	return 0;
}
