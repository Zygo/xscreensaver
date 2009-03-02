// This structure come from smith.motif.tar.Z
// of "Designing X clients with Xt/Motif,"
// by Jerry D. Smith, Morgan Kaufmann, 1992
/*
Public data structures:
*/

/*
Constants:
*/

#define menu_TITLE              -1001
#define menu_SEPARATOR  -1002
#define menu_SUBMENU    -1003
#define menu_ENTRY              -1004
#define menu_TOG_ENTRY  -1005
#define menu_END                -1006

typedef struct _menu_entry {
        int type;
        char *label;                            /* can be NULL */
        char *instance_name;
        void (*callback)();                     /* can be NULL */
        XtPointer data;                         /* can be NULL */
        struct _menu_entry *menu;       /* can be NULL */
        Widget id;                                      /* RETURN */
} menu_entry;

#if 0
extern void create_menus( Widget parent,menu_entry menu[], XmStringCharSet char_set);
#endif

/* number of buttons, toggles, and string options */

#define  PUSHBUTTONS  3

#define XLOCK "xlock"
#define WINDOW_WIDTH 160
#define WINDOW_HEIGHT 100
#define WINDOW_GEOMETRY "160x100"


