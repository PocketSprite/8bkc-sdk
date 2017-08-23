#ifndef KCGUI_WIDGETS_H
#define KCGUI_WIDGETS_H

//Gets called when an unknown key (start, select, b, left, right) is called during a file
//selection. Cb has the opportunity to modify the text on top as well as the glob value.
typedef void (*kcgui_filechooser_cb_t)(int button, const char **glob, const char **desc, void *usrptr);

int kcugui_filechooser(const char *glob, const char *desc, kcgui_filechooser_cb_t *cb, void *usrptr);

#endif