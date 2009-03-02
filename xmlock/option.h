/* number of buttons, toggles, and string options */

#define  PUSHBUTTONS  3
#define  TOGGLES  23
#define  OPTIONS  6

#define XLOCK "xlock"
#define WINDOW_WIDTH 160
#define WINDOW_HEIGHT 100
#define WINDOW_GEOMETRY "160x100"

/* This structure put together the description
   in the menu , the data , and commande associate
*/

typedef struct optionstruct {
	char *description;
	char *cmd;
	char *userdata;
	} OptionStruct;

void exitcallback(Widget w, XtPointer client_data, XtPointer call_data);
