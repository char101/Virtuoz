#include "stdafx.h"
#include "SetWindowPosTimeout.h"

namespace
{
	struct SET_WINDOW_POS_TIMEOUT_PARAM
	{
		HWND hWnd, hWndInsertAfter;
		int X, Y, cx, cy;
		UINT uFlags;
		BOOL bRet;
	};

	unsigned __stdcall SetWindowPosTimeoutThread(void *pParam)
	{
		SET_WINDOW_POS_TIMEOUT_PARAM &param = *(SET_WINDOW_POS_TIMEOUT_PARAM *)pParam;

		param.bRet = SetWindowPos(param.hWnd, param.hWndInsertAfter, param.X, param.Y, param.cx, param.cy, param.uFlags);

		_endthreadex(0);
		return 0;
	}
}

BOOL SetWindowPosTimeout(HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags, DWORD dwTimeout)
{
	SET_WINDOW_POS_TIMEOUT_PARAM param;
	param.hWnd = hWnd;
	param.hWndInsertAfter = hWndInsertAfter;
	param.X = X;
	param.Y = Y;
	param.cx = cx;
	param.cy = cy;
	param.uFlags = uFlags;
	param.bRet = FALSE;

	HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, SetWindowPosTimeoutThread, &param, 0, NULL);
	if(!hThread)
	{
		switch(errno)
		{
		case EAGAIN:
			SetLastError(ERROR_TOO_MANY_THREADS);
			break;

		case EACCES:
			SetLastError(ERROR_NOT_ENOUGH_MEMORY);
			break;

		case EINVAL:
		default: // should not get to default:, but just in case
			SetLastError(ERROR_INVALID_PARAMETER);
			break;
		}

		return FALSE;
	}

	BOOL bSucceeded = TRUE;
	DWORD dwError = ERROR_SUCCESS;

	DWORD dwWait = WaitForSingleObject(hThread, dwTimeout);
	if(dwWait != WAIT_OBJECT_0)
	{
		bSucceeded = FALSE;

		if(dwWait == WAIT_FAILED)
		{
			dwError = GetLastError();
		}
		else if(dwWait == WAIT_TIMEOUT)
		{
			dwError = ERROR_TIMEOUT;
		}

		TerminateThread(hThread, 0);
	}

	CloseHandle(hThread);

	SetLastError(dwError);
	return bSucceeded ? param.bRet : FALSE;
}
