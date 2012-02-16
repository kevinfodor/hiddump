/*
 ==============================================================================
 Name        : usb_hid_reports.c
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
#include "usb_hid_reader.h"

// Module include
#include "usb_hid_reports.h"

// Local declarations

// Implementation

bool hid_read(phid_device_t p_hid_device)
{
	DWORD length;
	bool result = false;
	BOOL success;
	phid_report_t p_report = &p_hid_device->report[HID_REPORT_TYPE_INPUT];

	// We only do this if we can actually receive an input report
	if (0 == p_report->report_buffer_length)
	{
		return (false);
	}

	/*
	 The Windows' HID driver requests input reports periodically.
	 The driver stores these reports in a buffer. ReadFile retrieves
	 on or more reports from the buffer. If the buffer is empty, ReadFile
	 waits for a report to arrive. In this case below, we attempt
	 to read a single report.
	 */
	success = ReadFile(p_hid_device->h_device, p_report->p_report_buffer,
			p_report->report_buffer_length, &length, NULL);
	if ((success) && (length == p_report->report_buffer_length))
	{
		// Unpack the report into our provided HID data structure
		result = hid_unpack_report(p_report->p_report_buffer,
				p_report->report_buffer_length, HidP_Input,
				p_report->p_hid_data, p_report->hid_data_length,
				p_hid_device->p_ppd);
	}

	return (result);
}

bool hid_read_overlapped(phid_device_t p_hid_device, HANDLE h_completion_event)
{
	static OVERLAPPED overlap;
	DWORD length;
	bool status;
	BOOL read_status;
	phid_report_t p_report = &p_hid_device->report[HID_REPORT_TYPE_INPUT];

	// We only do this if we can actually receive an input report
	if (0 == p_report->report_buffer_length)
	{
		return (false);
	}

	/*
	 Setup the overlap structure using the completion event passed in to
	 use for signalling the completion of the Read
	 */

	memset(&overlap, 0, sizeof(OVERLAPPED));

	// Assign he completion event
	overlap.hEvent = h_completion_event;

	/*
	 Execute the read call saving the return code to determine how to
	 proceed (ie. the read completed synchronously or not).
	 The Windows' HID driver requests input reports periodically.
	 The driver stores these reports in a buffer. ReadFile retrieves
	 on or more reports from the buffer. If the buffer is empty, ReadFile
	 waits for a report to arrive. In this case below, we attempt
	 to read a single report.
	 */
	read_status = ReadFile(p_hid_device->h_device, p_report->p_report_buffer,
			p_report->report_buffer_length, &length, &overlap);
	/*
	 If the status is false, then one of two cases occurred.
	 1) ReadFile call succeeded but the Read is an overlapped one.  Here,
	 we should return true to indicate that the Read succeeded.  However,
	 the calling thread should be blocked on the completion event
	 which means it won't continue until the read actually completes

	 2) The ReadFile call failed for some unknown reason...In this case,
	 the return code will be false
	 */
	if (!read_status)
	{
		DWORD error;

		// Get error that occurred to determine if the read is still pending
		// or if it is completed.
		error = GetLastError();
		status = (ERROR_IO_PENDING == GetLastError());
	}
	else
	{
		/*
		 If read_status is true, then the ReadFile call completed synchronously,
		 since the calling thread is probably going to wait on the completion
		 event, signal the event so it knows it can continue.
		 */
		SetEvent(h_completion_event);
		status = true;
	}

	return (status);
}

bool hid_write(phid_device_t p_hid_device)
{
	DWORD written;
	phid_data_t p_hid_data;
	size_t index;
	bool status = true;
	BOOL write_status;
	phid_report_t p_report = &p_hid_device->report[HID_REPORT_TYPE_OUTPUT];

	/*
	 Begin by looping through the HID's hid_data_t structure and setting
	 the is_data_set field to false to indicate that each structure has
	 not yet been set for this WriteFile call.
	 */
	p_hid_data = p_report->p_hid_data;
	for (index = 0; index < p_report->hid_data_length; index++, p_hid_data++)
	{
		p_hid_data->is_data_set = false;
	}

	/*
	 In setting all the data in the reports, we need to pack a report buffer
	 and call WriteFile for each report ID that is represented by the
	 device structure.  To do so, the is_data_set field will be used to
	 determine if a given report field has already been set.
	 */
	p_hid_data = p_report->p_hid_data;
	for (index = 0; index < p_report->hid_data_length; index++, p_hid_data++)
	{
		if (!p_hid_data->is_data_set)
		{
			/*
			 Package the report for this data structure.  hid_pack_report will
			 set the is_data_set fields of this structure and any other
			 structures that it includes in the report with this structure
			 */
			hid_pack_report(p_report->p_report_buffer,
					p_report->report_buffer_length, HidP_Output, p_hid_data,
					p_report->hid_data_length - index, p_hid_device->p_ppd);

			// Now a report has been packaged up...Send it down to the device
			write_status = WriteFile(p_hid_device->h_device,
					p_report->p_report_buffer, p_report->report_buffer_length,
					&written, NULL)
					&& (written == p_report->report_buffer_length);

			status = (status && write_status); // any failure returns failure
		}
	}

	return (status);
}

bool hid_set_feature(phid_device_t p_hid_device)
{
	phid_data_t p_hid_data;
	size_t index;
	bool status = true;
	BOOL feature_status;
	phid_report_t p_report = &p_hid_device->report[HID_REPORT_TYPE_FEATURE];

	/*
	 Begin by looping through the HID_DEVICE's phid_data_t structure and
	 setting the is_data_set field to false to indicate that each structure has
	 not yet been set for this hid_set_feature() call.
	 */
	p_hid_data = p_report->p_hid_data;
	for (index = 0; index < p_report->hid_data_length; index++, p_hid_data++)
	{
		p_hid_data->is_data_set = false;
	}

	/*
	 In setting all the data in the reports, we need to pack a report buffer
	 and call WriteFile for each report ID that is represented by the
	 device structure.  To do so, the is_data_set field will be used to
	 determine if a given report field has already been set.
	 */
	p_hid_data = p_report->p_hid_data;
	for (index = 0; index < p_report->hid_data_length; index++, p_hid_data++)
	{
		if (!p_hid_data->is_data_set)
		{
			/*
			 Package the report for this data structure.  hid_pack_report will
			 set the is_data_set fields of this structure and any other
			 structures that it includes in the report with this structure
			 */

			hid_pack_report(p_report->p_report_buffer,
					p_report->report_buffer_length, HidP_Feature, p_hid_data,
					p_report->hid_data_length - index, p_hid_device->p_ppd);

			// Now a report has been packaged up...Send it down to the device
			feature_status = HidD_SetFeature(p_hid_device->h_device,
					p_report->p_report_buffer, p_report->report_buffer_length);

			status = (feature_status && status); // any failure returns failure
		}
	}

	return (status);
}

bool hid_get_feature(phid_device_t p_hid_device)
{
	size_t index;
	phid_data_t p_hid_data;
	bool status = true;
	BOOL feature_status;
	phid_report_t p_report = &p_hid_device->report[HID_REPORT_TYPE_FEATURE];

	/*
	 Begin by looping through the HID_DEVICE's phid_data_t structure and
	 setting the is_data_set field to false to indicate that each structure has
	 not yet been set for this hid_set_feature() call.
	 */
	p_hid_data = p_report->p_hid_data;
	for (index = 0; index < p_report->hid_data_length; index++, p_hid_data++)
	{
		p_hid_data->is_data_set = false;
	}

	/*
	 Next, each structure in the phid_data_t buffer is filled in with a value
	 that is retrieved from one or more calls to HidD_GetFeature.  The
	 number of calls is equal to the number of reportIDs on the device
	 */
	p_hid_data = p_report->p_hid_data;
	for (index = 0; index < p_report->hid_data_length; index++, p_hid_data++)
	{
		/*
		 If a value has yet to have been set for this structure, build a report
		 buffer with its report ID as the first byte of the buffer and pass
		 it in the HidD_GetFeature call.  Specifying the report ID in the
		 first specifies which report is actually retrieved from the device.
		 The rest of the buffer should be zeroed before the call
		 */
		if (!p_hid_data->is_data_set)
		{
			memset(p_report->p_report_buffer, 0,
					p_report->report_buffer_length);

			p_report->p_report_buffer[0] = p_hid_data->report_id;

			feature_status = HidD_GetFeature(p_hid_device->h_device,
					p_report->p_report_buffer, p_report->report_buffer_length);

			if (feature_status)
			{
				/*
				 If the return value is true, scan through the rest of the
				 phid_data_t structures and fill whatever values we can from
				 this report.
				 */
				feature_status = hid_unpack_report(p_report->p_report_buffer,
						p_report->report_buffer_length, HidP_Feature,
						p_report->p_hid_data, p_report->hid_data_length,
						p_hid_device->p_ppd);
			}

			status = (status && feature_status); // any failure returns failure
		}
	}

	return (status);
}

bool hid_unpack_report(char * const report_buffer, size_t report_buffer_length,
		HIDP_REPORT_TYPE report_type, phid_data_t p_hid_data,
		size_t hid_data_length, PHIDP_PREPARSED_DATA p_ppd)
{
	size_t index;
	uint8_t report_id;

	report_id = report_buffer[0]; // Report id is the first byte

	for (index = 0; index < hid_data_length; index++, p_hid_data++)
	{
		// Make sure the p_hid_data we are using to unpack is of
		// the same report type (id)
		if (report_id == p_hid_data->report_id)
		{
			// Button
			if (p_hid_data->is_button)
			{
				ULONG num_usages; // Number of usages returned from GetUsages.
				ULONG next_usage;
				ULONG index_usage;

				num_usages = p_hid_data->button.max_usage_length;

				// Extract all usages
				p_hid_data->status = HidP_GetUsages(report_type,
						p_hid_data->usage_page,
						0, // All collections
						p_hid_data->button.p_usages, &num_usages, p_ppd,
						report_buffer, report_buffer_length);

				/*
				 Get usages writes the list of usages into the buffer
				 p_data->button.usages. num_usages is set to the number of
				 usages written into this array.

				 A usage cannot not be defined as zero, so we'll mark a zero
				 following the list of usages to indicate the end of the list of
				 usages

				 NOTE: One anomaly of the GetUsages function is the lack of
				 ability to distinguish the data for one ButtonCaps from another
				 if two different caps structures have the same UsagePage
				 For instance:
				 Caps1 has UsagePage 07 and UsageRange of 0x00 - 0x167
				 Caps2 has UsagePage 07 and UsageRange of 0xe0 - 0xe7

				 However, calling GetUsages for each of the data structs
				 will return the same list of usages.  It is the
				 responsibility of the caller to set in the hid_device_t
				 structure which usages actually are valid for the
				 that structure.
				 */

				/*
				 Search through the usage list and remove those that
				 correspond to usages outside the define ranged for this
				 data structure.
				 */

				for (index_usage = 0, next_usage = 0; index_usage < num_usages;
						index_usage++)
				{
					if (p_hid_data->button.usage_min
							<= p_hid_data->button.p_usages[index_usage]
							&& p_hid_data->button.p_usages[index_usage]
									<= p_hid_data->button.usage_max)
					{
						p_hid_data->button.p_usages[next_usage++] =
								p_hid_data->button.p_usages[index_usage];
					}
				}

				if (next_usage < p_hid_data->button.max_usage_length)
				{
					p_hid_data->button.p_usages[next_usage] = 0;
				}
			}
			// Value
			else
			{
				LONG scaled_value = 0;
				ULONG value = 0;

				p_hid_data->status = HidP_GetUsageValue(report_type,
						p_hid_data->usage_page,
						0, // All Collections.
						p_hid_data->value.usage, &value, p_ppd, report_buffer,
						report_buffer_length);
				p_hid_data->value.value = value;

				if (HIDP_STATUS_SUCCESS != p_hid_data->status)
				{
					return (false);
				}

				p_hid_data->status = HidP_GetScaledUsageValue(report_type,
						p_hid_data->usage_page,
						0, // All Collections.
						p_hid_data->value.usage, &scaled_value, p_ppd,
						report_buffer, report_buffer_length);
				p_hid_data->value.scaled_value = scaled_value;
			}

			p_hid_data->is_data_set = true;
		}
	}
	return (true);
}

bool hid_pack_report(char * report_buffer, size_t report_buffer_length,
		HIDP_REPORT_TYPE report_type, phid_data_t p_hid_data,
		size_t hid_data_length, PHIDP_PREPARSED_DATA const p_ppd)
{
	size_t index;
	uint8_t curr_report_id;

	// All report buffers that are initially sent need to be zero'd out.
	memset(report_buffer, 0, report_buffer_length);

	/*
	 Go through the data structures and set all the values that correspond to
	 the curr_report_id which is obtained from the first data structure
	 in the list
	 */
	curr_report_id = p_hid_data->report_id;

	for (index = 0; index < hid_data_length; index++, p_hid_data++)
	{
		/*
		 There are two different ways to determine if we set the current data
		 structure:
		 1) Store the report ID were using and only attempt to set those
		 data structures that correspond to the given report ID.  This
		 example shows this implementation.

		 2) Attempt to set all of the data structures and look for the
		 returned status value of HIDP_STATUS_INVALID_REPORT_ID.  This
		 error code indicates that the given usage exists but has a
		 different report ID than the report ID in the current report
		 buffer
		 */
		if (p_hid_data->report_id == curr_report_id)
		{
			if (p_hid_data->is_button)
			{
				ULONG num_usages; // Number of usages to set for a given report.

				num_usages = p_hid_data->button.max_usage_length;

				p_hid_data->status = HidP_SetUsages(report_type,
						p_hid_data->usage_page,
						0, // All collections
						p_hid_data->button.p_usages, &num_usages, p_ppd,
						report_buffer, report_buffer_length);
			}
			else
			{
				p_hid_data->status = HidP_SetUsageValue(report_type,
						p_hid_data->usage_page,
						0, // All Collections.
						p_hid_data->value.usage, p_hid_data->value.value, p_ppd,
						report_buffer, report_buffer_length);
			}

			if (HIDP_STATUS_SUCCESS != p_hid_data->status)
			{
				return (false);
			}
		}
	}

	/*
	 At this point, all data structures that have the same report_id as the
	 first one will have been set in the given report.  Time to loop
	 through the structure again and mark all of those data structures as
	 having been set.
	 */
	for (index = 0; index < hid_data_length; index++, p_hid_data++)
	{
		if (curr_report_id == p_hid_data->report_id)
		{
			p_hid_data->is_data_set = true;
		}
	}

	return (true);
}
