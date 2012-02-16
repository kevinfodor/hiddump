/*
 ==============================================================================
 Name        : usb_enum.h
 Date        : Jul 26, 2011
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

#ifndef USB_ENUM_H_
#define USB_ENUM_H_

#ifdef __cplusplus
extern "C"
{
#endif

/* ************************************************************************* */
/*!
 \defgroup usb_enum

 \brief This group allows the enumeration of USB entries in the system.
 */
/* ************************************************************************* */

typedef enum _usb_device_type_t
{
	USB_HOST_CONTROLLER,

	USB_ROOT_HUB,

	USB_EXTERNAL_HUB,

	USB_DEVICE

} usb_device_type_t, *pusb_device_type_t;

typedef struct _usb_host_controller_info_t
{
	HANDLE h_host_controller;

	PTSTR p_driver_key;

#if 0
ULONG VendorID;

ULONG DeviceID;

ULONG SubSysID;

ULONG Revision;
#endif

} usb_host_controller_info_t, *pusb_host_controller_info_t;

typedef struct _usb_hub_info_t
{
	PTSTR p_hub_name;

	HANDLE h_hub;

	USB_NODE_INFORMATION node_info;

	// USB_HUB_CAPABILITIES_EX is only available in Vista and later headers
#if (_WIN32_WINNT >= 0x0600)
	USB_HUB_CAPABILITIES_EX hub_caps_ex;
#else
	USB_HUB_CAPABILITIES hub_caps;
#endif

} usb_hub_info_t, usb_root_hub_info_t, *pusb_hub_info_t, *pusb_root_hub_info_t;

//
// Structure used to build a linked list of string descriptors
// retrieved from a device.
//

typedef struct _usb_string_descriptor_entry_t
{
	struct _usb_string_descriptor_entry_t * p_next;

	uint8_t index;

	uint16_t language_id;

	USB_STRING_DESCRIPTOR string_descriptor[0];

} usb_string_descriptor_entry_t, *pusb_string_descriptor_entry_t;

//
// Structure used to build a linked list of configuration descriptors
// retrieved from a device.
//

typedef struct _usb_configuration_descriptor_entry_t
{
	struct _usb_configuration_descriptor_entry_t *p_next;

	pusb_string_descriptor_entry_t p_string_descriptors;

	uint8_t configuration_index;

	USB_CONFIGURATION_DESCRIPTOR configuration_descriptor[0];

} usb_configuration_descriptor_entry_t, *pusb_configuration_descriptor_entry_t;

typedef struct _usb_device_info_t
{
	HANDLE h_hub;

	USB_NODE_CONNECTION_INFORMATION connection_info;

	USB_DEVICE_DESCRIPTOR device_descriptor;

	pusb_configuration_descriptor_entry_t p_configuration_descriptors;

} usb_device_info_t, *pusb_device_info_t;

typedef struct _usb_external_hub_info_t
{
	usb_hub_info_t hub;

	usb_device_info_t device_info;

} usb_external_hub_info_t, *pusb_external_hub_info_t;

typedef union _usb_enum_info_union_t
{
	usb_host_controller_info_t host_controller;
	usb_root_hub_info_t root_hub;
	usb_external_hub_info_t external_hub;
	usb_device_info_t device;

} usb_enum_info_union_t, *pusb_enum_info_union_t;

typedef struct _usb_enum_info_t
{
	usb_device_type_t type;
	usb_enum_info_union_t u;

} usb_enum_info_t, *pusb_enum_info_t;

/* ************************************************************************** */
/*!
 \ingroup usb_enum

 \brief Callback declaration which caller must provide.

 \param[in] p_enum_info - Pointer to a structure describing the enumerated
 item.
 \param[in] p_arg - The user provided callback argument.

 \return Provided by implementation to indicate if enumeration should
 continue(true) or stop(false).

 */
/* ************************************************************************** */

typedef bool
(*USB_ENUM_ITEM_CALLBACK)(pusb_enum_info_t const p_enum_info, void *p_arg);

/* ************************************************************************** */
/*!
 \ingroup usb_enum

 \brief This API enumerates(iterates) over all USB entries.

 \param[in] usb_enum_item_callback - The raw output buffer with packet structures.
 \param[in] p_enum_item_callback_arg - Size of the output buffer to pack into.

 \return Indicates if enumeration was successful.

 This function performs a generic USB enumeration over the host controllers
 and root hubs available, allowing an application defined callback to be called
 for each hc, hub and port found. Applications can use this callback function
 to operate on the hc, hub or port being enumerated.

 The enumeration process goes like this:

 (1) Enumerate Host Controllers and Root Hubs
 Host controllers currently have symbolic link names of the form HCDx,
 where x starts at 0.  Use CreateFile() to open each host controller
 symbolic link.

 After a host controller has been opened, send the host controller an
 IOCTL_USB_GET_ROOT_HUB_NAME request to get the symbolic link name of
 the root hub that is part of the host controller.

 (2) Enumerate Hubs (Root Hubs and External Hubs)
 Given the name of a hub, use CreateFile() to hub the hub.  Send the
 hub an IOCTL_USB_GET_NODE_INFORMATION request to get info about the
 hub, such as the number of downstream ports.

 (3) Enumerate Downstream Ports
 Given an handle to an open hub and the number of downstream ports on
 the hub, send the hub an IOCTL_USB_GET_NODE_CONNECTION_INFORMATION
 request for each downstream port of the hub to get info about the
 device (if any) attached to each port.  If there is a device attached
 to a port, send the hub an IOCTL_USB_GET_NODE_CONNECTION_NAME request
 to get the symbolic link name of the hub attached to the downstream
 port.  If there is a hub attached to the downstream port, recurse to
 step (2).

 */
/* ************************************************************************** */

bool usb_enumerate(USB_ENUM_ITEM_CALLBACK usb_enum_item_callback,
		void *p_enum_item_callback_arg);

#ifdef __cplusplus
}
#endif

#endif /* USB_ENUM_H_ */
