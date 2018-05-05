
/*
Some easy-to-use-ish widgets for common use sceanarios. Need to have uGUI initialized. 
*/

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include <fnmatch.h>
#include <string.h>
#include "appfs.h"
#include "8bkc-hal.h"
#include "8bkc-ugui.h"
#include "ugui.h"
#include "8bkcgui-widgets.h"


//Filter for kcugui_filechooser_filter_glob. Accepts multiple globs separated by a comma: "*.gb,*.gbc"
int kcugui_filechooser_filter_glob(const char *name, void *filterarg) {
	const char *glob=(const char *)filterarg;
	for (const char *p=glob; p!=NULL; p=strchr(p, ',')) {
		char ppart[128];
		if (*p==',') p++; //skip over comma
		//Copy part of string to non-const array
		strncpy(ppart, p, sizeof(ppart));
		//Zero-terminate at ','. Make sure that worst-case it's terminated at the end of the local
		//array.
		char *pend=strchr(ppart, ',');
		if (pend!=NULL) *pend=0; else ppart[sizeof(ppart)-1]=0;
		//Try to match
		if (fnmatch(ppart, name, FNM_CASEFOLD)==0) {
			return 1;
		}
	}
	return 0;
}

static int nextFdFileForFilter(int fd, fc_filtercb_t filter, void *filterarg, const char **name) {
	while(1) {
		fd=appfsNextEntry(fd);
		if (fd==APPFS_INVALID_FD) break;
		appfsEntryInfo(fd, name, NULL);
		if (filter(*name, filterarg)) return fd;
	}
	return APPFS_INVALID_FD;
}

static void remove_ext(char *fn) {
	int dot=-1;
	for (int i=0; i<strlen(fn); i++) {
		if (fn[i]=='.') dot=i;
	}
	if (dot!=-1) fn[dot]=0;
}

int kcugui_filechooser_filter(fc_filtercb_t filter, void *filterarg, char *desc, kcugui_filechooser_cb_t cb, void *usrptr, int flags) {
	int scpos=-1;
	int curspos=0;
	int oldkeys=0xffff; //so we do not detect keys that were pressed on entering this
	int endpos=9999;
	int selFd=APPFS_INVALID_FD;
	int selPos=0;
	const char *name;
	while(1) {
		char selFn[65];
		int fd=APPFS_INVALID_FD;

		kcugui_cls();
		UG_FontSelect(&FONT_6X8);
		UG_SetForecolor(C_YELLOW);
		UG_PutString(0, 0, desc);
		
		//Grab first entry
		fd=nextFdFileForFilter(fd, filter, filterarg, &name);
		
		//Skip invisible entries.
		int p=0;
		while (p!=((scpos<0)?0:scpos) && fd!=APPFS_INVALID_FD) {
			fd=nextFdFileForFilter(fd, filter, filterarg, &name);
			p++;
		}
		
		selFn[0]=0;
		
		if (fd==APPFS_INVALID_FD && p==0) {
			UG_SetForecolor(C_RED);
			UG_PutString(0, 32, "*NO FILES*");
		} else {
			UG_SetForecolor(C_WHITE);
			for (int y=(scpos<0)?-scpos:0; y<6; y++) {
				if (fd==APPFS_INVALID_FD) {
					endpos=p-1;
					break;
				}
				if (p==curspos) {
					UG_SetForecolor(C_WHITE);
					selFd=fd;
					strncpy(selFn, name, 64);
					selFn[64]=0;
					selPos=12+8*y;
				} else {
					UG_SetForecolor(C_BLUE);
				}
				//stop name from wrapping around
				char truncnm[12];
				strncpy(truncnm, name, 11);
				truncnm[11]=0;
				int dot=-1;
				if (flags & KCUGUI_FILE_FLAGS_NOEXT) remove_ext(truncnm);
				//show
				UG_PutString(0, 12+8*y, truncnm);
				p++;
				//Grab next fd
				fd=nextFdFileForFilter(fd, filter, filterarg, &name);
			}
		}
		kcugui_flush();

		if (flags & KCUGUI_FILE_FLAGS_NOEXT) remove_ext(selFn);
		if (strlen(selFn)>11) strncat(selFn, "   ", 64);

		int prKeys;
			int scpos=0;
		do {
			int keys=kchal_get_keys();
			//Filter out keys that are just pressed
			prKeys=(keys^oldkeys)&keys;
			if (prKeys&KC_BTN_UP) {
				curspos--;
				if (curspos<0) curspos=0;
				if (scpos>(curspos-1)) scpos--;
			}
			if (prKeys&KC_BTN_DOWN) {
				curspos++;
				if (curspos>endpos) curspos--;
				if (curspos>(scpos+4)) scpos++;
			}
			if (prKeys&KC_BTN_A) {
				kchal_wait_keys_released();
				return selFd;
			}
			if (prKeys&(~(KC_BTN_UP|KC_BTN_DOWN|KC_BTN_A))) {
				if (cb) cb(prKeys, &filterarg, &desc, usrptr);
			}
			oldkeys=keys;
			vTaskDelay(30/portTICK_PERIOD_MS);
			if (strlen(selFn)>11) {
				scpos++;
				UG_SetForecolor(C_WHITE);
				int cp=scpos/6;
				for (int i=0; i<14; i++) {
					cp=cp%strlen(selFn);
					UG_PutChar(selFn[cp], i*6-(scpos%6), selPos, C_WHITE, C_BLACK);
					cp++;
				}
				kcugui_flush();
			}
		} while (prKeys==0);
	}
}

int kcugui_filechooser(char *glob, char *desc, kcugui_filechooser_cb_t cb, void *usrptr, int flags) {
	return kcugui_filechooser_filter(kcugui_filechooser_filter_glob, glob, desc, cb, usrptr, flags);
}

int kcugui_menu(kcugui_menuitem_t *menu, char *desc, kcugui_menu_cb_t cb, void *usrptr) {
	int scpos=-1;
	int curspos=0;
	int oldkeys=0xffff; //so we do not detect keys that were pressed on entering this
	int endpos=9999;
	int selPos=0;
	int selScr;
	while(1) {
		kcugui_cls();
		UG_FontSelect(&FONT_6X8);
		UG_SetForecolor(C_YELLOW);
		UG_PutString(0, 0, desc);
		
		int p=0;
		
		//Skip invisible entries.
		while (p!=((scpos<0)?0:scpos) && (menu[p].flags&KCUGUI_MENUITEM_LAST)==0) p++;
		
		if ((menu[p].flags&KCUGUI_MENUITEM_LAST) && p==0) {
			UG_SetForecolor(C_RED);
			UG_PutString(0, 32, "*NO ITEMS*");
			selPos=-1;
		} else {
			UG_SetForecolor(C_WHITE);
			for (int y=(scpos<0)?-scpos:0; y<6; y++) {
				if (menu[p].flags&KCUGUI_MENUITEM_LAST) {
					endpos=p-1;
					if (p==2) {
						UG_SetForecolor(C_BLUE);
						UG_PutString(0, 12+8*y, "[end]");
					}
					break;
				}
				if (p==curspos) {
					UG_SetForecolor(C_WHITE);
					selPos=12+8*y;
				} else {
					UG_SetForecolor(C_BLUE);
				}
				//stop name from wrapping around
				char truncnm[12];
				strncpy(truncnm, menu[p].name, 11);
				truncnm[11]=0;
				//show
				UG_PutString(0, 12+8*y, truncnm);
				p++;
			}
		}
		kcugui_flush();

		int prKeys;
		selScr=0;
		do {
			int keys=kchal_get_keys();
			//Filter out keys that are just pressed
			prKeys=(keys^oldkeys)&keys;
			if (prKeys&KC_BTN_UP) {
				curspos--;
				if (curspos<0) curspos=0;
				if (scpos>(curspos-1)) scpos--;
			}
			if (prKeys&KC_BTN_DOWN) {
				curspos++;
				if (curspos>endpos) curspos--;
				if (curspos>(scpos+4)) scpos++;
			}
			if (prKeys&KC_BTN_A) {
				kchal_wait_keys_released();
				return curspos;
			}
			if (prKeys&(~(KC_BTN_UP|KC_BTN_DOWN|KC_BTN_A))) {
				if (cb) cb(prKeys, &desc, &menu, curspos, usrptr);
			}
			
			vTaskDelay(30/portTICK_PERIOD_MS);
			if (selPos>0 && strlen(menu[curspos].name)>11) {
				selScr++;
				UG_SetForecolor(C_WHITE);
				int cp=selScr/6;
				for (int i=0; i<14; i++) {
					cp=cp%(strlen(menu[curspos].name)+3);
					char mc;
					if (cp>=strlen(menu[curspos].name)) {
						mc=' ';
					} else {
						mc=menu[curspos].name[cp];
					}
					UG_PutChar(mc, i*6-(selScr%6), selPos, C_WHITE, C_BLACK);
					cp++;
				}
				kcugui_flush();
			}
			
			oldkeys=keys;
		} while (prKeys==0);
	}
}