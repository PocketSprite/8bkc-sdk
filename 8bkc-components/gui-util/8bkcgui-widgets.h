#ifndef KCGUI_WIDGETS_H
#define KCGUI_WIDGETS_H

//Gets called when an unknown key (start, select, b, left, right) is called during a file
//selection. Cb has the opportunity to modify the text on top as well as the glob value.
//Return 1 if something has changed.
typedef int (*kcugui_filechooser_cb_t)(int button, char **glob, char **desc, void *usrptr);

int kcugui_filechooser(char *glob, char *desc, kcugui_filechooser_cb_t cb, void *usrptr);


#define KCUGUI_MENUITEM_LAST (1<<0)

typedef struct {
	char *name;
	int flags;
	void *user;
} kcugui_menuitem_t;

typedef int (*kcugui_menu_cb_t)(int button, char **desc, kcugui_menuitem_t **menu, int item_selected, void *userptr);

int kcugui_menu(kcugui_menuitem_t *menu, char *desc, kcugui_menu_cb_t cb, void *usrptr);


#endif