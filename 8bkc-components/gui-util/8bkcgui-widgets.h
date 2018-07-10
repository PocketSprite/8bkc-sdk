#ifndef KCGUI_WIDGETS_H
#define KCGUI_WIDGETS_H

#ifdef __cplusplus
extern "C" {
#endif

#define KCUGUI_CB_NO_CHANGE 0 /*!< Will not have any effect on the menu */
#define KCUGUI_CB_REFRESH 1 /*!< Will refresh the listing */
#define KCUGUI_CB_CANCEL 2 /*!< Will cause the menu to exit with a return value of -1 */

/**
 * @brief Callback for the filechooser.
 *
 * Gets called when an unknown key (start, select, b, left, right) is called during a file
 * selection. Cb has the opportunity to modify the text on top as well as the filter arg (which is
 * be a ptr to the glob value in kcugui_filechooser).
 *
 * This callback is free to modify the things ``filterarg`` and ``desc`` point at, to change filter behaviour 
 * and/or the visible name of the menu.
 *
 * @param button Bitmap of KC_BTN_* values, for buttons pressed causing this callback
 * @param filterarg Pointer to filter arguments. Is a pointer to ``filterarg`` as passed to 
 *                  kcugui_filechooser_filter, or to ``glob`` (cast to a void*) for 
 *                  `kcugui_filechooser`.
 * @param desc Pointer to the description of the menu.
 * @param userptr User pointer, as passed to kcugui_filechooser/kcugui_filechooser_filter.
 *
 * @returns One of KCUGUI_CB_*
 */
typedef int (*kcugui_filechooser_cb_t)(int button, void **filterarg, char **desc, void *usrptr);

#define KCUGUI_FILE_FLAGS_NOEXT (1<<0) /*!< Pass this to not show extensions */

/**
 * @brief Show a menu allowing the user to select a file on the appfs
 *
 * This uses a comma-separated list of globs (http://man7.org/linux/man-pages/man7/glob.7.html) to
 * filter out which files will be shown and which ones will be hidden. A callback can be specified
 * to handle unknown keypresses; this callback is able to change the glob pattern as well as the
 * menu description.
 *
 * @note kcugui should be inited (using ``kcugui_init()``) before using this.
 *
 * @param glob List of comma-separated globs to filter files by (ex. "*.gb,*.gbc")
 * @param desc Description of menu, shown to the user
 * @param cb Callback called when unknown keys (left/right/b/select/start/power) are pressed
 * @param userptr Opaque pointer, passed straight to the callback function
 * @param flags One of KCUGUI_FILE_FLAGS_*
 * @returns Appfs fd of file selected, or -1 if callback returned KCUGUI_CB_CANCEL
 */
int kcugui_filechooser(char *glob, char *desc, kcugui_filechooser_cb_t cb, void *usrptr, int flags);

/**
 * @brief Custom filter definition for kcugui_filechooser_filter.
 *
 * @param name Filename to filter
 * @param filterarg Opaque arg as passed to kcugui_filechooser_filter
 * @returns Return 1 on match (display), 0 on no match (don't display).
 */
typedef int (*fc_filtercb_t)(const char *name, void *filterarg);


/**
 * @brief Standard filter for globbing as used in kcugui_filechooser
 *
 * @param name Filename to filter
 * @param filterarg Opaque arg as passed to kcugui_filechooser_filter. In this case, it's the list of globs.
 * @returns Return 1 on match (display), 0 on no match (don't display).
 */
int kcugui_filechooser_filter_glob(const char *name, void *filterarg);

/**
 * @brief Show a menu allowing the user to select a file on the appfs. Filter by user-supplied callback.
 *
 * This calls a filter function callback on each filename to decide if the menu will show the file
 * or not. An additional callback can be specified to handle unknown keypresses; this callback is able 
 * to change the filter data as well as the menu description.
 *
 * @param filter User-supplied filter callback that decides if a name is shown or not
 * @param filterarg Opaque argument to the filter callback function
 * @param desc Description of menu, shown to the user (11 chars max)
 * @param cb Callback called when unknown keys (left/right/b/select/start/power) are pressed, or NULL
 *           if unused.
 * @param userptr Opaque pointer, passed straight to the unknown key callback function
 * @param flags One of KCUGUI_FILE_FLAGS_*
 * @returns Appfs fd of file selected, or -1 if callback returned KCUGUI_CB_CANCEL
 */
int kcugui_filechooser_filter(fc_filtercb_t filter, void *filterarg, char *desc, kcugui_filechooser_cb_t cb, 
			void *usrptr, int flags);


#define KCUGUI_MENUITEM_LAST (1<<0)

/**
 * @brief Structure describing one menu item
 */
typedef struct {
	char *name;		/*!< Name as shown to the user */
	int flags;		/*!< OR'ed combination of KCUGUI_MENUITEM_* flags */
	void *user;		/*!< Opaque user value */
} kcugui_menuitem_t;

/**
 * @brief Callback for the menu system.
 *
 * Gets called when an unknown key (start, select, b, left, right) is called during a menu item
 * selection. Cb has the opportunity to modify the text on top as well as the menu itself.
 *
 * This callback is free to modify the things ``menu`` and ``desc`` point at, to change filter behaviour 
 * and/or the visible name of the menu.
 *
 * @param button Bitmap of KC_BTN_* values, for buttons pressed causing this callback
 * @param desc Pointer to the description of the menu.
 * @param menu Pointer to a pointer to the menu in use, possibly as passed to `kcugui_menu` but also possibly
 *             modified by an earlier call to this callback
 * @param item_selected Currently selected item
 * @param userptr User pointer, as passed to kcugui_filechooser/kcugui_filechooser_filter.
 * @returns One of KCUGUI_CB_*
 */
typedef int (*kcugui_menu_cb_t)(int button, char **desc, kcugui_menuitem_t **menu, int item_selected, void *userptr);

/**
 * @brief Show a generic menu
 * 
 * Shows a menu with the provided items, allowing the user to use up/down to select an item and a
 * to confirm.
 *
 * @note kcugui should be inited (using ``kcugui_init()``) before using this.
 *
 * @param menu Array of kcugui_menuitem_t items. The last item should be a dummy item that has 
 *             ``KCUGUI_MENUITEM_LAST`` set as a flag.
 * @param desc Description, as shown to user (11 chars max)
 * @param cb Callback to call if an unknown key is pressed, or NULL if unused.
 * @param usrptr Opaque pointer to pass to the callback
 * @returns Menu item chosen, or -1 if callback returned KCUGUI_CB_CANCEL
 */
int kcugui_menu(kcugui_menuitem_t *menu, char *desc, kcugui_menu_cb_t cb, void *usrptr);

#ifdef __cplusplus
}
#endif


#endif