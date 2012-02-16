/*
 ==============================================================================
 Name        : usb_hid_reader.c
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
#include <hidsdi.h>

// Other includes
#include "output.h"
#include "utils.h"
#include "usb_defs.h"
#include "usb_hid.h"
#include "usb_debug.h"
#include "usb_hid_reports.h"

// Module include
#include "usb_hid_reader.h"

#define READ_THREAD_TIMEOUT_MSEC     1000

typedef struct _usb_hid_reader_thread_context_t
{
	// Pointer to the HID device we are working with
	phid_device_t p_hid_device;

	// Handle to the message handler
	HWND h_msg_handler_wnd;

	// Unpacked report ready event
	HANDLE h_unpacked_report_ready;

	// Flag to indicate reader should terminate
	bool terminate_thread;

	// Thread handle
	HANDLE h_reader_thread;

	// Thread id
	DWORD thread_id;

} usb_hid_reader_thread_context_t, *pusb_hid_reader_thread_context_t;

// Local declarations

static DWORD WINAPI asynch_read_thread_proc(
		pusb_hid_reader_thread_context_t p_context);

static DWORD WINAPI
asynch_read_thread_proc(pusb_hid_reader_thread_context_t p_context)
{
	HANDLE h_event_object;

	//
	// Create the completion event to send to the the OverlappedRead routine
	//

	h_event_object = CreateEvent(NULL, FALSE, FALSE, NULL);

	//
	// If NULL returned, then we cannot proceed any farther so we just exit the
	//  the thread
	//

	if (NULL == h_event_object)
	{
		goto AsyncRead_End;
	}

	//
	// Now we enter the main read loop, which does the following:
	//  1) Calls ReadOverlapped()
	//  2) Waits for read completion with a timeout just to check if
	//      the main thread wants us to terminate our the read request
	//  3) If the read fails, we simply break out of the loop
	//      and exit the thread
	//  4) If the read succeeds, we call UnpackReport to get the relevant
	//      info and then post a message to main thread to indicate that
	//      there is new data to display.
	//  5) We then block on the display event until the main thread says
	//      it has properly displayed the new data
	//  6) Look to repeat this loop if we are doing more than one read
	//      and the main thread has yet to want us to terminate
	//

	do
	{
		bool read_status;
		DWORD wait_status;

		//
		// Call ReadOverlapped() and if the return status is TRUE, the ReadFile
		//  succeeded so we need to block on completionEvent, otherwise, we just
		//  exit
		//

		read_status = hid_read_overlapped(p_context->p_hid_device,
				h_event_object);

		if (!read_status)
		{
			break;
		}

		//
		// Wait for the completion event to be signaled or a timeout
		//

		wait_status = WaitForSingleObject(h_event_object,
				READ_THREAD_TIMEOUT_MSEC);

		//
		// If completionEvent was signaled, then a read just completed
		//   so let's leave this loop and process the data
		//

		switch (wait_status)
		{
		case WAIT_OBJECT_0:
		{
			phid_report_t p_report =
					&p_context->p_hid_device->report[HID_REPORT_TYPE_INPUT];

			// Success, use the report data
			// Unpack Input Report from device. InputReportBuffer gets
			// unpacked into various HID_DATA structures
			hid_unpack_report(p_report->p_report_buffer, // Raw-data
					p_report->report_buffer_length, // Raw-data len
					HidP_Input, // Input report
					p_report->p_hid_data, // HID-Data array
					p_report->hid_data_length, // HID-Data array(elements)
					p_context->p_hid_device->p_ppd);

			if (NULL != p_context->h_unpacked_report_ready)
			{
				PostMessage(p_context->h_msg_handler_wnd, WM_DISPLAY_READ_DATA,
						0, (LPARAM) p_context->p_hid_device);

				WaitForSingleObject(p_context->h_unpacked_report_ready,
						INFINITE);
			}
			break;
		}
		case WAIT_TIMEOUT:
			// Timeout error
			// Cancel read operation.

			CancelIo(p_context->p_hid_device->h_device);
			break;
		default:
			// Undefined error
			// Cancel read operation.

			CancelIo(p_context->p_hid_device->h_device);
			break;

		}
	} while (!p_context->terminate_thread);

	CloseHandle(h_event_object);

	AsyncRead_End:

	PostMessage(p_context->h_msg_handler_wnd, WM_READ_THREAD_TERMINATED, 0, 0);
	ExitThread(0);
	return (0);
}

HANDLE usb_hid_create_reader(HWND h_wnd, phid_device_t p_hid_device)
{
	pusb_hid_reader_thread_context_t p_context;

	if (NULL == p_hid_device)
	{
		return NULL;
	}

	// Create context memory
	p_context = (pusb_hid_reader_thread_context_t) malloc(
			sizeof(usb_hid_reader_thread_context_t));
	if (NULL == p_context)
	{
		return NULL;
	}

	p_context->h_unpacked_report_ready = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (NULL == p_context->h_unpacked_report_ready)
	{
		free(p_context);
		return NULL;
	}

	//
	// For asynchronous read, default to using the same information
	//    passed in as the lParam.  This is because data related to
	//    Ppd and such cannot be retrieved using the standard HidD_
	//    functions.  However, it is necessary to parse future reports.
	//

	if (NULL == p_hid_device)
	{
		print_errno("OpenHidDevice");
		CloseHandle(p_context->h_unpacked_report_ready);
		free(p_context);
		return NULL;
	}

	//
	// Start a new read thread
	//

	p_context->p_hid_device = p_hid_device;
	p_context->terminate_thread = FALSE;
	p_context->h_msg_handler_wnd = h_wnd;

	p_context->h_reader_thread = CreateThread(NULL, 0,
			(LPTHREAD_START_ROUTINE) asynch_read_thread_proc, p_context, 0,
			&p_context->thread_id);

	if (NULL == p_context->h_reader_thread)
	{
		print_errno("Unable to create read thread");
		CloseHandle(p_context->h_unpacked_report_ready);
		free(p_context);
		return NULL;
	}

	return (HANDLE) p_context;
}

void usb_hid_stop_reader(HANDLE h_reader)
{
	pusb_hid_reader_thread_context_t p_context =
			(pusb_hid_reader_thread_context_t) h_reader;

	if (NULL == p_context)
	{
		return;
	}

	p_context->terminate_thread = TRUE;

	return;
}

void usb_hid_destroy_reader(HANDLE h_reader)
{
	pusb_hid_reader_thread_context_t p_context =
			(pusb_hid_reader_thread_context_t) h_reader;

	if (NULL == p_context)
	{
		return;
	}

	CloseHandle(p_context->h_unpacked_report_ready);

	free(p_context);

	return;
}

void usb_hid_read_handled(HANDLE h_reader)
{
	pusb_hid_reader_thread_context_t p_context =
			(pusb_hid_reader_thread_context_t) h_reader;

	if (NULL == p_context)
	{
		return;
	}

	SetEvent(p_context->h_unpacked_report_ready);

	return;
}
