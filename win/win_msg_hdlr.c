/*
 ==============================================================================
 Name        : win_msg_hdlr.c
 Date        : Aug 26, 2011
 ==============================================================================

 BSD License
 -----------

 Copyright (c) 2011, and Kevin Fodor, All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 - Redistributions of source code must retain the above copyright notice,
 this list of conditions and the following disclaimer.

 - Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation
 and/or other materials provided with the distribution.

 - Neither the name of Kevin Fodor nor the names of
 its contributors may be used to endorse or promote products derived from
 this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE.

 NOTICE:
 SOME OF THIS CODE MAY HAVE ELEMENTS TAKEN FROM OTHER CODE WITHOUT ATTRIBUTION.
 IF THIS IS THE CASE IT WAS DUE TO OVERSIGHT WHILE DEBUGGING AND I APOLOGIZE.
 IF ANYONE HAS ANY REASON TO BELIEVE THAT ANY OF THIS CODE VIOLATES OTHER
 LICENSES PLEASE CONTACT ME WITH DETAILS SO THAT I MAY CORRECT THE SITUATION.

 ==============================================================================
 */

// Standard includes
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

// Windows includes
#include <windows.h>

// Other includes
#include "utils.h"
#include "output.h"

// Project includes
#include "win_msg_hdlr.h"

#define WINDOWLESS_CLASS_NAME TEXT("WindowlessClass")

typedef struct _msg_handler_context_t
{
	// A user defined message handler callback
	msg_handler_callback_t callback;

	// Parameters which may be accessed by the callback
	win_proc_msg_context_t params;

} msg_handler_context_t, *p_msg_handler_context_t;

// Local declarations

static void msg_pump(HWND hWnd);

static INT_PTR WINAPI win_proc_callback(HWND hWnd, UINT message, WPARAM wParam,
		LPARAM lParam);

static bool default_msg_handler_callback(p_win_proc_msg_context_t p_context);

// Define a default handler structure to use
static msg_handler_context_t default_handler =
{ default_msg_handler_callback,
{ NULL, 0, NULL } };

// Implementation

static void msg_pump(HWND hWnd)
{
	MSG msg;
	BOOL result;

	/*
	 Get all messages for any window that belongs to this thread,
	 without any filtering. Potential optimization could be
	 obtained via use of filter values if desired.
	 */

	while ((result = GetMessage(&msg, NULL, 0, 0)) != 0)
	{
		if (result == -1)
		{
			print_errno("GetMessage");
			break;
		}
		else
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
}

/*

 Routine Description:
 Simple Windows callback for handling messages.
 This is where all the work is done because the example
 is using a window to process messages. This logic would be handled
 differently if registering a service instead of a window.

 Parameters:
 hWnd - the window handle being registered for events.

 message - the message being interpreted.

 wParam and lParam - extended information provided to this
 callback by the message sender.

 For more information regarding these parameters and return value,
 see the documentation for WNDCLASS and CreateWindow.

 */

static INT_PTR WINAPI win_proc_callback(HWND hWnd, UINT message, WPARAM wParam,
		LPARAM lParam)
{
	bool b_msg_handled;
	LRESULT result = 1;
	winapi_proc_args_t args;
	static p_msg_handler_context_t p_context = &default_handler;

	// Increment event counter
	p_context->params.msg_count++;

	// Load in arguments
	args.hWnd = hWnd;
	args.message = message;
	args.wParam = wParam;
	args.lParam = lParam;
	p_context->params.p_winapi_proc_args = &args;

	// Handle message...
	switch (message)
	{
	case WM_CREATE:

		// Initialize context, switch to real context
		p_context =
				(p_msg_handler_context_t) (((LPCREATESTRUCT) lParam)->lpCreateParams);

		if (NULL == p_context)
		{
			// Terminate on failure. No context we can't continue
			print_errno("WM_CREATE");
			ExitProcess(1);
		}

		// Initialize message counter
		p_context->params.msg_count = 1;

		// Initialize args
		p_context->params.p_winapi_proc_args = &args;

		// Call notification handler
		p_context->callback(&p_context->params);
		break;

	case WM_CLOSE:
		// Call notification handler
		p_context->callback(&p_context->params);
		DestroyWindow(hWnd);
		break;

	case WM_DESTROY:
		// Call notification handler
		p_context->callback(&p_context->params);
		PostQuitMessage(0);
		break;

	default:
		// Call notification, check if event handled
		b_msg_handled = p_context->callback(&p_context->params);
		if (b_msg_handled == FALSE)
		{
			// Send all other messages on to the default windows handler.
			result = DefWindowProc(hWnd, message, wParam, lParam);
		}
		break;
	}

	return result;
}

static bool default_msg_handler_callback(p_win_proc_msg_context_t p_context)
{
	// Dummy. Tell message handler we handled nothing.
	return FALSE;
}

void win_msg_hdlr_start(HINSTANCE hInstance, msg_handler_callback_t callback,
		void *p_callback_arg)
{
	HWND hWnd;
	msg_handler_context_t context;
	ATOM result;
	WNDCLASS wnd_class =
	{ };

	// Load parameters
	if (callback == NULL)
	{
		context.callback = default_msg_handler_callback;
	}
	else
	{
		context.callback = callback;
	}

	// Initialize caller inputs
	context.params.p_callback_arg = p_callback_arg;

	// Initialize msg handler elements
	context.params.msg_count = 0;

	// Initialize pointer to p_winapi_proc_args
	context.params.p_winapi_proc_args = NULL;

	// Define a simple window-less class
	wnd_class.hInstance = (HINSTANCE) (GetModuleHandle(0));
	wnd_class.lpfnWndProc = (WNDPROC) (win_proc_callback);
	wnd_class.lpszClassName = WINDOWLESS_CLASS_NAME;

	// Register this class
	result = RegisterClass(&wnd_class);
	if (!result)
	{
		print_err(result, "RegisterClass");
		return;
	}

	// Create a Message Only "Window"
	hWnd = CreateWindow(
			WINDOWLESS_CLASS_NAME,
			NULL,
			0,
			0, 0,
			0, 0,
			HWND_MESSAGE, NULL,
			hInstance,
			(LPVOID)&context
	);

	if (NULL == hWnd)
	{
		print_errno("CreateWindow");
		return;
	}

	// The message pump loops until the window is destroyed.
	msg_pump(hWnd);

	return;
}

