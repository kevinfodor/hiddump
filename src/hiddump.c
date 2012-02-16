/*
 ==============================================================================
 Name        : hiddump.c
 Date        : Aug 15, 2011
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

// WDDK includes
#include <usbioctl.h>
#include <hidusage.h>
#include <hidpi.h>
#include <hidsdi.h>

// Other includes
#include "utils.h"
#include "output.h"
#include "win_msg_hdlr.h"
#include "usb_defs.h"
#include "usb_hid.h"
#include "usb_debug.h"
#include "usb_hid_msg_hdlr.h"

// Module include
#include "hiddump.h"

#define LINE_WIDTH      (80)

// Local declarations

// Global declarations
cmd_line_params_t g_cmd_line_params =
{ 0, 0, false, false };

// Implementation
int hid_dump(void)
{
	char * p_device_path;
	hid_device_t hid_device;
	bool success;

	// Before we do anything, let's enumerate the entire USB chain.
	// This will give us an overview of what the host has.
	if (true == g_cmd_line_params.enumerate)
	{
		usb_print_enumeration(g_cmd_line_params.show_descriptors);
	}

	LINE(LINE_WIDTH, '-', true);

	// Simple initialization of the target HID
	memset(&hid_device, 0, sizeof(hid_device));

	// Find the HID path for this specified device
	success = usb_get_hid_device_path(g_cmd_line_params.vid,
			g_cmd_line_params.pid, &p_device_path);
	if (!success)
	{
		fprintf(stderr, "Cannot find HID with VID=0x%0x, PID=0x%0x\n",
				g_cmd_line_params.vid, g_cmd_line_params.pid);
	}
	else
	{
		// We need only read(overlapped) access
		usb_open_options_t options = USB_READ_ACCESS | USB_OVERLAPPED;

		success = usb_open_hid(p_device_path, options, &hid_device);
		if (!success)
		{
			fprintf(stderr, "Cannot open HID with device path '%s'\n",
					p_device_path);
		}
		else
		{
			hid_handler_context_t hid_handler;

			// Load HID handler
			hid_handler.h_device_notify = NULL;
			hid_handler.h_reader = NULL;
			hid_handler.p_hid_device = &hid_device;

			// Print our HID information
			usb_print_hid_device(&hid_device);

			// Check if we should run the real-time HID parser
			if (true == g_cmd_line_params.run_parser)
			{
				// Start message handler (never returns)
				win_msg_hdlr_start(g_cmd_line_params.hInstance, hid_msg_hdlr,
						&hid_handler);
			}

			// We are now done with the HID
			usb_close_hid(&hid_device);
		}
	}

	LINE(LINE_WIDTH, '-', true);

	return EXIT_SUCCESS;
}
