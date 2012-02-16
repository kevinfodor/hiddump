/*
 ==============================================================================
 Name        : usb_hid_reports.h
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

#ifndef USB_HID_REPORTS_H_
#define USB_HID_REPORTS_H_

#ifdef __cplusplus
extern "C"
{
#endif

/* ************************************************************************* */
/*!
 \defgroup usb_hid_reports

 \brief These APIs read, write and process HID reports using the Windows APIs
 */
/* ************************************************************************* */

/* ************************************************************************** */
/*!
 \ingroup usb_hid_reports

 \brief Reads a report from a HID.

 \param[in] p_hid_device - A pointer to the HID to read from.

 \return Indicates if the HID read completed successfully.

 */
/* ************************************************************************** */

bool hid_read(phid_device_t p_hid_device);

/* ************************************************************************** */
/*!
 \ingroup usb_hid_reports

 \brief Reads a report from a HID using the overlapped I/O method.

 \param[in] p_hid_device - A pointer to the HID to read from.
 \param[] h_completion_event - A handle to an event to use for indicating
 when an overlapped report read is completed.

 \return Indicates if the HID read completed successfully.

 */
/* ************************************************************************** */

bool hid_read_overlapped(phid_device_t p_hid_device, HANDLE h_completion_event);

/* ************************************************************************** */
/*!
 \ingroup usb_hid_reports

 \brief Writes a report to the HID.

 \param[in] p_hid_device - A pointer to the HID to write to.

 \return Indicates if the HID write completed successfully.

 */
/* ************************************************************************** */

bool hid_write(phid_device_t p_hid_device);

/* ************************************************************************** */
/*!
 \ingroup usb_hid_reports

 \brief Sets a report feature on the HID.

 \param[in] p_hid_device - A pointer to the HID to set the feature of.

 \return Indicates if the HID set completed successfully.

 */
/* ************************************************************************** */

bool hid_set_feature(phid_device_t p_hid_device);

/* ************************************************************************** */
/*!
 \ingroup usb_hid_reports

 \brief Gets a report feature from the HID.

 \param[in] p_hid_device - A pointer to the HID to get the feature from.

 \return Indicates if the HID get completed successfully.

 */
/* ************************************************************************** */

bool hid_get_feature(phid_device_t p_hid_device);

/* ************************************************************************** */
/*!
 \ingroup usb_hid_reports

 \brief This API unpacks a HID report buffer retrieved from the device.

 \param[in] p_report_buffer - The raw output buffer with packed structures.
 \param[in] report_buffer_length - Size of the output buffer to unpack from.
 \param[in] report_type - The report type (input, output, feature) being unpacked.
 \param[in,out] p_hid_data - The data structure to unpack to.
 \param[in] hid_data_length - The number of data structures to unpack.
 \param[in] p_ppd - The HID's pre-parsed data.

 \return Indicates if the report unpacking was successful.

 This routine takes in a raw report buffer and unpacks it into a list of
 phid_data_t structures (p_hid_data) the given report for all data values in
 the list that correspond to the report ID of the first item in the list.

 For every data structure in the list that has the same report ID as the first
 item in the list will be set in the report.  Every data item that is
 set will also have it's is_data_set field marked with true.

 A return value of false indicates an unexpected error occurred when retrieving
 a given data value.  The caller should expect that assume that no values
 within the given data structure were set.

 A return value of true indicates that all data values for the given report
 ID were set without error.

 */
/* ************************************************************************** */

bool hid_unpack_report(char * const p_report_buffer,
		size_t report_buffer_length, HIDP_REPORT_TYPE report_type,
		phid_data_t p_hid_data, size_t hid_data_length,
		PHIDP_PREPARSED_DATA const p_ppd);

/* ************************************************************************** */
/*!
 \ingroup usb_hid_reports

 \brief This API packs a HID report buffer for sending to the device.

 \param[in] p_report_buffer - The raw output buffer with packed structures.
 \param[in] report_buffer_length - Size of the output buffer to pack into.
 \param[in] report_type - The report type (input, output, feature) being packed.
 \param[in,out] p_hid_data - The data structure to pack from.
 \param[in] hid_data_length - The number of data structures to pack.
 \param[in] p_ppd - The HID's pre-parsed data.

 \return Indicates if the report packing was successful.

 This routine takes in a list of phid_data_t structures (p_hid_data) and builds
 in report_buffer the given report for all data values in the list that
 correspond to the report ID of the first item in the list.

 For every data structure in the list that has the same report ID as the first
 item in the list will be set in the report.  Every data item that is
 set will also have it's is_data_set field marked with true.

 A return value of false indicates an unexpected error occurred when setting
 a given data value.  The caller should expect that assume that no values
 within the given data structure were set.

 A return value of true indicates that all data values for the given report
 ID were set without error.

 */
/* ************************************************************************** */

bool hid_pack_report(char * p_report_buffer, size_t report_buffer_length,
		HIDP_REPORT_TYPE report_type, phid_data_t p_hid_data,
		size_t hid_data_length, PHIDP_PREPARSED_DATA const p_ppd);

#ifdef __cplusplus
}
#endif

#endif /* USB_HID_REPORTS_H_ */
