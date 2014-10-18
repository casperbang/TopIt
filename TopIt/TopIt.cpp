#include "stdafx.h"
#include "TopIt.h"
#include "stdio.h"

#define MAX_LOADSTRING 100
#define MAX_TITLESTRING 255

#define REDRAW_TIMER 1
#define ALPHA_TIMER 2
#define ALPHA_MAX 255
#define ALPHA_MIN 0
#define ALPHA_STEP 10

typedef enum { EDGE_LEFT, EDGE_RIGHT, EDGE_TOP, EDGE_BOTTOM } edge_t;

/*#define EDGE_LEFT 1
#define EDGE_RIGHT 2
#define EDGE_TOP 3
#define EDGE_BOTTOM 4*/
#define WS_EX_LAYERED 0x00080000

static const wchar_t TOPIT_ORIG_TITLE[] = L"TOPIT_ORIG_TITLE";
static const wchar_t TOPIT_ORIG_WS[] = L"TOPIT_ORIG_WS";

static const wchar_t TEXT_ALPHA[] = L" [Opacity: %i%%]";
static const wchar_t TEXT_ONTOP[] = L" [On top]";
static const wchar_t TEXT_OFFTOP[] = L" [Off top]";

// Global Variables:
HINSTANCE g_hInstance; // Current window instance
wchar_t szTitle[MAX_LOADSTRING]; // Title bar text of current window
wchar_t szWindowClass[MAX_LOADSTRING]; // The main window class name

HWND g_hWndHidden = NULL;
HWND g_hWndTopIt = NULL;
HWND g_hWndAlphaTimer = NULL;

RECT rectActive;
RECT rectSmartSize;

int AltPressed = false;
HMODULE hUserDll ;
HPEN g_hRectanglePen = NULL;

static ATOM hotKeyAtomOnTop,
hotKeyAtomHide,
hotKeyAtomNoBorder,
hotKeyAtomMoveRight,
hotKeyAtomMoveLeft,
hotKeyAtomMoveUp,
hotKeyAtomMoveDown,
hotKeyAtomMoveRightUp,
hotKeyAtomMoveRightDown,
hotKeyAtomMoveLeftUp,
hotKeyAtomMoveLeftDown,
hotKeyAtomSmartSize,
hotKeyAtomAlphaMinus,
hotKeyAtomAlphaPlus;


// Support functions

BOOL IsNotMyOwnWindowHandle(HWND hWnd)
{
	return (hWnd != g_hWndTopIt);
}

BOOL HasNoParent(HWND hWnd)
{
	return (GetParent(hWnd) == NULL);
}

BOOL IsResizable(HWND hWnd)
{
	return GetWindowLong(hWnd, GWL_STYLE) & WS_THICKFRAME;
}

BOOL IsWindowOnDesktop(HWND hWnd)
{
	wchar_t title[MAX_TITLESTRING];

	if (IsNotMyOwnWindowHandle(hWnd) && IsWindowVisible(hWnd) && HasNoParent(hWnd))
	{
		BOOL bNoOwner = GetWindow(hWnd, GW_OWNER) == NULL;
		LONG lExStyle = GetWindowLong(hWnd, GWL_EXSTYLE);
		if ((((lExStyle & WS_EX_TOOLWINDOW) == NULL) && bNoOwner) || ((lExStyle & WS_EX_APPWINDOW) && ~bNoOwner))
		{
			GetWindowTextW(hWnd, title, sizeof(title) / sizeof(wchar_t));
			if (title != NULL && !IsIconic(hWnd))
				return TRUE;
		}
	}
	return FALSE;
}

// Adjust specified rectSmartSize edge, such that it is as large as possible without overlapping existing window.
BOOL CALLBACK ExpandEdgeEnumWindowsProc(HWND hWnd, LPARAM lParam)
{
	RECT rectApps;
	GetWindowRect(hWnd, &rectApps);

	if (IsWindowOnDesktop(hWnd))
	{
		if (lParam == hotKeyAtomMoveLeft)
		{
			if (rectApps.right < rectActive.left && rectApps.bottom > rectActive.top && rectApps.top < rectActive.bottom)
			{
				rectSmartSize.left = rectApps.right;
				return TRUE;
			}
		}
		else if (lParam == hotKeyAtomMoveRight)
		{
			if (rectApps.left > rectActive.right && rectApps.bottom > rectActive.top && rectApps.top < rectActive.bottom)
			{
				rectSmartSize.right = rectApps.left;
				return TRUE;
			}
		}
		else if (lParam == hotKeyAtomMoveUp)
		{
			if (rectApps.bottom < rectActive.top && rectApps.right > rectActive.left && rectApps.left < rectActive.right)
			{
				rectSmartSize.top = rectApps.bottom; 
				return TRUE;
			}
		}
		else if (lParam == hotKeyAtomMoveDown)
		{
			if (rectApps.top > rectActive.bottom && rectApps.right > rectActive.left && rectApps.left < rectActive.right)
			{
				rectSmartSize.bottom = rectApps.top; 
				return TRUE;
			}
		}
	}
	return FALSE;
}

// Adjust specified rectSmartSize edge, such that it is as large as possible but smaller than current size.
BOOL CALLBACK ReduceEdgeEnumWindowsProc(HWND hWnd, LPARAM lParam)
{
	RECT rectApps;
	GetWindowRect(hWnd, &rectApps);

	if (IsWindowOnDesktop(hWnd))
	{
		if (lParam == hotKeyAtomMoveLeft)
		{
			if (rectApps.left < rectActive.right && rectApps.bottom > rectActive.top && rectApps.top < rectActive.bottom)
			{
				rectSmartSize.right = rectApps.left; 
				return TRUE;
			}
		}
		else if (lParam == hotKeyAtomMoveRight)
		{
			if (rectApps.right > rectActive.left && rectApps.bottom > rectActive.top && rectApps.top < rectActive.bottom)
			{
				rectSmartSize.left = rectApps.right;
				return TRUE;
			}
		}
		else if (lParam == hotKeyAtomMoveUp)
		{
			if (rectApps.bottom < rectActive.top && rectApps.right > rectActive.left && rectApps.left < rectActive.right)
			{
				rectSmartSize.top = rectApps.bottom; 
				return TRUE;
			}
		}
		else if (lParam == hotKeyAtomMoveDown)
		{
			if (rectApps.top > rectActive.bottom && rectApps.right > rectActive.left && rectApps.left < rectActive.right)
			{
				rectSmartSize.bottom = rectApps.top;
				return TRUE;
			}
		}
	}
	return FALSE;
}

/*
long HighlightWindowEdge(HWND hWndFoundWindow)
{
	HDC hWindowDC = NULL; // The DC of the found window.
	HGDIOBJ	hPrevPen = NULL; // Handle of the existing pen in the DC of the found window.
	HGDIOBJ	hPrevBrush = NULL; // Handle of the existing brush in the DC of the found window.
	RECT rect; // Rectangle area of the found window.
	long lRet = 0;

	// Get the screen coordinates of the rectangle of the found window.
	GetWindowRect(hWndFoundWindow, &rect);

	// Get the window DC of the found window.
	hWindowDC = GetWindowDC(hWndFoundWindow);

	if (hWindowDC)
	{
		// Select our created pen into the DC and backup the previous pen.
		hPrevPen = SelectObject(hWindowDC, g_hRectanglePen);

		// Select a transparent brush into the DC and backup the previous brush.
		hPrevBrush = SelectObject(hWindowDC, GetStockObject(HOLLOW_BRUSH));

		// Draw a rectangle in the DC covering the entire window area of the found window.
		Rectangle(hWindowDC, 0, 0, rect.right - rect.left, rect.bottom - rect.top);

		// Reinsert the previous pen and brush into the found window's DC.
		SelectObject(hWindowDC, hPrevPen);
		SelectObject(hWindowDC, hPrevBrush);

		// Finally release the DC.
		ReleaseDC(hWndFoundWindow, hWindowDC);
	}

	return lRet;
}
*/

long HighlightWindowEdge(HWND hWndFoundWindow, edge_t edge)
{
	HDC	hWindowDC = NULL; // The DC of the found window.
	HGDIOBJ	hPrevPen = NULL; // Handle of the existing pen in the DC of the found window.
	HGDIOBJ	hPrevBrush = NULL; // Handle of the existing brush in the DC of the found window.
	RECT rect; // Rectangle area of the found window.
	long lRet = 0;

	// Get the screen coordinates of the rectangle of the found window.
	GetWindowRect(hWndFoundWindow, &rect);

	// Get the window DC of the found window.
	hWindowDC = GetWindowDC(hWndFoundWindow);

	if (hWindowDC)
	{
		// Select our created pen into the DC and backup the previous pen.
		hPrevPen = SelectObject(hWindowDC, g_hRectanglePen);

		// Select a transparent brush into the DC and backup the previous brush.
		hPrevBrush = SelectObject(hWindowDC, GetStockObject(HOLLOW_BRUSH));

		// Draw
		if (edge == EDGE_LEFT)
		{
			MoveToEx(hWindowDC, 0, 0, NULL);
			LineTo(hWindowDC, 0, rect.bottom);
		}
		else if (edge == EDGE_RIGHT)
		{
			MoveToEx(hWindowDC, rect.right - rect.left - 2, 0, NULL);
			LineTo(hWindowDC, rect.right - rect.left - 2, rect.bottom);
		}
		else if (edge == EDGE_TOP)
		{
			MoveToEx(hWindowDC, 0, 0, NULL);
			LineTo(hWindowDC, rect.right - rect.left - 2, 0);
		}
		else if (edge == EDGE_BOTTOM)
		{
			MoveToEx(hWindowDC, 0, rect.bottom, NULL);
			LineTo(hWindowDC, rect.right - rect.left - 2, rect.bottom);
		}

		// Reinsert the previous pen and brush into the found window's DC.
		SelectObject(hWindowDC, hPrevPen);
		SelectObject(hWindowDC, hPrevBrush);

		// Finally release the DC.
		ReleaseDC(hWndFoundWindow, hWindowDC);
	}

	return lRet;
}



// Dispatching

void ToggleZOrder(HWND hWndForeground)
{

	// Place original title in a a dynamic allocated piece of memory and store it with the window
	int length = MAX_TITLESTRING * sizeof(wchar_t);
	wchar_t *strTitleOriginal = (wchar_t*)malloc(length);
	GetWindowTextW(hWndForeground, strTitleOriginal, length);
	SetPropW(hWndForeground, TOPIT_ORIG_TITLE, (HANDLE)strTitleOriginal);

	// Copy the obtained title into a buffer we'll modify
	wchar_t strTitleModified[MAX_TITLESTRING];
	wcscpy_s(strTitleModified, strTitleOriginal);
		
	if (GetWindowLong(hWndForeground, GWL_EXSTYLE) & WS_EX_TOPMOST)
	{
		SetWindowPos(hWndForeground, HWND_NOTOPMOST, NULL, NULL, NULL, NULL, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
		wcscat_s(strTitleModified, TEXT_OFFTOP);
	}
	else
	{
		SetWindowPos(hWndForeground, HWND_TOPMOST, NULL, NULL, NULL, NULL, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
		wcscat_s(strTitleModified, TEXT_ONTOP);
	}

	SetWindowTextW(hWndForeground, strTitleModified);
	
	Sleep(500); // Not nice, blocking app-message loop, change to timer based!

	HANDLE hOrigTitle = GetPropW(hWndForeground, TOPIT_ORIG_TITLE);
	if (hOrigTitle != NULL)
	{
		RemovePropW(hWndForeground, TOPIT_ORIG_TITLE);
		SetWindowTextW(hWndForeground, (LPCWSTR)hOrigTitle);
		free(hOrigTitle);
	}
}

void ToggleVisibility(HWND hWndForeground)
{

	long ws = GetWindowLong(hWndForeground, GWL_EXSTYLE);

	if (g_hWndHidden == NULL)
	{
		g_hWndHidden = hWndForeground;
		ShowWindow(hWndForeground, SW_HIDE);
	}
	else
	{
		ShowWindow(g_hWndHidden, SW_SHOW);
		SetForegroundWindow(g_hWndHidden);
		g_hWndHidden = NULL;
	}
}

void CancelPendingTimerForForegroundWindow(HWND hWndTopIt, HWND hWndForeground, int timerId)
{
	if (g_hWndAlphaTimer == hWndForeground)
	{
		// Then we can safely cancel it
		KillTimer(hWndTopIt, timerId);
		g_hWndAlphaTimer = NULL;
	}
}

void SetTransparancy(HWND hWndTopIt, HWND hWndForeground, BYTE alpha)
{
	SetLayeredWindowAttributes(hWndForeground, 0, alpha, LWA_ALPHA);

	// Reserve a char buffer for the original title
	wchar_t* strTitleOriginal;

	HANDLE hOrigTitle = GetPropW(hWndForeground, TOPIT_ORIG_TITLE);
	
	// Is the original title already stored in the window prop?
	if (hOrigTitle != NULL)
	{
		// Then we just reuse that one
		strTitleOriginal = (wchar_t*)hOrigTitle;
	}
	else
	{
		// Place original title in a a dynamic allocated piece of memory and store it with the window
		int length = MAX_TITLESTRING * sizeof(wchar_t);
		strTitleOriginal = (wchar_t*)malloc(length);
		GetWindowTextW(hWndForeground, strTitleOriginal, length);
		SetPropW(hWndForeground, TOPIT_ORIG_TITLE, (HANDLE)strTitleOriginal);
	}

	// Copy the obtained title into a buffer we'll modify
	wchar_t strTitleModified[MAX_TITLESTRING];
	wcscpy_s(strTitleModified, strTitleOriginal);

	wchar_t msg[MAX_TITLESTRING]; // Replace with sizeOf...
	double percentage = (100.0*(alpha / 255.0));
	swprintf_s(msg, TEXT_ALPHA, (int)percentage);
	wcscat_s(strTitleModified, msg);

	SetWindowTextW(hWndForeground, strTitleModified);

	// Start the timer that will set the text back
	SetTimer(hWndTopIt, ALPHA_TIMER, 1000, NULL);
}

void IncrementOpacity(HWND hWndTopIt, HWND hWndForeground)
{
	BYTE alpha;

	CancelPendingTimerForForegroundWindow(hWndTopIt, hWndForeground, ALPHA_TIMER);

	// If we have no existing timing update pending
	if (g_hWndAlphaTimer == NULL)
	{
		// Remember this window, we might need to send a message to it later
		g_hWndAlphaTimer = hWndForeground;

		DWORD dwStyle = GetWindowLong(hWndForeground, GWL_EXSTYLE);

		SetWindowLong(hWndForeground, GWL_EXSTYLE, dwStyle | WS_EX_LAYERED);

		if (GetLayeredWindowAttributes(hWndForeground, NULL, &alpha, NULL))
		{
			if (alpha < ALPHA_STEP)
				alpha = ALPHA_MIN;
			else
				alpha -= ALPHA_STEP;
		}
		else
			alpha = ALPHA_MAX;

		SetTransparancy(hWndTopIt, hWndForeground, alpha);
	}
}


void DecrementOpacity(HWND hWndTopIt, HWND hWndForeground)
{
	BYTE alpha;

	CancelPendingTimerForForegroundWindow(hWndTopIt, hWndForeground, ALPHA_TIMER);

	// If we have no existing timing update pending
	if (g_hWndAlphaTimer == NULL)
	{
		g_hWndAlphaTimer = hWndForeground;

		DWORD dwStyle = GetWindowLong(hWndForeground, GWL_EXSTYLE);

		SetWindowLong(hWndForeground, GWL_EXSTYLE, dwStyle | WS_EX_LAYERED);
		
		if (GetLayeredWindowAttributes(hWndForeground, NULL, &alpha, NULL))
		{
			if (alpha >= ALPHA_MAX - ALPHA_STEP)
				alpha = ALPHA_MAX;
			else
				alpha += ALPHA_STEP;
		}
		else
			alpha = ALPHA_MAX;

		SetTransparancy(hWndTopIt, hWndForeground, alpha);
	}
}

void ToggleBorder(HWND hWndForeground)
{
	long ws = GetWindowLong(hWndForeground, GWL_STYLE);
	//long es = GetWindowLong(hWndForeground, GWL_EXSTYLE);
	long wsNew = ws & (~(WS_CAPTION | WS_BORDER | WS_SYSMENU | WS_THICKFRAME));

	//AdjustWindowRectEx(&WindowRect, dwStyle, FALSE, dwExStyle);

	if (ws & WS_CAPTION)
	{
		SetPropW(hWndForeground, TOPIT_ORIG_WS, (HANDLE)ws);
		SetWindowLong(hWndForeground, GWL_STYLE, wsNew);
	}
	else
	{
		HANDLE hOrigWs = GetPropW(hWndForeground, TOPIT_ORIG_WS);
		if (hOrigWs != NULL)
		{
			RemovePropW(hWndForeground, TOPIT_ORIG_WS);
			SetWindowLong(hWndForeground, GWL_STYLE, (LONG)hOrigWs);
		}
	}

	RedrawWindow(hWndForeground, NULL, NULL, RDW_ERASE | RDW_INTERNALPAINT | RDW_FRAME | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);

	InvalidateRect(hWndForeground, NULL, TRUE);

	RECT rect;
	GetWindowRect(hWndForeground, &rect);
	SetWindowPos(hWndForeground, NULL, 0, 0, rect.right - rect.left + 1, rect.bottom - rect.top + 1, SWP_NOMOVE);
	SetWindowPos(hWndForeground, NULL, 0, 0, rect.right - rect.left, rect.bottom - rect.top, SWP_NOMOVE);
}

void ExpandRight(HWND hWndTopIt, HWND hWndForeground, WPARAM wParam)
{
	SetTimer(hWndTopIt, REDRAW_TIMER, 2000, NULL);

	// If this is a new border selection
	if (AltPressed == NULL)
	{
		HighlightWindowEdge(hWndForeground, EDGE_RIGHT);
		AltPressed = EDGE_RIGHT;
	}
	// If this is a command to move left border right
	else if (AltPressed == EDGE_LEFT)
	{
		GetWindowRect(hWndForeground, &rectActive);

		// Stretch all edges to maximum sizes
		rectSmartSize.bottom = rectActive.bottom;
		rectSmartSize.top = rectActive.top;
		rectSmartSize.right = GetSystemMetrics(SM_CXFULLSCREEN);
		rectSmartSize.left = rectActive.left;

		// Adjust edges according to other windows
		EnumWindows(ReduceEdgeEnumWindowsProc, wParam);

		SetWindowPos(hWndForeground, NULL, rectSmartSize.left, rectSmartSize.top, rectSmartSize.right - rectSmartSize.left, rectSmartSize.bottom - rectSmartSize.top, NULL);
		HighlightWindowEdge(hWndForeground, EDGE_LEFT);

	}
	// If this is a command to move right border right
	else
	{
		GetWindowRect(hWndForeground, &rectActive);

		// Stretch all edges to maximum sizes
		rectSmartSize.bottom = rectActive.bottom;
		rectSmartSize.top = rectActive.top;
		rectSmartSize.right = GetSystemMetrics(SM_CXFULLSCREEN);
		rectSmartSize.left = rectActive.left;

		// Adjust edges according to other windows
		EnumWindows(ExpandEdgeEnumWindowsProc, wParam);

		SetWindowPos(hWndForeground, NULL, rectSmartSize.left, rectSmartSize.top, rectSmartSize.right - rectSmartSize.left, rectSmartSize.bottom - rectSmartSize.top, NULL);
		HighlightWindowEdge(hWndForeground, EDGE_RIGHT);
		//AltPressed = false;

	}
}

void ExpandLeft(HWND hWndTopIt, HWND hWndForeground, WPARAM wParam)
{
	SetTimer(hWndTopIt, REDRAW_TIMER, 2000, NULL);

	// If this is a new border selection
	if (AltPressed == NULL)
	{
		HighlightWindowEdge(hWndForeground, EDGE_LEFT);
		AltPressed = EDGE_LEFT;
	}
	// If this is a command to move right border left
	else if (AltPressed == EDGE_RIGHT)
	{
		GetWindowRect(hWndForeground, &rectActive);

		// Stretch all edges to maximum sizes
		rectSmartSize.bottom = rectActive.bottom;
		rectSmartSize.top = rectActive.top;
		rectSmartSize.right = GetSystemMetrics(SM_CXFULLSCREEN);
		rectSmartSize.left = rectActive.left;

		// Adjust edges according to other windows
		EnumWindows(ReduceEdgeEnumWindowsProc, wParam);

		SetWindowPos(hWndForeground, NULL, rectSmartSize.left, rectSmartSize.top, rectSmartSize.right - rectSmartSize.left, rectSmartSize.bottom - rectSmartSize.top, NULL);
		HighlightWindowEdge(hWndForeground, EDGE_RIGHT);

	}
	// If this is a command to move left border left
	else
	{

		GetWindowRect(hWndForeground, &rectActive);

		rectSmartSize.bottom = rectActive.bottom;
		rectSmartSize.top = rectActive.top;
		rectSmartSize.right = rectActive.right;
		rectSmartSize.left = 0;

		//RECT rect;
		EnumWindows(ExpandEdgeEnumWindowsProc, wParam);

		SetWindowPos(hWndForeground, NULL, rectSmartSize.left, rectSmartSize.top, rectSmartSize.right - rectSmartSize.left, rectSmartSize.bottom - rectSmartSize.top, NULL);

		HighlightWindowEdge(hWndForeground, EDGE_LEFT);
		//AltPressed = false;

	}
}

void ExpandUp(HWND hWndTopIt, HWND hWndForeground, WPARAM wParam)
{
	GetWindowRect(hWndForeground, &rectActive);

	rectSmartSize.bottom = rectActive.bottom;
	rectSmartSize.top = 0;
	rectSmartSize.right = rectActive.right;
	rectSmartSize.left = rectActive.left;

	EnumWindows(ExpandEdgeEnumWindowsProc, wParam);

	SetWindowPos(hWndForeground, NULL, rectSmartSize.left, rectSmartSize.top, rectSmartSize.right - rectSmartSize.left, rectSmartSize.bottom - rectSmartSize.top, NULL);

	/*
	RECT rect;
	GetWindowRect(hwndActive, &rect);
	SetWindowPos(hwndActive,NULL,rect.left,rect.top - 10,rect.right-rect.left,rect.bottom-rect.top, SWP_NOSIZE);
	break;
	*/
}

void ExpandDown(HWND hWndTopIt, HWND hWndForeground, WPARAM wParam)
{
	GetWindowRect(hWndForeground, &rectActive);

	rectSmartSize.bottom = GetSystemMetrics(SM_CYMAXIMIZED); //SM_CYFULLSCREEN
	rectSmartSize.top = rectActive.top;
	rectSmartSize.right = rectActive.right;
	rectSmartSize.left = rectActive.left;

	EnumWindows(ExpandEdgeEnumWindowsProc, wParam);

	SetWindowPos(hWndForeground, NULL, rectSmartSize.left, rectSmartSize.top, rectSmartSize.right - rectSmartSize.left, rectSmartSize.bottom - rectSmartSize.top, NULL);


	/*
	RECT rect;
	GetWindowRect(hwndActive, &rect);
	SetWindowPos(hwndActive,NULL,rect.left,rect.top+10,rect.right-rect.left,rect.bottom-rect.top, SWP_NOSIZE);
	break;
	*/

}

void ExpandSmart(HWND hWndTopIt, HWND hWndForeground, WPARAM wParam)
{

	if (IsResizable(hWndForeground)){

		GetWindowRect(hWndForeground, &rectActive);

		rectSmartSize.bottom = GetSystemMetrics(SM_CYMAXIMIZED);
		rectSmartSize.top = 0;
		rectSmartSize.right = GetSystemMetrics(SM_CXMAXIMIZED);
		rectSmartSize.left = 0;

		EnumWindows(ExpandEdgeEnumWindowsProc, hotKeyAtomMoveDown);
		EnumWindows(ExpandEdgeEnumWindowsProc, hotKeyAtomMoveUp);
		EnumWindows(ExpandEdgeEnumWindowsProc, hotKeyAtomMoveLeft);
		EnumWindows(ExpandEdgeEnumWindowsProc, hotKeyAtomMoveRight);

		SetWindowPos(hWndForeground, NULL, rectSmartSize.left, rectSmartSize.top, rectSmartSize.right - rectSmartSize.left, rectSmartSize.bottom - rectSmartSize.top, NULL);
	}
}

void Hotkey(HWND hWndTopIt, HWND hWndForeground, WPARAM wParam)
{
	if (wParam == hotKeyAtomOnTop)
		ToggleZOrder(hWndForeground);
	else if (wParam == hotKeyAtomHide)
		ToggleVisibility(hWndForeground);
	else if (wParam == hotKeyAtomAlphaMinus)
		IncrementOpacity(hWndTopIt, hWndForeground);
	else if (wParam == hotKeyAtomAlphaPlus)
		DecrementOpacity(hWndTopIt, hWndForeground);
	else if (wParam == hotKeyAtomNoBorder)
		ToggleBorder(hWndForeground);
	else if (wParam == hotKeyAtomMoveRight)
		ExpandRight(hWndTopIt, hWndForeground, wParam);
	else if (wParam == hotKeyAtomMoveLeft)
		ExpandLeft(hWndTopIt, hWndForeground, wParam);
	else if (wParam == hotKeyAtomMoveUp)
		ExpandUp(hWndTopIt, hWndForeground, wParam);
	else if (wParam == hotKeyAtomMoveDown)
		ExpandDown(hWndTopIt, hWndForeground, wParam);
	else if (wParam == hotKeyAtomMoveRightUp)
	{
		ExpandUp(hWndTopIt, hWndForeground, wParam);
		ExpandRight(hWndTopIt, hWndForeground, wParam);
	}
	else if (wParam == hotKeyAtomMoveLeftUp)
	{
		ExpandUp(hWndTopIt, hWndForeground, wParam);
		ExpandLeft(hWndTopIt, hWndForeground, wParam);
	}
	else if (wParam == hotKeyAtomMoveRightDown)
	{
		ExpandDown(hWndTopIt, hWndForeground, wParam);
		ExpandRight(hWndTopIt, hWndForeground, wParam);
	}
	else if (wParam == hotKeyAtomMoveLeftDown)
	{
		ExpandDown(hWndTopIt, hWndForeground, wParam);
		ExpandLeft(hWndTopIt, hWndForeground, wParam);
	}
	else if (wParam == hotKeyAtomSmartSize)
		ExpandSmart(hWndTopIt, hWndForeground, wParam);
}

void Timer(HWND hWnd, HWND hWndForeground, WPARAM wParam)
{
	if (wParam == ALPHA_TIMER)
	{
		if (g_hWndAlphaTimer != NULL){
			
			HANDLE hOrigTitle = GetPropW(hWndForeground, TOPIT_ORIG_TITLE);
			if (hOrigTitle != NULL)
			{
				RemovePropW(hWndForeground, TOPIT_ORIG_TITLE);
				SetWindowTextW(hWndForeground, (LPCWSTR)hOrigTitle);
				free(hOrigTitle);
			}

			KillTimer(hWnd, ALPHA_TIMER);
			g_hWndAlphaTimer = NULL;
		}
	}
	else if (wParam == REDRAW_TIMER)
	{
		//if(GetAsyncKeyState( VK_MENU )>0)
		{
			if (AltPressed != NULL)
			{
				//MessageBox(NULL,"ALT NOT PRESSED",NULL, NULL);
				AltPressed = NULL;
				RedrawWindow(hWndForeground, NULL, NULL, RDW_ERASE | RDW_INTERNALPAINT | RDW_FRAME | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
				//RedrawWindow( hwndActive, NULL, NULL, RDW_ALLCHILDREN | RDW_INVALIDATE );
				KillTimer(hWnd, REDRAW_TIMER);
			}
		}
	}
}

void Create(HWND hWnd)
{
	g_hRectanglePen = CreatePen(PS_SOLID, 3, RGB(255, 0, 0));

	hotKeyAtomOnTop = GlobalAddAtom("HotKeyOnTop");
	hotKeyAtomHide = GlobalAddAtom("hotKeyAtomHide");
	hotKeyAtomNoBorder = GlobalAddAtom("HotKeyNoBorder");
	hotKeyAtomMoveRight = GlobalAddAtom("HotKeyMoveRight");
	hotKeyAtomMoveLeft = GlobalAddAtom("HotKeyMoveLeft");
	hotKeyAtomMoveUp = GlobalAddAtom("HotKeyMoveUp");
	hotKeyAtomMoveDown = GlobalAddAtom("HotKeyMoveDown");
	hotKeyAtomMoveRightUp = GlobalAddAtom("HotKeyMoveRightUp");
	hotKeyAtomMoveLeftUp = GlobalAddAtom("HotKeyMoveLeftUp");
	hotKeyAtomMoveRightDown = GlobalAddAtom("HotKeyMoveRightDown");
	hotKeyAtomMoveLeftDown = GlobalAddAtom("HotKeyMoveLeftDown");
	hotKeyAtomSmartSize = GlobalAddAtom("HotKeySmartSize");
	hotKeyAtomAlphaMinus = GlobalAddAtom("HotKeyAlphaMinus");
	hotKeyAtomAlphaPlus = GlobalAddAtom("HotKeyAlphaPlus");

	RegisterHotKey(hWnd, hotKeyAtomOnTop, MOD_ALT, 0x5A);				// Z
	RegisterHotKey(hWnd, hotKeyAtomHide, MOD_ALT, 0x58);				// X 
	RegisterHotKey(hWnd, hotKeyAtomNoBorder, MOD_ALT, 0x43);			// C 
	RegisterHotKey(hWnd, hotKeyAtomMoveRight, MOD_ALT, VK_NUMPAD6);		// 6 
	RegisterHotKey(hWnd, hotKeyAtomMoveLeft, MOD_ALT, VK_NUMPAD4);		// 4
	RegisterHotKey(hWnd, hotKeyAtomMoveUp, MOD_ALT, VK_NUMPAD8);		// 8 
	RegisterHotKey(hWnd, hotKeyAtomMoveDown, MOD_ALT, VK_NUMPAD2);		// 2 
	RegisterHotKey(hWnd, hotKeyAtomMoveRightUp, MOD_ALT, VK_NUMPAD9);	// 9 
	RegisterHotKey(hWnd, hotKeyAtomMoveLeftUp, MOD_ALT, VK_NUMPAD7);	// 7 
	RegisterHotKey(hWnd, hotKeyAtomMoveRightDown, MOD_ALT, VK_NUMPAD3);	// 3 
	RegisterHotKey(hWnd, hotKeyAtomMoveLeftDown, MOD_ALT, VK_NUMPAD1);	// 1 
	RegisterHotKey(hWnd, hotKeyAtomSmartSize, MOD_ALT, VK_NUMPAD5);		// 5
	RegisterHotKey(hWnd, hotKeyAtomAlphaMinus, MOD_ALT, VK_SUBTRACT);	// -
	RegisterHotKey(hWnd, hotKeyAtomAlphaPlus, MOD_ALT, VK_ADD);		// +

	//res = RegisterHotKey,hWnd,0xB123,MOD_CONTROL | MOD_ALT, MapVirtualKey(0x5A,0); //; CTRL + ALT + A (041h is 65 - 065h is 101)
}

void CleanUp(HWND hWnd)
{
	if (g_hWndHidden != NULL)
	{
		ShowWindow(g_hWndHidden, SW_SHOW);
		SetForegroundWindow(g_hWndHidden);
		g_hWndHidden = NULL;
	}

	UnregisterHotKey, hWnd, hotKeyAtomOnTop;
	UnregisterHotKey, hWnd, hotKeyAtomNoBorder;
	UnregisterHotKey, hWnd, hotKeyAtomHide;
	UnregisterHotKey, hWnd, hotKeyAtomMoveRight;
	UnregisterHotKey, hWnd, hotKeyAtomMoveLeft;
	UnregisterHotKey, hWnd, hotKeyAtomMoveUp;
	UnregisterHotKey, hWnd, hotKeyAtomMoveDown;
	UnregisterHotKey, hWnd, hotKeyAtomMoveRightUp;
	UnregisterHotKey, hWnd, hotKeyAtomMoveLeftUp;
	UnregisterHotKey, hWnd, hotKeyAtomMoveRightDown;
	UnregisterHotKey, hWnd, hotKeyAtomMoveLeftDown;
	UnregisterHotKey, hWnd, hotKeyAtomSmartSize;
	UnregisterHotKey, hWnd, hotKeyAtomAlphaMinus;

	//DeletePen(g_hRectanglePen);
	DeleteObject((HGDIOBJ)(HPEN)g_hRectanglePen);

	//TODO: Iterate through windows and look for the props we've added and clean up
	//RemovePropW(hWnd, TOPIT_ORIG_TITLE);
	//free(hOrigTitle);
}


// Win32 core callbacks

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	HWND hWndForeground = GetForegroundWindow();

	switch (message)
	{
	case WM_CREATE:
		Create(hWnd);
		break;

	case WM_HOTKEY:
		Hotkey(hWnd, hWndForeground, wParam);
		break;
	case WM_TIMER:
		Timer(hWnd, hWndForeground, wParam);
		break;
	case WM_DESTROY:
		CleanUp(hWnd);

		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

// This function and its usage are only necessary if you want this code
// to be compatible with Win32 systems prior to the 'RegisterClassEx'
// function that was added to Windows 95. It is important to call this function
// so that the application will get 'well formed' small icons associated
// with it.
ATOM Register(HINSTANCE hInstance)
{
	WNDCLASSEXW wcex;
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = (WNDPROC)WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, (LPCTSTR)IDI_TOPIT);
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = (LPCWSTR)IDC_TOPIT;
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, (LPCTSTR)IDI_SMALL);

	return RegisterClassExW(&wcex);
}


int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
{
	LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadStringW(hInstance, IDC_TOPIT, szWindowClass, MAX_LOADSTRING);
	Register(hInstance);

	hUserDll = LoadLibrary("USER32.dll");

	g_hInstance = hInstance;
	g_hWndTopIt = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);

	// If we have a window handle, treat as non-zero boolean type signaling success
	if (!g_hWndTopIt)
	{
		return FALSE;
	}

	HACCEL hAccelTable = LoadAccelerators(hInstance, (LPCTSTR)IDC_TOPIT);

	// Main message loop
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) 
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) 
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return (int) msg.wParam;
}

















