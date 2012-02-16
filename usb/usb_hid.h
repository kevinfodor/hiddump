/*
 ==============================================================================
 Name        : usb_hid.h
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
 */

#ifndef USB_HID_H_
#define USB_HID_H_

#ifdef __cplusplus
extern "C"
{
#endif

/* ************************************************************************* */
/*!
 \defgroup usb_hid

 \brief These APIs open and close HIDs using the Windows APIs
 */
/* ************************************************************************* */

// HID open method options.
#define USB_READ_ACCESS				(0x01) // For Read
#define USB_WRITE_ACCESS			(0x02) // For Write
#define USB_EXCLUSIVE_ACCESS		(0x08) // Exclusive
#define USB_OVERLAPPED				(0x04) // Overlapped
// HID related descriptor types
#define USB_HID_DESCRIPTOR_TYPE         (0x21)
#define USB_HID_REPORT_DESCRIPTOR_TYPE  (0x22)

__PACKED__

// HID Class HID Descriptor
//
typedef struct _usb_hid_descriptor_t
{
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint16_t bcdHID;
	uint8_t bCountryCode;
	uint8_t bNumDescriptors;

	struct
	{
		uint8_t bDescriptorType;
		uint16_t wDescriptorLength;

	} optional_descriptors[1];

} usb_hid_descriptor_t, *pusb_hid_descriptor_t;

// HID Class HID Report Descriptor
//
typedef struct _usb_hid_report_descriptor_t
{
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t data[0];

} usb_hid_report_descriptor_t, *pusb_hid_report_descriptor_t;

__UNPACKED__

/*

 A structure to hold the steady state data received from the hid device.
 Each time a read packet is received we fill in this structure.
 Each time we wish to write to a hid device we fill in this structure.
 This structure is here only for convenience.  Most real applications will
 have a more efficient way of moving the hid data to the read, write, and
 feature routines.
 */

typedef struct _hid_data_t
{
	bool is_button;
	USAGE usage_page; // The usage page for which we are looking.
	uint32_t status; // The last status returned from the access function
	// when updating this field.
	uint8_t report_id; // ReportID for this given data structure
	bool is_data_set; // Variable to track whether a given data structure
	//  has already been added to a report structure

	union
	{
		struct
		{
			USAGE usage_min; // Variables to track the usage minimum and max
			USAGE usage_max; // If equal, then only a single usage
			size_t max_usage_length; // Usages buffer length.
			PUSAGE p_usages; // list of usages (buttons ``down'' on the device.

		} button;

		struct
		{
			USAGE usage; // The usage describing this value;

			uint32_t value; // The raw value from the device
			int32_t scaled_value; // The value with scale applied

		} value;
	};

} hid_data_t, *phid_data_t;

typedef enum _hid_report_type_t
{
	HID_REPORT_TYPE_FIRST = HidP_Input,
	HID_REPORT_TYPE_INPUT = HID_REPORT_TYPE_FIRST,
	HID_REPORT_TYPE_OUTPUT = HidP_Output,
	HID_REPORT_TYPE_FEATURE = HidP_Feature,
	HID_REPORT_TYPE_SIZE

} hid_report_type_t, *p_hid_report_type_t;

// For the above enumeration to work, the following must be true
COMPILE_TIME_ASSERT(HID_REPORT_TYPE_SIZE == 3, hid_report_type_t_is_wrong_size);

typedef struct _hid_report_t
{
	char *p_report_buffer;
	size_t report_buffer_length;

	phid_data_t p_hid_data; // array of hid data structures
	size_t hid_data_length; // Number elements in this array.

	PHIDP_BUTTON_CAPS p_button_caps;
	size_t number_button_caps;

	PHIDP_VALUE_CAPS p_value_caps;
	size_t number_value_caps;

} hid_report_t, *phid_report_t;

typedef struct _hid_device_t
{
	HANDLE h_device; // A file handle to the hid device.

	PHIDP_PREPARSED_DATA p_ppd; // The opaque parser info describing this device
	HIDP_CAPS caps; // The Capabilities of this hid device.
	HIDD_ATTRIBUTES attributes;

	// Hid Report Type Details
	hid_report_t report[HID_REPORT_TYPE_SIZE];

} hid_device_t, *phid_device_t;

typedef uint8_t usb_open_options_t;

// APIs

/* ************************************************************************** */
/*!
 \ingroup usb_hid

 \brief Retrieves the null terminated character string containing a HID
 path based on the provided VID and PID.

 \param[in] vid - The Vendor-Id of the HID.
 \param[in] pid - The Product-Id of the HID.
 \param[in,out] pp_device_path - The pointer to the device path (must be freed).

 \return Indicates if the location and retrieval of the path was
 successful.

 */
/* ************************************************************************** */

bool usb_get_hid_device_path(usb_vid_t vid, usb_pid_t pid,
		char ** pp_device_path);

/* ************************************************************************** */
/*!
 \ingroup usb_hid

 \brief Opens the HID identified by the provided device path and options.

 \param[in] pp_device_path - The device path of the HID.
 \param[in] options - A bitmask containing the desired access options.
 \param[in,out] p_hid_device - A pointer to a hid_device_t struct to populate.

 \return Indicates if the HID was opened successfully.

 */
/* ************************************************************************** */

bool usb_open_hid(char * const pp_device_path, uint8_t options,
		phid_device_t p_hid_device);

/* ************************************************************************** */
/*!
 \ingroup usb_hid

 \brief Closes the HID identified by the provided p_hid_device_t struct.

 \param[in] p_hid_device - A pointer to the HID to close.

 \return Indicates if the HID was closed successfully.

 */
/* ************************************************************************** */

void usb_close_hid(phid_device_t p_hid_device);

#ifdef __cplusplus
}
#endif

#endif /* UBS_HID_H_ */
