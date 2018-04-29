/*
 Simple graphics library for the PocketSprite based on 8x8 tiles.
*/

#include "tilegfx.h"
#include "8bkc-hal.h"
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_timer.h"


static uint16_t *fb;
static tilegfx_rect_t fb_rect={0};
static uint64_t anim_start_time;

//Set this to 1 to check all writes to the framebuffer. If a tile rendering function contains an error leading
//to writes outside of the framebuffer, enabling this will cause an abort when that happens.
#define DEBUG_WRITES 0

#if !DEBUG_WRITES
#define CHECK_OOB_WRITE(addr)
#else
#define CHECK_OOB_WRITE(addr) do if (addr<fb || addr>=&fb[fb_rect.h*fb_rect.w]) abort(); while(0)
#endif


//Render a tile that is not clipped by the screen extremities
static void render_tile_full(uint16_t *dest, const uint16_t *tile, int trans_col) {
	if (trans_col==-1) {
		for (int y=0; y<8; y++) {
			CHECK_OOB_WRITE(&dest[0]);
			CHECK_OOB_WRITE(&dest[7]);
			memcpy(dest, tile, 8*2);
			tile+=8;
			dest+=fb_rect.w;
		}
	} else {
		for (int y=0; y<8; y++) {
			for (int x=0; x<8; x++) {
				if (tile[x]!=trans_col) {
					CHECK_OOB_WRITE(&dest[x]);
					dest[x]=tile[x];
				}
			}
			tile+=8;
			dest+=fb_rect.w;
		}
	}
}

//Render a tile that is / may be clipped by the screen extremities
//Argument clip is the clipping region in screen coordinates. Xstart/Ystart indicates the upper left corner of
//the tile; this may be outside of the clipping region.
static void render_tile_part(uint16_t *dest, const uint16_t *tile, int xstart, int ystart, const tilegfx_rect_t *clip, int trans_col) {
	for (int y=0; y<8; y++) {
		if (y+ystart>=clip->y && y+ystart<clip->y+clip->h) {
			for (int x=0; x<8; x++) {
				if (x+xstart>=clip->x && x+xstart<clip->x+clip->w && tile[x]!=trans_col) {
					CHECK_OOB_WRITE(&dest[x]);
					dest[x]=tile[x];
				}
			}
		}
		tile+=8;
		dest+=fb_rect.w;
	}
}

static int get_tile_idx(const tilegfx_map_t *tiles, int idx) {
	if (tiles->gfx->anim_offsets==NULL) return idx;
	int off=tiles->gfx->anim_offsets[idx];
	if (off==0xffff) return idx;
	const tilegfx_anim_frame_t *f=&tiles->gfx->anim_frames[off];
	uint64_t t_ms=(esp_timer_get_time()-anim_start_time)/1000;
	t_ms=t_ms%f->delay_ms; //first frame is total cycle len
	while(t_ms) {
		f++;
		if (t_ms < f->delay_ms) return f->tile;
		t_ms-=f->delay_ms;
	}
	return(idx); //should never happen
}

void tilegfx_tile_map_render(const tilegfx_map_t *tiles, int offx, int offy, const tilegfx_rect_t *rdest) {
	const tilegfx_rect_t *dest=rdest;
	tilegfx_rect_t mdest;
	if (dest==NULL) {
		//Whole screen; always safe
		dest=&fb_rect;
	} else {
		memcpy(&mdest, rdest, sizeof(mdest));
		dest=&mdest;
		//Crop dest to screen
		if (mdest.x<0) {
			offx+=-mdest.x;
			mdest.w+=mdest.x;
			mdest.x=0;
		} else if (mdest.x+mdest.w > fb_rect.w) {
			mdest.w-=(mdest.x+mdest.w)-fb_rect.w;
		}
		if (mdest.y<0) {
			offy+=-mdest.y;
			mdest.h+=mdest.y;
			mdest.y=0;
		} else if (mdest.y+mdest.h > fb_rect.h) {
			mdest.h-=(mdest.y+mdest.h)-fb_rect.h;
		}
		if (mdest.h<=0 || mdest.w<=0) return; //nothing to do
	}
	//Make sure to wrap around if offx/offy aren't in the tile map
	offx%=(tiles->w*8);
	offy%=(tiles->h*8);
	if (offx<0) offx+=tiles->w*8;
	if (offy<0) offy+=tiles->h*8;

	//Embiggen rendering field to start at the edges of all corner tiles.
	//We'll cut off the bits outside of dest when we get there.
	int sx=dest->x-(offx&7);
	int sy=dest->y-(offy&7);
	int ex=sx+((dest->w+(offx&7)+7)&~7);
	int ey=sy+((dest->h+(offy&7)+7)&~7);

	//x and y are the real onscreen coords that may fall outside the framebuffer.
	int tileposy=((offy/8)*tiles->w);
	uint16_t *p=fb+(fb_rect.w*sy)+sx;
	for (int y=sy; y<ey; y+=8) {
		int tileposx=offx/8;
		uint16_t *pp=p;
		for (int x=sx; x<ex; x+=8) {
			int tileno=tiles->tiles[tileposx+tileposy];
			if (tileno!=0xffff) {
				if (x < dest->x || y < dest->y || x+7 >= dest->x+dest->w || y+7 >= dest->y+dest->h) {
					render_tile_part(pp, &tiles->gfx->tile[get_tile_idx(tiles, tileno)*64], 
								x, y, dest, tiles->gfx->trans_col);
				} else {
					render_tile_full(pp, &tiles->gfx->tile[get_tile_idx(tiles, tileno)*64], tiles->gfx->trans_col);
				}
			}
			tileposx++;
			if (tileposx >= tiles->w) tileposx=0; //wraparound
			pp+=8; //we filled these 8 columns
		}
		tileposy+=tiles->w; //skip to next row
		if (tileposy >= tiles->h*tiles->w) tileposy-=tiles->h*tiles->w; //wraparound
		p+=fb_rect.w*8; //we filled these 8 lines
	}
}

esp_timer_handle_t vbl_timer=NULL;
SemaphoreHandle_t vbl_sema=NULL;

static void vbl_cb(void *arg) {
	xSemaphoreGive(vbl_sema);
}

int tilegfx_init(int doublesize, int hz) {
	if (doublesize) {
		fb_rect.w=KC_SCREEN_W*2;
		fb_rect.h=KC_SCREEN_H*2;
	} else {
		fb_rect.w=KC_SCREEN_W;
		fb_rect.h=KC_SCREEN_H;
	}
	fb=malloc(fb_rect.w*fb_rect.h*2);
	if (!fb) goto err;
	vbl_sema=xSemaphoreCreateBinary();
	if (!vbl_sema) goto err;
	const esp_timer_create_args_t args={
		.callback=vbl_cb,
		.arg=NULL,
		.dispatch_method=ESP_TIMER_TASK,
		.name="vbl"
	};
	esp_err_t err=esp_timer_create(&args, &vbl_timer);
	if (err!=ESP_OK) goto err;
	esp_timer_start_periodic(vbl_timer, 1000000/hz);
	anim_start_time=0;
	return 1;
err:
	tilegfx_deinit();
	return 0;
}

void tilegfx_deinit() {
	if (fb) {
		free(fb);
		fb=NULL;
	}
	if (vbl_timer) {
		esp_timer_delete(vbl_timer);
		vbl_timer=NULL;
	}
	if (vbl_sema) {
		vSemaphoreDelete(vbl_sema);
		vbl_sema=NULL;
	}
}

//32-bit pixel:
// p1              p2
// rrrrrggggggbbbbbrrrrrggggggbbbbb
//We try to do subpixel rendering, and to this effect, for the subpixel itself:
//r=r1*0.75+r2*0.25
//g=g1*0.5+g2*0.5
//b=b1*0.25+b2*0.75
//Obviously, we also have to average the two lines of pixels together to keep the aspect ratio.

//rrrr.0ggg.gg0b.bbb0
//To take the lsb off, and with 0xf7be

//Takes the double-sized fb, scales it back to something that can actually be rendered.
void undo_x2_scaling() {
	uint32_t *fbw=(uint32_t*)fb; //fb but accessed 2 16-bit pixels at a time
	uint16_t *fbp=fb;
	for (int y=0; y<KC_SCREEN_H; y++) {
		for (int x=0; x<KC_SCREEN_W; x++) {
			uint32_t p=fbw[0];
			uint32_t p2=fbw[KC_SCREEN_W]; //one line down
			//Colors are stored with the bytes swapped. We need to swap them back.
			p=((p&0xFF00FF00)>>8)|((p&0x00ff00ff)<<8);
			p2=((p2&0xFF00FF00)>>8)|((p2&0x00ff00ff)<<8);
			p=((p&0xf7bef7be)>>1)+((p2&0xf7bef7be)>>1); //average both pixels
			
			//It's possible to do this using some code doing bitshifts and parallel
			//multiplies and stuff to get the most speed out of the CPU. This is not
			//that code. Pray to the GCC gods that this gets optimized into anything sane.
			int r=(((p>>27)&0x1F)*3+((p>>11)&0x1f)*1)/4;
			int g=(((p>>21)&0x3F)+((p>>5)&0x3F))/2;
			int b=(((p>>16)&0x1F)*1+((p>>0)&0x1f)*3)/4;
			uint16_t c=(r<<11)+(g<<5)+(b<<0);
			CHECK_OOB_WRITE(fbp);
			*fbp++=(c<<8)|(c>>8);
			fbw++;
		}
		fbw+=KC_SCREEN_W; //skip the next line; we already read that.
	}
}

void tilegfx_fade(uint8_t r, uint8_t g, uint8_t b, uint8_t pct) {
	//Pre-calculate mixed r, g, b components. rr, rg, rb already will be in the fb format
	//and can be ORed to be directly written into the fb. pct = 255 for transparent, 0 for only the rgb values given
	uint16_t rr[32], rg[64], rb[32];
	for (int i=0; i<32; i++) {
		int c=((i*8*pct+r*(255-pct))>>11)<<11;
		rr[i]=(c>>8)|(c<<8);
	}
	for (int i=0; i<64; i++) {
		int c=((i*4*pct+g*(255-pct))>>10)<<5;
		rg[i]=(c>>8)|(c<<8);
	}
	for (int i=0; i<32; i++) {
		int c=((i*8*pct+b*(255-pct))>>11)<<0;
		rb[i]=(c>>8)|(c<<8);
	}

	//Precalculation done. Do actual fade.
	for (int i=0; i<fb_rect.w*fb_rect.h; i++) {
		uint16_t c=fb[i];
		c=(c<<8)|(c>>8);
		fb[i]=rr[c>>11]|rg[(c>>5)&0x3f]|rb[c&0x1f];
	}
}

void tilegfx_flush() {
	if (fb_rect.w!=KC_SCREEN_W) {
		undo_x2_scaling();
	}
	kchal_send_fb(fb);
	xSemaphoreTake(vbl_sema, portMAX_DELAY);
}

tilegfx_map_t *tilegfx_create_tilemap(int w, int h, const tilegfx_tileset_t *tiles) {
	tilegfx_map_t *ret=malloc(sizeof(tilegfx_map_t)+h*w*2);
	if (!ret) return NULL;
	ret->w=w;
	ret->h=h;
	ret->gfx=tiles;
	memset((void*)ret->tiles, 0xff, h*w*2);
	return ret;
}


void tilegfx_destroy_tilemap(tilegfx_map_t *map) {
	free(map);
}

tilegfx_map_t *tilegfx_dup_tilemap(const tilegfx_map_t *orig) {
	tilegfx_map_t *ret=tilegfx_create_tilemap(orig->w, orig->h, orig->gfx);
	if (!ret) return NULL;
	memcpy((void*)ret->tiles, orig->tiles, ret->h*ret->w*2);
	return ret;
}

uint16_t *tilegfx_get_fb() {
	return fb;
}
