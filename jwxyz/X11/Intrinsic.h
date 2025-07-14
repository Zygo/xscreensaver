#pragma once

typedef int Boolean;
typedef struct jwxyz_XtAppContext *XtAppContext;
typedef unsigned long XtInputMask;
#define XtIMXEvent 1
#define XtIMTimer 2
#define XtIMAlternateInput 4
#define XtIMSignal 8
#define XtIMAll (XtIMXEvent | XtIMTimer | XtIMAlternateInput | XtIMSignal) 