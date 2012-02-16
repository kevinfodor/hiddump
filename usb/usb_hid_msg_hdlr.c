/*
 ==============================================================================
 Name        : usb_hid_winmsg_hdlr.c
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
#include <dbt.h>

// Other includes
#include "output.h"
#include "hexdump.h"
#include "utils.h"
#include "usb_defs.h"
#include "usb_hid.h"
#include "usb_debug.h"
#include "usb_hid_reader.h"
#include "win_msg_hdlr.h"
#include "win_device_notification.h"

// Module include
#include "usb_hid_msg_hdlr.h"

// Implementation
bool hid_msg_hdlr(p_win_proc_msg_context_t p_context)
{
	bool msg_handled = TRUE;
	p_winapi_proc_args_t p_args = p_context->p_winapi_proc_args;
	p_hid_handler_context_t p_hid =
			(p_hid_handler_context_t) p_context->p_callback_arg;

	switch (p_args->message)
	{
	case WM_DISPLAY_READ_DATA:
	{
		static char buffer[1024];
		phid_data_t p_hid_data;
		size_t loop;
		phid_device_t p_hid_device;

		Message("WM_DISPLAY_READ_DATA", p_context->msg_count);

		//
		// LParam is the device that was read from
		//

		p_hid_device = (phid_device_t) p_args->lParam;

		//
		// Display all the data stored in the Input data field for the device
		//

//		if ((pParams->msg_count % 50) == 0)
		{
			phid_report_t p_report =
					&p_hid_device->report[HID_REPORT_TYPE_INPUT];

			p_hid_data = p_report->p_hid_data;

			hex_dump(stdout, NULL, (uint8_t const *) p_report->p_report_buffer,
					p_report->report_buffer_length);

			for (loop = 0; loop < p_report->hid_data_length; loop++)
			{
				usb_print_hid_report(p_hid_data, buffer, sizeof(buffer));
				printf("::\t%s\n", buffer);
				p_hid_data++;
			}
		}
		usb_hid_read_handled(p_hid->h_reader);

	}

		break;
	case WM_READ_THREAD_TERMINATED:
		Message("WM_READ_THREAD_TERMINATED", p_context->msg_count);
		usb_hid_destroy_reader(p_hid->h_reader);
		break;

	case WM_CREATE:
	{
		GUID guid;
		bool success;

		Message("WM_CREATE", p_context->msg_count);

		// Register for HID device notifications
		HidD_GetHidGuid(&guid);

		success = register_device_notifications(guid, p_args->hWnd,
				&p_hid->h_device_notify);
		if (!success)
		{
			// Terminate on failure.
			print_err(-1, "register_device_notifications");
			ExitProcess(1);
		}
		printf("Registered for HID-USB device notifications.\n");

		// Create reader thread
		p_hid->h_reader = usb_hid_create_reader(p_args->hWnd,
				p_hid->p_hid_device);
	}
		break;

	case WM_DEVICECHANGE:
		Message("WM_DEVICECHANGE", p_context->msg_count);

		{
			PDEV_BROADCAST_DEVICEINTERFACE lpdb =
					(PDEV_BROADCAST_DEVICEINTERFACE) p_args->lParam;

			if (DBT_DEVTYP_DEVICEINTERFACE == lpdb->dbcc_devicetype)
			{
				PDEV_BROADCAST_DEVICEINTERFACE lpdbi =
						(PDEV_BROADCAST_DEVICEINTERFACE) p_args->lParam;
				printf("Device Change: '%s'\n", lpdbi->dbcc_name);
			}
		}
		// Output some messages to the window.
		switch (p_args->wParam)
		{
		case DBT_DEVICEARRIVAL:
			Message("DBT_DEVICEARRIVAL", p_context->msg_count);
			break;
		case DBT_DEVICEREMOVECOMPLETE:
			Message("DBT_DEVICEREMOVECOMPLETE", p_context->msg_count);
			break;
		case DBT_DEVNODES_CHANGED:
			Message("DBT_DEVNODES_CHANGED", p_context->msg_count);
			break;
		default:
			Message("DBT_ default", p_context->msg_count);
			break;
		}
		break;

	case WM_CLOSE:
	{
		BOOL success;
		Message("WM_CLOSE", p_context->msg_count);
		usb_hid_stop_reader(p_hid->h_reader);
		success = UnregisterDeviceNotification(p_hid->h_device_notify);
		if (!success)
		{
			print_err(success, "UnregisterDeviceNotification");
		}
	}
		break;
	case WM_DESTROY:
		Message("WM_DESTROY", p_context->msg_count);
		break;

	default:
		msg_handled = FALSE;
		break;
	}

	return msg_handled;
}
