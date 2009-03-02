#include <windows.h> 
#include <scrnsave.h>
#include <stdlib.h>
#include <time.h>

/* Win95 specific globals */
extern HWND hwnd;        // window handle
extern HDC hdc;          // device context
extern RECT rc;          // coords of the screen
extern HANDLE thread;    // thread handle
extern DWORD thread_id;  // thread id number

unsigned int xlockmore_create(void);
void xlockmore_destroy(void);
unsigned int xlockmore_timer(void);

BOOL WINAPI ScreenSaverConfigureDialog(hDlg, message, wParam, lParam)
	HWND hDlg;
	UINT message;
	WPARAM wParam;
	LPARAM lParam;
{
    switch(message) 
      {
      case WM_INITDIALOG:
        /* We really should initialize something here */
        break;
      case WM_COMMAND:
        switch(LOWORD(wParam)) 
          {
          case IDOK:
            MessageBox(hDlg, TEXT("Okie dokie"), TEXT("Confirmed"),
                       MB_ICONINFORMATION);
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

