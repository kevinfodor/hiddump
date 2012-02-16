/*
 ==============================================================================
 Name        : win_msg_hdlr.h
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

#ifndef WIN_MSG_HDLR_H_
#define WIN_MSG_HDLR_H_

#ifdef __cplusplus
extern "C"
{
#endif

/*!
 \brief

 */

typedef struct _winapi_proc_args_t
{
	// Windows notification arguments. This is a collection
	// of arguments the WINAPI uses when handling messages
	HWND hWnd;
	UINT message;
	WPARAM wParam;
	LPARAM lParam;

} winapi_proc_args_t, *p_winapi_proc_args_t;

typedef struct _win_proc_msg_context
{
	// User defined message handler arg
	void * p_callback_arg;

	// Message counter
	size_t msg_count;

	// Window Processing Args
	p_winapi_proc_args_t p_winapi_proc_args;

} win_proc_msg_context_t, *p_win_proc_msg_context_t;

// Message handler callback
typedef bool(*msg_handler_callback_t)(p_win_proc_msg_context_t p_contect);

void win_msg_hdlr_start(HINSTANCE hInstance, msg_handler_callback_t callback,
		void *p_callback_arg);

#ifdef __cplusplus
}
#endif

#endif /* WIN_MSG_HDLR_H_ */
