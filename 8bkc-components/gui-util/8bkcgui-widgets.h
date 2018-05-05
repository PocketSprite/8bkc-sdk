#ifndef KCGUI_WIDGETS_H
#define KCGUI_WIDGETS_H

//Gets called when an unknown key (start, select, b, left, right) is called during a file
//selection. Cb has the opportunity to modify the text on top as well as the filter arg (which is
//be a ptr to the glob value in kcugui_filechooser).
//Return 1 if something has changed.
typedef int (*kcugui_filechooser_cb_t)(int button, void **filterarg, char **desc, void *usrptr);

int kcugui_filechooser(char *glob, char *desc, kcugui_filechooser_cb_t cb, void *usrptr);

//Custom filter for kcugui_filechooser_filter. Return 1 on match (display), 0 on no match (don't display).
typedef int (*fc_filtercb_t)(const char *name, void *filterarg);
//Standard filter for globbing as used in kcugui_filechooser
int kcugui_filechooser_filter_glob(const char *name, void *filterarg);

int kcugui_filechooser_filter(fc_filtercb_t filter, void *filterarg, char *desc, kcugui_filechooser_cb_t cb, void *usrptr);


#define KCUGUI_MENUITEM_LAST (1<<0)

typedef struct {
	char *name;
	int flags;
	void *user;
} kcugui_menuitem_t;

typedef int (*kcugui_menu_cb_t)(int button, char **desc, kcugui_menuitem_t **menu, int item_selected, void *userptr);

int kcugui_menu(kcugui_menuitem_t *menu, char *desc, kcugui_menu_cb_t cb, void *usrptr);


#endif