#ifndef KCGUI_WIDGETS_H
#define KCGUI_WIDGETS_H


#define KCUGUI_CB_NO_CHANGE 0 //Will not have any effect on the menu
#define KCUGUI_CB_REFRESH 1 //Will refresh the listing
#define KCUGUI_CB_CANCEL 2 //Will cause the menu to exit with a return value of -1

//Gets called when an unknown key (start, select, b, left, right) is called during a file
//selection. Cb has the opportunity to modify the text on top as well as the filter arg (which is
//be a ptr to the glob value in kcugui_filechooser).
//Return one of KCUGUI_CB_*
typedef int (*kcugui_filechooser_cb_t)(int button, void **filterarg, char **desc, void *usrptr);

//Pass this to not show extensions
#define KCUGUI_FILE_FLAGS_NOEXT (1<<0)

int kcugui_filechooser(char *glob, char *desc, kcugui_filechooser_cb_t cb, void *usrptr, int flags);

//Custom filter for kcugui_filechooser_filter. Return 1 on match (display), 0 on no match (don't display).
typedef int (*fc_filtercb_t)(const char *name, void *filterarg);
//Standard filter for globbing as used in kcugui_filechooser
int kcugui_filechooser_filter_glob(const char *name, void *filterarg);

int kcugui_filechooser_filter(fc_filtercb_t filter, void *filterarg, char *desc, kcugui_filechooser_cb_t cb, void *usrptr, int flags);


#define KCUGUI_MENUITEM_LAST (1<<0)

typedef struct {
	char *name;
	int flags;
	void *user;
} kcugui_menuitem_t;

//Return one of KCUGUI_CB_*
typedef int (*kcugui_menu_cb_t)(int button, char **desc, kcugui_menuitem_t **menu, int item_selected, void *userptr);

int kcugui_menu(kcugui_menuitem_t *menu, char *desc, kcugui_menu_cb_t cb, void *usrptr);

#endif