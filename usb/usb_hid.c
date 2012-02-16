/*
 ==============================================================================
 Name        : usb_hid.c
 Date        : Aug 17, 2011
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

 Notice: The contents of this file were created with extensive help
 from the Microsoft Windows Driver Kit (WDK) sample application "hclient"
 You can find more details about hclient at
 http://msdn.microsoft.com/en-us/library/ff538800(v=vs.85).aspx

 The HClient sample code describes how to write a user-mode client application
 to communicate with devices that conform to the HID device class specification.

 Specifically the contents of this file were derived from the PNP.c
 file in that sample project (\hclient\PNP.c). I will include the
 copyright from that file here to give credit where it is due, although
 I do not claim to know anything about any legal ramifications of deriving
 code from those provided examples.

 For "PNP.c" Copyright (c) 1996    Microsoft Corporation

 */

// Standard includes
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

// Windows includes
#include <windows.h>
#include <hidsdi.h>
#include <setupapi.h>

// Other includes
#include "utils.h"
#include "usb_defs.h"

// Module include
#include "usb_hid.h"

// Local declarations
static bool open_hid(char * const p_device_path, bool has_read_access,
		bool has_write_access, bool is_exclusive, bool is_overlapped,
		phid_device_t p_hid_device);

static bool fill_hid_info(phid_device_t p_hid_device);

// Implementation
static bool open_hid(char * const p_device_path, bool has_read_access,
		bool has_write_access, bool is_exclusive, bool is_overlapped,
		phid_device_t p_hid_device)
{
	DWORD access_flags = 0;
	DWORD sharing_flags = 0;
	bool success;
	BOOL result;

	if (NULL == p_device_path)
	{
		return (false);
	}

	if (has_read_access)
	{
		access_flags |= GENERIC_READ;
	}

	if (has_write_access)
	{
		access_flags |= GENERIC_WRITE;
	}

	if (!is_exclusive)
	{
		sharing_flags = FILE_SHARE_READ | FILE_SHARE_WRITE;
	}

	/*

	 The hid.dll api's do not pass the overlapped structure into
	 DeviceIoControl so to use them we must have a non overlapped device.
	 If the request is for an overlapped device we will close the device below
	 and get a handle to an overlapped device

	 */

	p_hid_device->h_device = CreateFile(p_device_path, access_flags,
			sharing_flags, NULL, // no SECURITY_ATTRIBUTES structure
			OPEN_EXISTING, // No special create flags
			0, // Open device as non-overlapped so we can get data
			NULL); // No template file

	if (INVALID_HANDLE_VALUE == p_hid_device->h_device)
	{
		usb_close_hid(p_hid_device);
		return (false);
	}

	/*

	 If the device was not opened as overlapped, then fill the rest of the
	 HidDevice structure.  However, if opened as overlapped, this handle cannot
	 be used the calls to the HidD_ exported functions since each of these
	 functions does synchronous I/O.

	 */

	result = HidD_GetPreparsedData(p_hid_device->h_device,
			&p_hid_device->p_ppd);
	if (!result)
	{
		usb_close_hid(p_hid_device);
		return (false);
	}

	result = HidD_GetAttributes(p_hid_device->h_device,
			&p_hid_device->attributes);
	if (!result)
	{
		usb_close_hid(p_hid_device);
		return (false);
	}

	result = HidP_GetCaps(p_hid_device->p_ppd, &p_hid_device->caps);
	if (!result)
	{
		usb_close_hid(p_hid_device);
		return (false);
	}

	/*

	 At this point the client has a choice.  It may chose to look at the
	 Usage and Page of the top level collection found the HIDP_CAPS
	 structure.  this way it could just use the usages it knows about.
	 If either HidP_GetUsages or HidP_GetUsageValue return an error then
	 that particular usage does not exist the report.
	 This is most likely the preferred method as the application can only
	 use usages of which it already knows.
	 this case the app need not even call GetButtonCaps or GetValueCaps.

	 this example, however, we will call FillDeviceInfo to look for all
	 of the usages the device.

	 */

	success = fill_hid_info(p_hid_device);
	if (!success)
	{
		usb_close_hid(p_hid_device);
		return (false);
	}

	if (is_overlapped)
	{
		CloseHandle(p_hid_device->h_device);

		p_hid_device->h_device = CreateFile(p_device_path, access_flags,
				sharing_flags, NULL, // no SECURITY_ATTRIBUTES structure
				OPEN_EXISTING, // No special create flags
				FILE_FLAG_OVERLAPPED, // Now we open the device as overlapped
				NULL); // No template file

		if (INVALID_HANDLE_VALUE == p_hid_device->h_device)
		{
			usb_close_hid(p_hid_device);
			return (false);
		}
	}

	return (true);
}

static bool fill_hid_info(phid_device_t p_hid_device)
{
	hid_report_type_t report_index;

	//
	// setup Input, Output and Feature buffers.
	//
	for (report_index = HID_REPORT_TYPE_FIRST;
			report_index < HID_REPORT_TYPE_SIZE; report_index++)
	{
		PHIDP_BUTTON_CAPS p_button_caps;
		PHIDP_VALUE_CAPS p_value_caps;
		phid_data_t p_data;
		size_t index;
		USAGE usage;
		size_t data_index;
		size_t num_values = 0;
		HIDP_REPORT_TYPE report_type;
		phid_report_t p_report = &p_hid_device->report[report_index];

		// Figure out which report type we are working on and assign
		// locals relevant to it.
		switch (report_index)
		{
		case HID_REPORT_TYPE_INPUT:
			p_report->report_buffer_length =
					p_hid_device->caps.InputReportByteLength;
			p_report->number_button_caps =
					p_hid_device->caps.NumberInputButtonCaps;
			p_report->number_value_caps =
					p_hid_device->caps.NumberInputValueCaps;
			report_type = HidP_Input;
			break;

		case HID_REPORT_TYPE_OUTPUT:
			p_report->report_buffer_length =
					p_hid_device->caps.OutputReportByteLength;
			p_report->number_button_caps =
					p_hid_device->caps.NumberOutputButtonCaps;
			p_report->number_value_caps =
					p_hid_device->caps.NumberOutputValueCaps;
			report_type = HidP_Output;
			break;

		case HID_REPORT_TYPE_FEATURE:
			p_report->report_buffer_length =
					p_hid_device->caps.FeatureReportByteLength;
			p_report->number_button_caps =
					p_hid_device->caps.NumberFeatureButtonCaps;
			p_report->number_value_caps =
					p_hid_device->caps.NumberFeatureValueCaps;
			report_type = HidP_Feature;
			break;

		default:
			break;

		}

		if (p_report->report_buffer_length > 0)
		{
			// Allocate memory to hold our reports
			p_report->p_report_buffer = (PCHAR) calloc(
					p_report->report_buffer_length, sizeof(char));
			if (NULL == p_report->p_report_buffer)
			{
				return (false);
			}
		}
		else
		{
			p_report->p_report_buffer = NULL;
		}

		// Have the HidP_GetButtonCaps function fill the capability
		// structure arrays.
		if (p_report->number_button_caps > 0)
		{
			ULONG number_button_caps = p_report->number_button_caps;
			NTSTATUS status;

			// Allocate memory to hold the button and value capabilities.
			// NumberXXCaps is terms of array elements.
			p_report->p_button_caps = p_button_caps =
					(PHIDP_BUTTON_CAPS) calloc(p_report->number_button_caps,
							sizeof(HIDP_BUTTON_CAPS));

			if (NULL == p_report->p_button_caps)
			{
				free(p_report->p_report_buffer);
				p_report->p_report_buffer = NULL;
				free(p_report->p_button_caps);
				p_report->p_button_caps = NULL;
				return (false);
			}

			status = HidP_GetButtonCaps (report_type,
					p_button_caps,
					&number_button_caps,
					p_hid_device->p_ppd);
			if (HIDP_STATUS_SUCCESS != status)
			{
				return (false);
			}

			// Verify what we asked for is what we got
			if (number_button_caps != p_report->number_button_caps)
			{
				return (false);
			}
		}
		else
		{
			p_report->p_button_caps = NULL;
		}

		// Have the HidP_GetButtonCaps function fill the capability
		// structure arrays.
		if (p_report->number_value_caps > 0)
		{
			ULONG number_value_caps = p_report->number_value_caps;
			NTSTATUS status;

			p_report->p_value_caps = p_value_caps = (PHIDP_VALUE_CAPS) calloc(
					p_report->number_value_caps, sizeof(HIDP_VALUE_CAPS));

			if (NULL == p_report->p_value_caps)
			{
				free(p_report->p_report_buffer);
				p_report->p_report_buffer = NULL;
				free(p_report->p_button_caps);
				p_report->p_button_caps = NULL;
				free(p_report->p_value_caps);
				p_report->p_value_caps = NULL;
				return (false);
			}

			status = HidP_GetValueCaps (report_type,
					p_value_caps,
					&number_value_caps,
					p_hid_device->p_ppd);
			if (HIDP_STATUS_SUCCESS != status)
			{
				return (false);
			}

			// Verify what we asked for is what we got
			if (number_value_caps != p_report->number_value_caps)
			{
				return (false);
			}
		}
		else
		{
			p_report->p_value_caps = NULL;
		}

		/*

		 Depending on the device, some value caps structures may represent more
		 than one value (a range).  In the interest of being verbose, over
		 efficient, we will expand these so that we have one and only one
		 struct hid_data_t for each value.

		 To do this we need to count up the total number of values are listed
		 the value caps structure.  For each element the array we test
		 for range if it is a range then UsageMax and UsageMin describe the
		 usages for this range INCLUSIVE.

		 */

		for (index = 0; index < p_report->number_value_caps;
				index++, p_value_caps++)
		{
			if (p_value_caps->IsRange)
			{
				num_values += p_value_caps->Range.UsageMax
						- p_value_caps->Range.UsageMin + 1;
				if (p_value_caps->Range.UsageMin
						>= p_value_caps->Range.UsageMax
								+ p_report->number_button_caps)
				{
					return (false); // overrun check
				}
			}
			else
			{
				num_values++;
			}
		}

		p_value_caps = p_report->p_value_caps;

		/*
		 Allocate a buffer to hold the struct hid_data_t structures.
		 One element for each set of buttons, and one element for each value
		 found.
		 */

		p_report->hid_data_length = p_report->number_button_caps + num_values;

		p_report->p_hid_data = p_data = (phid_data_t) calloc(
				p_report->hid_data_length, sizeof(hid_data_t));

		if (NULL == p_data)
		{
			free(p_data);
			p_report->p_hid_data = NULL;
			return (false);
		}

		// Fill the button data
		data_index = 0;
		for (index = 0; index < p_report->number_button_caps;
				index++, p_data++, p_button_caps++, data_index++)
		{
			p_data->is_button = true;
			p_data->status = HIDP_STATUS_SUCCESS;
			p_data->usage_page = p_button_caps->UsagePage;
			if (p_button_caps->IsRange)
			{
				p_data->button.usage_min = p_button_caps->Range.UsageMin;
				p_data->button.usage_max = p_button_caps->Range.UsageMax;
			}
			else
			{
				p_data->button.usage_min = p_data->button.usage_max =
						p_button_caps->NotRange.Usage;
			}

			p_data->button.max_usage_length = HidP_MaxUsageListLength(
					report_type, p_button_caps->UsagePage, p_hid_device->p_ppd);

			p_data->button.p_usages = (PUSAGE) calloc(
					p_data->button.max_usage_length, sizeof(USAGE));

			p_data->report_id = p_button_caps->ReportID;
		}

		// Fill the value data
		for (index = 0; index < p_report->number_value_caps;
				index++, p_value_caps++)
		{
			if (p_value_caps->IsRange)
			{
				for (usage = p_value_caps->Range.UsageMin;
						usage <= p_value_caps->Range.UsageMax; usage++)
				{
					if (data_index >= (p_report->hid_data_length))
					{
						return (false); // error case
					}
					p_data->is_button = false;
					p_data->status = HIDP_STATUS_SUCCESS;
					p_data->usage_page = p_value_caps->UsagePage;
					p_data->value.usage = usage;
					p_data->report_id = p_value_caps->ReportID;
					p_data++;
					data_index++;
				}
			}
			else
			{
				if (data_index >= (p_report->hid_data_length))
				{
					return (false); // error case
				}
				p_data->is_button = false;
				p_data->status = HIDP_STATUS_SUCCESS;
				p_data->usage_page = p_value_caps->UsagePage;
				p_data->value.usage = p_value_caps->NotRange.Usage;
				p_data->report_id = p_value_caps->ReportID;
				p_data++;
				data_index++;
			}
		}
	}

	return (true);
}

bool usb_get_hid_device_path(usb_vid_t vid, usb_pid_t pid,
		char ** pp_device_path)
{
	GUID guid;
	HDEVINFO device_info_set;
	size_t member_index;
	bool found = false;

	// Obtain the HID-device interface GUID
	HidD_GetHidGuid(&guid);

	/*
	 Open a handle to the plug and play device node.
	 Retrieve a device information set for the devices in
	 a specified class. What is returned is a pointer to
	 a device information set that contains information and all
	 attached and enumerated devices the specified device interface class.

	 Each device information element contains a handle to the device's devnode
	 and a linked list of device interfaces associated with the device.
	 */

	device_info_set = SetupDiGetClassDevs(&guid, NULL, // Define no enumerator (global)
			NULL, // Define no
			(DIGCF_PRESENT | // Only Devices present
					DIGCF_DEVICEINTERFACE));// Function class devices.

	if (INVALID_HANDLE_VALUE == device_info_set)
	{
		return (false);
	}

	// Initialize index
	member_index = 0;

	do
	{
		BOOL result;
		ULONG buf_size = 0;
		bool opened;
		hid_device_t hid_device;
		SP_DEVICE_INTERFACE_DATA device_intf_data;
		PSP_DEVICE_INTERFACE_DETAIL_DATA device_interface_detail = NULL;

		// Store size of the structure passed in
		ZeroMemory(&device_intf_data, sizeof(device_intf_data));
		device_intf_data.cbSize = sizeof(device_intf_data);

		/*
		 * Retrieve a specific device interface from the previously
		 * retrieved device_info_set array.
		 */
		result = SetupDiEnumDeviceInterfaces(device_info_set, 0, // No care about specific PDOs
				&guid, member_index, &device_intf_data);

		if (!result)
		{
			if (ERROR_NO_MORE_ITEMS == GetLastError())
			{
				// We found all the items we could
				break;
			}

			// We didn't finish
			break;
		}

		/*
		 * Retrieve the device path name for the device interface found.
		 * Do this one time to first learn the required buffer size
		 *
		 */
		result = SetupDiGetDeviceInterfaceDetail(device_info_set,
				&device_intf_data, NULL, // probing so no output buffer yet
				0, // probing so output buffer length of zero
				&buf_size, NULL); // not interested the specific dev-node

		if (!result) // We expect an error here
		{
			device_interface_detail = malloc(buf_size);
			if (device_interface_detail != NULL)
			{
				device_interface_detail->cbSize =
						sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
				ZeroMemory(device_interface_detail->DevicePath,
						sizeof(device_interface_detail->DevicePath));
			}
			else
			{
				break;
			}
		}

		// Now do it again to retrieve the actual detail information
		result = SetupDiGetDeviceInterfaceDetail(device_info_set,
				&device_intf_data, device_interface_detail, buf_size, &buf_size,
				NULL);

		if (!result)
		{
			break;
		}

		// Initialize local structure and open device
		memset(&hid_device, 0, sizeof(hid_device));
		opened = open_hid(device_interface_detail->DevicePath, false, // ReadAccess - none
				false, // WriteAccess - none
				false, // Overlapped - no
				false, // Exclusive - no
				&hid_device);

		if (!opened)
		{
			break;
		}

		// Now determine what to do. Is this the HID we are looking for?
		if ((pid == hid_device.attributes.ProductID)
				&& (vid == hid_device.attributes.VendorID))
		{
			// This is what we are looking for.

			// Check if they want the device path or if they are just
			// probing around.
			if (NULL != pp_device_path)
			{
				size_t device_path_size;

				// Create one
				device_path_size = strlen(device_interface_detail->DevicePath)
						+ 1;

				*pp_device_path = malloc(device_path_size);

				if (NULL == *pp_device_path)
				{
					break;
				}

				strncpy(*pp_device_path, device_interface_detail->DevicePath,
						device_path_size);
			}

			found = true; // stop searching
		}

		// Close device and move on
		usb_close_hid(&hid_device);

		// Free detailed data
		free(device_interface_detail);
		device_interface_detail = NULL;

		// Increment to next list member
		member_index++;

	} while (!found);

	// Free resources used when creating the list
	SetupDiDestroyDeviceInfoList(device_info_set);

	return (found);
}

bool usb_open_hid(char * const p_device_path, uint8_t options,
		phid_device_t p_hid_device)
{
	bool result = false;
	bool open_for_read = false;
	bool open_for_write = false;
	bool open_exclusive = false;
	bool open_overlapped = false;

	// Verify inputs
	if ((NULL != p_device_path) && (NULL != p_hid_device))
	{
		// Translate bit-mask to explicit booleans
		if (options & USB_READ_ACCESS)
		{
			open_for_read = true;
		}
		if (options & USB_WRITE_ACCESS)
		{
			open_for_write = true;
		}
		if (options & USB_EXCLUSIVE_ACCESS)
		{
			open_exclusive = true;
		}
		if (options & USB_OVERLAPPED)
		{
			open_overlapped = true;
		}

		// Open the device
		result = open_hid(p_device_path, open_for_read, open_for_write,
				open_exclusive, open_overlapped, p_hid_device);
	}

	return (result);
}

void usb_close_hid(phid_device_t p_hid_device)
{
	hid_report_type_t report_index;

	if (NULL == p_hid_device)
	{
		return;
	}

	if (INVALID_HANDLE_VALUE != p_hid_device->h_device)
	{
		CloseHandle(p_hid_device->h_device);
		p_hid_device->h_device = INVALID_HANDLE_VALUE;
	}

	if (NULL != p_hid_device->p_ppd)
	{
		HidD_FreePreparsedData(p_hid_device->p_ppd);
		p_hid_device->p_ppd = NULL;
	}

	for (report_index = HID_REPORT_TYPE_FIRST;
			report_index < HID_REPORT_TYPE_SIZE; report_index++)
	{
		phid_report_t report = &p_hid_device->report[report_index];

		if (NULL != report->p_report_buffer)
		{
			free(report->p_report_buffer);
			report->p_report_buffer = NULL;
		}

		if (NULL != report->p_hid_data)
		{
			free(report->p_hid_data);
			report->p_hid_data = NULL;
		}

		if (NULL != report->p_button_caps)
		{
			free(report->p_button_caps);
			report->p_button_caps = NULL;
		}

		if (NULL != report->p_value_caps)
		{
			free(report->p_value_caps);
			report->p_value_caps = NULL;
		}
	}

	// Re-Initialize
	memset(p_hid_device, 0, sizeof(*p_hid_device));

	return;
}
