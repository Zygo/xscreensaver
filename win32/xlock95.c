#include <windows.h> 
#include <scrnsave.h>
#include <stdlib.h>
#include <time.h>
#include "xlock.h"
#include "xlock95.h"

/* Win95 specific globals */
extern HWND hwnd;        // window handle
extern HDC hdc;          // device context
extern RECT rc;          // coords of the screen
extern HANDLE thread;    // thread handle
extern DWORD thread_id;  // thread id number

unsigned int xlockmore_create(void);
void xlockmore_destroy(void);
unsigned int xlockmore_timer(void);

static int *enabled = NULL;

char *xlock95_get_modelist() {
  int m;
  HKEY skey;
  char key[1024];
  static char modelist[1024] = ""; /* make this dynamic!! */
  if(enabled == NULL) {
    enabled = (int*)malloc((numprocs-1)*sizeof(int));
  }
  /* Populate enabled[] from registry */
  for(m=0; m<numprocs-1; m++) {	/* last one is "random" */
    sprintf(key, "Control Panel\\Screen Saver.xlock95\\%s",
	    LockProcs[m].cmdline_arg);
    if (RegOpenKeyEx(HKEY_CURRENT_USER, key, 0,KEY_READ,&skey) !=ERROR_SUCCESS)
      enabled[m] = 1; /* Default is enabled */
    else {
      TCHAR buf[10];
      DWORD size = sizeof(buf);
      if (RegQueryValueEx(skey,"Enabled",NULL,NULL,(LPBYTE)buf, &size) 
	  !=ERROR_SUCCESS)
	enabled[m] = 1; /* Default is enabled */
      else	
	enabled[m] = (strncmp(buf, "1", 2) == 0);
      RegCloseKey(skey);
    }
    /* Construct modelist suitable for random.c */
    if(enabled[m]) {
      snprintf(modelist, sizeof(modelist),
	       "%s%s,", modelist, LockProcs[m].cmdline_arg);
    }
  }
  return modelist;
}

BOOL WINAPI ScreenSaverConfigureDialog(hDlg, message, wParam, lParam)
	HWND hDlg;
	UINT message;
	WPARAM wParam;
	LPARAM lParam;
{
  int m;
  int nItem;
  HWND hwndList;
  TCHAR tchBuffer[100];
  HRESULT hr;
  HKEY skey;
  LONG res;
  char key[1024];

    switch(message) 
      {
      case WM_INITDIALOG:
	m=8;
	SendDlgItemMessage(hDlg, MODE_LIST, LB_SETTABSTOPS, 1, (LPARAM)&m);
	(void) xlock95_get_modelist(); /* Populate enabled[] list */
	for(m=0; m<numprocs-1; m++) {	/* last one is "random" */
	  sprintf(tchBuffer, "%s\t%s",
		  enabled[m]?"x":"", LockProcs[m].cmdline_arg);
	  SendDlgItemMessage(hDlg, MODE_LIST, LB_ADDSTRING, 0,
			     (LPARAM) tchBuffer);
	}
        break;
      case WM_COMMAND:
        switch(LOWORD(wParam)) 
          {
          case MODE_LIST:
            switch (HIWORD(wParam)) 
              {
              case LBN_SELCHANGE:
        	m = SendDlgItemMessage(hDlg, MODE_LIST, LB_GETCURSEL, 0, 0);
		if(m >= 0 && m < numprocs-1) {       	
		  SetDlgItemText(hDlg, DESC_LABEL, LockProcs[m].desc);  
		  SetDlgItemText(hDlg, DELAY_TEXT,
				 _itoa(LockProcs[m].def_delay, tchBuffer, 10));
		  SetDlgItemText(hDlg, COUNT_TEXT,
				 _itoa(LockProcs[m].def_count, tchBuffer, 10));
		  SetDlgItemText(hDlg, CYCLES_TEXT,
				 _itoa(LockProcs[m].def_cycles,tchBuffer, 10));
		  SendDlgItemMessage(hDlg, ENABLE_BUTTON, BM_SETCHECK,
				     (WPARAM)(enabled[m]?
					      BST_CHECKED:BST_UNCHECKED), 0);
		}
		return TRUE;
		break;
	      case LBN_DBLCLK:
		m = SendDlgItemMessage(hDlg, MODE_LIST, LB_GETCURSEL, 0, 0); 
        	if(m >= 0 && m < numprocs-1) {
		  enabled[m] = !enabled[m];
		
		  SendDlgItemMessage(hDlg, ENABLE_BUTTON, BM_SETCHECK,
				     (WPARAM)(enabled[m]?
					      BST_CHECKED:BST_UNCHECKED), 0);
		  sprintf(tchBuffer, "%c\t%s",
			  enabled[m]?'x':' ', LockProcs[m].cmdline_arg);
		  SendDlgItemMessage(hDlg, MODE_LIST, LB_DELETESTRING,
				     (WPARAM)m, 0);
		  SendDlgItemMessage(hDlg, MODE_LIST, LB_INSERTSTRING, m,
			      (LPARAM) tchBuffer);
		  SendDlgItemMessage(hDlg, MODE_LIST, LB_SETCURSEL, m, 0);
		}
	      }
            break;
          case ENABLE_BUTTON:
            switch (HIWORD(wParam)) 
              {
              case BN_CLICKED:
		m = SendDlgItemMessage(hDlg, MODE_LIST, LB_GETCURSEL, 0, 0);
		if(m >= 0 && m < numprocs-1) {
		  enabled[m] = !enabled[m];
		  SendDlgItemMessage(hDlg, ENABLE_BUTTON, BM_SETCHECK,
				     (WPARAM)(enabled[m]?
					      BST_CHECKED:BST_UNCHECKED), 0);
		  sprintf(tchBuffer, "%c\t%s",
			  enabled[m]?'x':' ', LockProcs[m].cmdline_arg);
		  SendDlgItemMessage(hDlg, MODE_LIST, LB_DELETESTRING, m, 0);
		  SendDlgItemMessage(hDlg, MODE_LIST, LB_INSERTSTRING, m,
				     (LPARAM)tchBuffer);
		  SendDlgItemMessage(hDlg, MODE_LIST, LB_SETCURSEL, m, 0);
		}
        	return TRUE;
              }
            break;
          case IDOK:
            {
              HKEY skey;
              char key[1024];
              char buf[20];
              for(m=0; m<numprocs-1; m++) {
        	sprintf(key, "Control Panel\\Screen Saver.xlock95\\%s",
        		LockProcs[m].cmdline_arg);
        	if (RegCreateKeyEx(HKEY_CURRENT_USER, key,
        			   0,0,0,KEY_ALL_ACCESS,0,&skey,0)
		    !=ERROR_SUCCESS) return;
        	sprintf(buf, "%d", enabled[m]);
        	RegSetValueEx(skey,"Enabled",0,REG_SZ,buf,1);
        	RegCloseKey(skey);
              }
            }
            EndDialog(hDlg, IDOK);
            return TRUE;
          case IDCANCEL:
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
          }
      }
    return FALSE;
}

LONG WINAPI ScreenSaverProc(HWND myhwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  static UINT         uTimer;   // timer identifier  
  switch (msg)
    {
    case WM_ERASEBKGND:
      {
        HDC myhdc=(HDC) wParam; RECT myrc; GetClientRect(myhwnd,&myrc);
        FillRect(myhdc, &myrc, (struct HBRUSH__ *)GetStockObject(BLACK_BRUSH));
      }
      break;

    case WM_CREATE:
      {
	int delay;
        hwnd = myhwnd;
        GetClientRect(hwnd, &rc); // get window coords
        hdc = GetDC(hwnd);        // get device context
        SetTextColor(hdc, RGB(255,255,255)); // set text foreground to white
        SetBkColor(hdc, RGB(0,0,0));         // set text background to black

	delay = xlockmore_create();
        // start the timer
	uTimer = SetTimer(hwnd, 1, delay/1000, NULL);
      }
      break;

    case WM_DESTROY:
      {
	// Stop the timer
	if (uTimer) 
	  KillTimer(hwnd, uTimer); 
        xlockmore_destroy();
      }
      break;

    case WM_TIMER:
      {
        return xlockmore_timer();
      }
      break;

    default:
      return DefScreenSaverProc(myhwnd, msg, wParam, lParam);
    }
  return 0;
}

BOOL WINAPI RegisterDialogClasses(HANDLE hInst)
{
  return TRUE;
}

