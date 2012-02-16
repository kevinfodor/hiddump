/*
 ==============================================================================
 Name        : usb_enum.c
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

 Notice: The contents of this file were created with extensive help
 from the Microsoft Windows Driver Kit (WDK) sample application "USBView"
 You can find more details about USBView at
 http://msdn.microsoft.com/en-us/library/ff558728(v=VS.85).aspx

 The Usbview.exe sample demonstrates how a user-mode application can
 enumerate USB host controllers, USB hubs, and attached USB devices and
 can query information about the devices from the registry and through
 USB requests to the devices.

 Specifically the contents of this file were derived from the Enum.c
 file in that sample project (src\usb\usbview). I will include the
 copyright from that file here to give credit where it is due, although
 I do not claim to know anything about any legal ramifications of deriving
 code from those provided examples.

 For "Enum.c" Copyright (c) 1997-1998 Microsoft Corporation

 */

// Standard includes
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Windows includes
#include <windows.h>
#include <initguid.h> // only include once!
#include <setupapi.h>
#include <tchar.h>

// WDK includes
#include <usbiodef.h>
#include <usbioctl.h>
#include <usb.h>
#include <usb100.h>

// Module include
#include "usb_enum.h"

/*
 * Local helper functions
 */
static PTSTR WideStrToMultiStr(LPCWSTR WideStr);

static void GetConnectionInfoFromHub(HANDLE Hub,
		PUSB_NODE_CONNECTION_INFORMATION pConnectionInfoEx, ULONG iIndex);

static PTSTR GetExternalHubName(HANDLE Hub, ULONG ConnectionIndex);

static PTSTR GetRootHubName(HANDLE hHostController);

static PTSTR GetHCDDriverKeyName(HANDLE hHostController);

static pusb_string_descriptor_entry_t GetStringDescriptor(HANDLE hHubDevice,
		ULONG ConnectionIndex, UCHAR DescriptorIndex, USHORT LanguageID);

static pusb_string_descriptor_entry_t GetStringDescriptors(HANDLE hHubDevice,
		ULONG ConnectionIndex, UCHAR DescriptorIndex, ULONG NumLanguageIDs,
		USHORT *LanguageIDs, pusb_string_descriptor_entry_t StringDescNodeTail);

static pusb_string_descriptor_entry_t GetAllStringDescriptors(HANDLE hHubDevice,
		ULONG ConnectionIndex, PUSB_DEVICE_DESCRIPTOR DeviceDesc,
		PUSB_CONFIGURATION_DESCRIPTOR ConfigDesc);

static bool usb_get_device_descriptor(HANDLE h_hub_device,
		size_t connection_index, uint8_t descriptor_index,
		uint8_t ** pp_descriptor, uint16_t * p_descriptor_length);

static bool usb_get_config_descriptor(HANDLE h_hub_device,
		size_t connection_index, uint8_t descriptor_index,
		uint8_t ** pp_descriptor, uint16_t * p_descriptor_length);

static void usb_free_descriptor(uint8_t * p_descriptor);

static bool usb_get_descriptors_from_node(pusb_device_info_t p_device_info,
		size_t connection_node);

/*
 * Local Enumerator functions
 */

static bool usb_enumerate_ports(HANDLE h_hub, uint8_t num_ports,
		USB_ENUM_ITEM_CALLBACK enum_item_callback, void * p_enum_item_arg);

static bool usb_enumerate_hubs(HANDLE h_host_controller,
		size_t connection_index, USB_ENUM_ITEM_CALLBACK enum_item_callback,
		void * p_enum_item_arg);

static PTSTR WideStrToMultiStr(LPCWSTR WideStr)
{
	ULONG nBytes;
	PTSTR MultiStr;

	// Get the length of the converted string
	//
	nBytes = WideCharToMultiByte(CP_ACP, 0, WideStr, -1, NULL, 0, NULL, NULL);

	if (nBytes == 0)
	{
		return NULL;
	}

	// Allocate space to hold the converted string
	//
	MultiStr = malloc(nBytes);

	if (MultiStr == NULL)
	{
		return NULL;
	}

	// Convert the string
	//
	nBytes = WideCharToMultiByte(CP_ACP, 0, WideStr, -1, MultiStr, nBytes, NULL,
			NULL);

	if (nBytes == 0)
	{
		free(MultiStr);
		return NULL;
	}
	return MultiStr;
}

static void GetConnectionInfoFromHub(HANDLE hHub,
		PUSB_NODE_CONNECTION_INFORMATION pConnectionInfoEx, ULONG iIndex)
{
	ULONG nBytesEx = sizeof(USB_NODE_CONNECTION_INFORMATION);
	BOOL success;

	//
	// Now query USBHUB for the USB_NODE_CONNECTION_INFORMATION structure
	// for this port.  This will tell us if a device is attached to this
	// port, among other things.
	//
	pConnectionInfoEx->ConnectionIndex = iIndex;

	success = DeviceIoControl(hHub, IOCTL_USB_GET_NODE_CONNECTION_INFORMATION,
			pConnectionInfoEx, nBytesEx, pConnectionInfoEx, nBytesEx, &nBytesEx,
			NULL);

	if (!success)
	{
		USB_NODE_CONNECTION_INFORMATION connectionInfo;
		ULONG nBytes = sizeof(USB_NODE_CONNECTION_INFORMATION);

		connectionInfo.ConnectionIndex = iIndex;

		success = DeviceIoControl(hHub,
				IOCTL_USB_GET_NODE_CONNECTION_INFORMATION, &connectionInfo,
				nBytes, &connectionInfo, nBytes, &nBytes, NULL);

		if (success)
		{

			// Copy IOCTL_USB_GET_NODE_CONNECTION_INFORMATION into
			// USB_NODE_CONNECTION_INFORMATION structure.
			//
			pConnectionInfoEx->ConnectionIndex = connectionInfo.ConnectionIndex;

			pConnectionInfoEx->ConnectionStatus =
					connectionInfo.ConnectionStatus;

			pConnectionInfoEx->CurrentConfigurationValue =
					connectionInfo.CurrentConfigurationValue;

			pConnectionInfoEx->DeviceAddress = connectionInfo.DeviceAddress;

			pConnectionInfoEx->DeviceDescriptor =
					connectionInfo.DeviceDescriptor;

			pConnectionInfoEx->DeviceIsHub = connectionInfo.DeviceIsHub;

			pConnectionInfoEx->LowSpeed = connectionInfo.LowSpeed;

			pConnectionInfoEx->NumberOfOpenPipes =
					connectionInfo.NumberOfOpenPipes;

			memcpy(&pConnectionInfoEx->PipeList[0], &connectionInfo.PipeList[0],
					sizeof(USB_PIPE_INFO) * 0);
		}
	}

	return;
}

static PTSTR GetExternalHubName(HANDLE Hub, ULONG ConnectionIndex)
{
	BOOL success;
	ULONG nBytes;
	USB_NODE_CONNECTION_NAME extHubName;
	PUSB_NODE_CONNECTION_NAME extHubNameW;
	PTSTR extHubNameA;

	extHubNameW = NULL;
	extHubNameA = NULL;

	// Get the length of the name of the external hub attached to the
	// specified port.
	//
	extHubName.ConnectionIndex = ConnectionIndex;

	success = DeviceIoControl(Hub, IOCTL_USB_GET_NODE_CONNECTION_NAME,
			&extHubName, sizeof(extHubName), &extHubName, sizeof(extHubName),
			&nBytes, NULL);

	if (!success)
	{
		goto GetExternalHubNameError;
	}

	// Allocate space to hold the external hub name
	//
	nBytes = extHubName.ActualLength;

	if (nBytes <= sizeof(extHubName))
	{
		goto GetExternalHubNameError;
	}

	extHubNameW = malloc(nBytes);

	if (extHubNameW == NULL)
	{
		goto GetExternalHubNameError;
	}

	// Get the name of the external hub attached to the specified port
	//
	extHubNameW->ConnectionIndex = ConnectionIndex;

	success = DeviceIoControl(Hub, IOCTL_USB_GET_NODE_CONNECTION_NAME,
			extHubNameW, nBytes, extHubNameW, nBytes, &nBytes, NULL);

	if (!success)
	{
		goto GetExternalHubNameError;
	}

	// Convert the External Hub name
	//
	extHubNameA = WideStrToMultiStr(extHubNameW->NodeName);

	// All done, free the uncoverted external hub name and return the
	// converted external hub name
	//
	free(extHubNameW);

	return extHubNameA;

	GetExternalHubNameError:
	// There was an error, free anything that was allocated
	//
	if (extHubNameW != NULL)
	{
		free(extHubNameW);
		extHubNameW = NULL;
	}

	return NULL;
}

static PTSTR GetRootHubName(HANDLE hHostController)
{
	BOOL success;
	ULONG nBytes;
	USB_ROOT_HUB_NAME rootHubName;
	PUSB_ROOT_HUB_NAME rootHubNameW;
	PTSTR rootHubNameA;

	rootHubNameW = NULL;
	rootHubNameA = NULL;

	// Get the length of the name of the Root Hub attached to the
	// Host Controller
	//
	success = DeviceIoControl(hHostController, IOCTL_USB_GET_ROOT_HUB_NAME, 0,
			0, &rootHubName, sizeof(rootHubName), &nBytes, NULL);

	if (!success)
	{
		goto GetRootHubNameError;
	}

	// Allocate space to hold the Root Hub name
	//
	nBytes = rootHubName.ActualLength;

	rootHubNameW = malloc(nBytes);

	if (rootHubNameW == NULL)
	{
		goto GetRootHubNameError;
	}

	// Get the name of the Root Hub attached to the Host Controller
	//
	success = DeviceIoControl(hHostController, IOCTL_USB_GET_ROOT_HUB_NAME,
			NULL, 0, rootHubNameW, nBytes, &nBytes, NULL);

	if (!success)
	{
		goto GetRootHubNameError;
	}

	// Convert the Root Hub name
	//
	rootHubNameA = WideStrToMultiStr(rootHubNameW->RootHubName);

	// All done, free the uncoverted Root Hub name and return the
	// converted Root Hub name
	//
	free(rootHubNameW);

	return rootHubNameA;

	GetRootHubNameError:
	// There was an error, free anything that was allocated
	//
	if (rootHubNameW != NULL)
	{
		free(rootHubNameW);
		rootHubNameW = NULL;
	}

	return NULL;
}

static PTSTR GetHCDDriverKeyName(HANDLE hHostController)
{
	BOOL success;
	ULONG nBytes;
	USB_HCD_DRIVERKEY_NAME driverKeyName;
	PUSB_HCD_DRIVERKEY_NAME driverKeyNameW;
	PTSTR driverKeyNameA;

	driverKeyNameW = NULL;
	driverKeyNameA = NULL;

	// Get the length of the name of the driver key of the HCD
	//
	success = DeviceIoControl(hHostController, IOCTL_GET_HCD_DRIVERKEY_NAME,
			&driverKeyName, sizeof(driverKeyName), &driverKeyName,
			sizeof(driverKeyName), &nBytes, NULL);

	if (!success)
	{
		goto GetHCDDriverKeyNameError;
	}

	// Allocate space to hold the driver key name
	//
	nBytes = driverKeyName.ActualLength;

	if (nBytes <= sizeof(driverKeyName))
	{
		goto GetHCDDriverKeyNameError;
	}

	driverKeyNameW = malloc(nBytes);

	if (driverKeyNameW == NULL)
	{
		goto GetHCDDriverKeyNameError;
	}

	// Get the name of the driver key of the device attached to
	// the specified port.
	//
	success = DeviceIoControl(hHostController, IOCTL_GET_HCD_DRIVERKEY_NAME,
			driverKeyNameW, nBytes, driverKeyNameW, nBytes, &nBytes, NULL);

	if (!success)
	{
		goto GetHCDDriverKeyNameError;
	}

	// Convert the driver key name
	//
	driverKeyNameA = WideStrToMultiStr(driverKeyNameW->DriverKeyName);

	// All done, free the uncoverted driver key name and return the
	// converted driver key name
	//
	free(driverKeyNameW);

	return driverKeyNameA;

	GetHCDDriverKeyNameError:
	// There was an error, free anything that was allocated
	//
	if (driverKeyNameW != NULL)
	{
		free(driverKeyNameW);
		driverKeyNameW = NULL;
	}

	return NULL;
}

static pusb_string_descriptor_entry_t GetStringDescriptor(HANDLE hHubDevice,
		ULONG ConnectionIndex, UCHAR DescriptorIndex, USHORT LanguageID)
{
	BOOL success;
	ULONG nBytes;
	ULONG nBytesReturned;

	UCHAR stringDescReqBuf[sizeof(USB_DESCRIPTOR_REQUEST)
			+ MAXIMUM_USB_STRING_LENGTH];

	PUSB_DESCRIPTOR_REQUEST stringDescReq;
	PUSB_STRING_DESCRIPTOR stringDesc;
	pusb_string_descriptor_entry_t stringDescNode;

	nBytes = sizeof(stringDescReqBuf);

	stringDescReq = (PUSB_DESCRIPTOR_REQUEST) stringDescReqBuf;
	stringDesc = (PUSB_STRING_DESCRIPTOR) (stringDescReq + 1);

	// Zero fill the entire request structure
	//
	memset(stringDescReq, 0, nBytes);

	// Indicate the port from which the descriptor will be requested
	//
	stringDescReq->ConnectionIndex = ConnectionIndex;

	//
	// USBHUB uses URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE to process this
	// IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION request.
	//
	// USBD will automatically initialize these fields:
	//     bmRequest = 0x80
	//     bRequest  = 0x06
	//
	// We must inititialize these fields:
	//     wValue    = Descriptor Type (high) and Descriptor Index (low byte)
	//     wIndex    = Zero (or Language ID for String Descriptors)
	//     wLength   = Length of descriptor buffer
	//
	stringDescReq->SetupPacket.wValue = (USB_STRING_DESCRIPTOR_TYPE << 8)
			| DescriptorIndex;

	stringDescReq->SetupPacket.wIndex = LanguageID;

	stringDescReq->SetupPacket.wLength = (USHORT) (nBytes
			- sizeof(USB_DESCRIPTOR_REQUEST));

	// Now issue the get descriptor request.
	//
	success = DeviceIoControl(hHubDevice,
			IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION, stringDescReq,
			nBytes, stringDescReq, nBytes, &nBytesReturned, NULL);

	//
	// Do some sanity checks on the return from the get descriptor request.
	//

	if (!success)
	{
		return NULL;
	}

	if (nBytesReturned < 2)
	{
		return NULL;
	}

	if (stringDesc->bDescriptorType != USB_STRING_DESCRIPTOR_TYPE)
	{
		return NULL;
	}

	if (stringDesc->bLength != nBytesReturned - sizeof(USB_DESCRIPTOR_REQUEST))
	{
		return NULL;
	}

	if (stringDesc->bLength % 2 != 0)
	{
		return NULL;
	}

	//
	// Looks good, allocate some (zero filled) space for the string descriptor
	// node and copy the string descriptor to it.
	//

	nBytes = sizeof(usb_string_descriptor_entry_t) + stringDesc->bLength;
	stringDescNode = (pusb_string_descriptor_entry_t) malloc(nBytes);
	if (stringDescNode == NULL)
	{
		return NULL;
	}

	// Zero initialize.
	memset(stringDescNode, 0, nBytes);

	// Initialize pointers
	stringDescNode->p_next = NULL;
	stringDescNode->index = DescriptorIndex;
	stringDescNode->language_id = LanguageID;

	// Copy string
	memcpy(stringDescNode->string_descriptor, stringDesc, stringDesc->bLength);

	return stringDescNode;
}

//*****************************************************************************
//
// GetStringDescriptors()
//
// hHubDevice - Handle of the hub device containing the port from which the
// String Descriptor will be requested.
//
// ConnectionIndex - Identifies the port on the hub to which a device is
// attached from which the String Descriptor will be requested.
//
// DescriptorIndex - String Descriptor index.
//
// NumLanguageIDs -  Number of languages in which the string should be
// requested.
//
// LanguageIDs - Languages in which the string should be requested.
//
//*****************************************************************************

static pusb_string_descriptor_entry_t GetStringDescriptors(HANDLE hHubDevice,
		ULONG ConnectionIndex, UCHAR DescriptorIndex, ULONG NumLanguageIDs,
		USHORT *LanguageIDs, pusb_string_descriptor_entry_t StringDescNodeTail)
{
	ULONG i;

	for (i = 0; i < NumLanguageIDs; i++)
	{
		StringDescNodeTail->p_next = GetStringDescriptor(hHubDevice,
				ConnectionIndex, DescriptorIndex, *LanguageIDs);

		if (StringDescNodeTail->p_next)
		{
			StringDescNodeTail = StringDescNodeTail->p_next;
		}

		LanguageIDs++;
	}

	return StringDescNodeTail;
}

//*****************************************************************************
//
// GetAllStringDescriptors()
//
// hHubDevice - Handle of the hub device containing the port from which the
// String Descriptors will be requested.
//
// ConnectionIndex - Identifies the port on the hub to which a device is
// attached from which the String Descriptors will be requested.
//
// DeviceDesc - Device Descriptor for which String Descriptors should be
// requested.
//
// ConfigDesc - Configuration Descriptor (also containing Interface Descriptor)
// for which String Descriptors should be requested.
//
//*****************************************************************************

static pusb_string_descriptor_entry_t GetAllStringDescriptors(HANDLE hHubDevice,
		ULONG ConnectionIndex, PUSB_DEVICE_DESCRIPTOR DeviceDesc,
		PUSB_CONFIGURATION_DESCRIPTOR ConfigDesc)
{
	pusb_string_descriptor_entry_t supportedLanguagesString;
	pusb_string_descriptor_entry_t stringDescNodeTail;
	ULONG numLanguageIDs;
	USHORT *languageIDs;

	PUCHAR descEnd;
	PUSB_COMMON_DESCRIPTOR commonDesc;

	//
	// Get the array of supported Language IDs, which is returned
	// in String Descriptor 0
	//
	supportedLanguagesString = GetStringDescriptor(hHubDevice, ConnectionIndex,
			0, 0);

	if (supportedLanguagesString == NULL)
	{
		return NULL;
	}

	numLanguageIDs = (supportedLanguagesString->string_descriptor->bLength - 2)
			/ 2;

	languageIDs = &supportedLanguagesString->string_descriptor->bString[0];

	stringDescNodeTail = supportedLanguagesString;

	//
	// Get the Device Descriptor strings
	//

	if (DeviceDesc->iManufacturer)
	{
		stringDescNodeTail = GetStringDescriptors(hHubDevice, ConnectionIndex,
				DeviceDesc->iManufacturer, numLanguageIDs, languageIDs,
				stringDescNodeTail);
	}

	if (DeviceDesc->iProduct)
	{
		stringDescNodeTail = GetStringDescriptors(hHubDevice, ConnectionIndex,
				DeviceDesc->iProduct, numLanguageIDs, languageIDs,
				stringDescNodeTail);
	}

	if (DeviceDesc->iSerialNumber)
	{
		stringDescNodeTail = GetStringDescriptors(hHubDevice, ConnectionIndex,
				DeviceDesc->iSerialNumber, numLanguageIDs, languageIDs,
				stringDescNodeTail);
	}

	//
	// Get the Configuration and Interface Descriptor strings
	//

	descEnd = (PUCHAR) ConfigDesc + ConfigDesc->wTotalLength;

	commonDesc = (PUSB_COMMON_DESCRIPTOR) ConfigDesc;

	while ((PUCHAR) commonDesc + sizeof(USB_COMMON_DESCRIPTOR) < descEnd
			&& (PUCHAR) commonDesc + commonDesc->bLength <= descEnd)
	{
		switch (commonDesc->bDescriptorType)
		{
		case USB_CONFIGURATION_DESCRIPTOR_TYPE:
			if (commonDesc->bLength != sizeof(USB_CONFIGURATION_DESCRIPTOR))
			{
				break;
			}
			if (((PUSB_CONFIGURATION_DESCRIPTOR) commonDesc)->iConfiguration)
			{
				stringDescNodeTail =
						GetStringDescriptors(
								hHubDevice,
								ConnectionIndex,
								((PUSB_CONFIGURATION_DESCRIPTOR) commonDesc)->iConfiguration,
								numLanguageIDs, languageIDs,
								stringDescNodeTail);
			}
			commonDesc = (PUSB_COMMON_DESCRIPTOR) ((PUCHAR) commonDesc
					+ commonDesc->bLength);
			continue;

		case USB_INTERFACE_DESCRIPTOR_TYPE:
			if (commonDesc->bLength != sizeof(USB_INTERFACE_DESCRIPTOR)/* &&
			 commonDesc->bLength != sizeof(USB_INTERFACE_DESCRIPTOR2)*/)
			{
				break;
			}
			if (((PUSB_INTERFACE_DESCRIPTOR) commonDesc)->iInterface)
			{
				stringDescNodeTail = GetStringDescriptors(hHubDevice,
						ConnectionIndex,
						((PUSB_INTERFACE_DESCRIPTOR) commonDesc)->iInterface,
						numLanguageIDs, languageIDs, stringDescNodeTail);
			}
			commonDesc = (PUSB_COMMON_DESCRIPTOR) ((PUCHAR) commonDesc
					+ commonDesc->bLength);
			continue;

		default:
			commonDesc = (PUSB_COMMON_DESCRIPTOR) ((PUCHAR) commonDesc
					+ commonDesc->bLength);
			continue;
		}
		break;
	}

	return supportedLanguagesString;
}

static bool usb_get_device_descriptor(HANDLE h_hub_device,
		size_t connection_index, uint8_t descriptor_index,
		uint8_t ** pp_descriptor, uint16_t * p_descriptor_length)
{
	bool success;
	size_t num_bytes;
	DWORD num_bytes_returned;

	PUSB_DESCRIPTOR_REQUEST p_device_desc_req;
	PUSB_DEVICE_DESCRIPTOR p_device_desc;

	// Request the Device Descriptor using an allocated buffer.
	//
	num_bytes = sizeof(USB_DESCRIPTOR_REQUEST) + sizeof(USB_DEVICE_DESCRIPTOR);

	p_device_desc_req = (PUSB_DESCRIPTOR_REQUEST) malloc(num_bytes);
	if (NULL == p_device_desc_req)
	{
		return false;
	}

	p_device_desc = (PUSB_DEVICE_DESCRIPTOR) (p_device_desc_req + 1);

	// Zero fill the entire request structure
	//
	memset(p_device_desc_req, 0, num_bytes);

	// Indicate the port from which the descriptor will be requested
	//
	p_device_desc_req->ConnectionIndex = connection_index;

	//
	// USBHUB uses URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE to process this
	// IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION request.
	//
	// USBD will automatically initialize these fields:
	//     bmRequest = 0x80
	//     bRequest  = 0x06
	//
	// We must initialize these fields:
	//     wValue    = Descriptor Type (high) and Descriptor Index (low byte)
	//     wIndex    = Zero (or Language ID for String Descriptors)
	//     wLength   = Length of descriptor buffer
	//
	p_device_desc_req->SetupPacket.wValue = (USB_DEVICE_DESCRIPTOR_TYPE << 8)
			| descriptor_index;

	p_device_desc_req->SetupPacket.wLength = (USHORT) (num_bytes
			- sizeof(USB_DESCRIPTOR_REQUEST));

	// Now issue the get descriptor request.
	//
	success = DeviceIoControl(h_hub_device,
			IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION, p_device_desc_req,
			num_bytes, p_device_desc_req, num_bytes, &num_bytes_returned, NULL);

	if (!success)
	{
		return false;
	}

	if (num_bytes != num_bytes_returned)
	{
		return false;
	}

	if (p_device_desc->bLength != sizeof(USB_DEVICE_DESCRIPTOR))
	{
		return false;
	}

	// Populate return values
	if (NULL != pp_descriptor)
	{
		*pp_descriptor = p_device_desc_req->Data;
	}
	else
	{
		free(p_device_desc_req);
	}

	if (NULL != p_descriptor_length)
	{
		*p_descriptor_length = num_bytes_returned
				- sizeof(USB_DESCRIPTOR_REQUEST);
	}

	return true;
}

static bool usb_get_config_descriptor(HANDLE h_hub_device,
		size_t connection_index, uint8_t descriptor_index,
		uint8_t ** pp_descriptor, uint16_t * p_descriptor_length)
{
	bool success;
	size_t num_bytes;
	ULONG num_bytes_returned;

	UCHAR config_desc_req_buf[sizeof(USB_DESCRIPTOR_REQUEST)
			+ sizeof(USB_CONFIGURATION_DESCRIPTOR)];

	PUSB_DESCRIPTOR_REQUEST p_config_desc_req;
	PUSB_CONFIGURATION_DESCRIPTOR p_config_desc;

	// Request the Configuration Descriptor the first time using our
	// local buffer, which is just big enough for the Configuration
	// Descriptor itself.
	//
	num_bytes = sizeof(config_desc_req_buf);

	p_config_desc_req = (PUSB_DESCRIPTOR_REQUEST) config_desc_req_buf;
	p_config_desc = (PUSB_CONFIGURATION_DESCRIPTOR) (p_config_desc_req + 1);

	// Zero fill the entire request structure
	//
	memset(p_config_desc_req, 0, num_bytes);

	// Indicate the port from which the descriptor will be requested
	//
	p_config_desc_req->ConnectionIndex = connection_index;

	//
	// USBHUB uses URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE to process this
	// IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION request.
	//
	// USBD will automatically initialize these fields:
	//     bmRequest = 0x80
	//     bRequest  = 0x06
	//
	// We must initialize these fields:
	//     wValue    = Descriptor Type (high) and Descriptor Index (low byte)
	//     wIndex    = Zero (or Language ID for String Descriptors)
	//     wLength   = Length of descriptor buffer
	//
	p_config_desc_req->SetupPacket.wValue = (USB_CONFIGURATION_DESCRIPTOR_TYPE
			<< 8) | descriptor_index;

	p_config_desc_req->SetupPacket.wLength = (USHORT) (num_bytes
			- sizeof(USB_DESCRIPTOR_REQUEST));

	// Now issue the get descriptor request.
	//
	success = DeviceIoControl(h_hub_device,
			IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION, p_config_desc_req,
			num_bytes, p_config_desc_req, num_bytes, &num_bytes_returned, NULL);

	if (!success)
	{
		return false;
	}

	if (num_bytes != num_bytes_returned)
	{
		return false;
	}

	if (p_config_desc->wTotalLength < sizeof(USB_CONFIGURATION_DESCRIPTOR))
	{
		return false;
	}

	// Now request the entire Configuration Descriptor using a dynamically
	// allocated buffer which is sized big enough to hold the entire descriptor
	//
	num_bytes = sizeof(USB_DESCRIPTOR_REQUEST) + p_config_desc->wTotalLength;

	p_config_desc_req = (PUSB_DESCRIPTOR_REQUEST) malloc(num_bytes);
	if (p_config_desc_req == NULL)
	{
		return false;
	}

	p_config_desc = (PUSB_CONFIGURATION_DESCRIPTOR) (p_config_desc_req + 1);

	// Indicate the port from which the descriptor will be requested
	//
	p_config_desc_req->ConnectionIndex = connection_index;

	//
	// USBHUB uses URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE to process this
	// IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION request.
	//
	// USBD will automatically initialize these fields:
	//     bmRequest = 0x80
	//     bRequest  = 0x06
	//
	// We must initialize these fields:
	//     wValue    = Descriptor Type (high) and Descriptor Index (low byte)
	//     wIndex    = Zero (or Language ID for String Descriptors)
	//     wLength   = Length of descriptor buffer
	//
	p_config_desc_req->SetupPacket.wValue = (USB_CONFIGURATION_DESCRIPTOR_TYPE
			<< 8) | descriptor_index;

	p_config_desc_req->SetupPacket.wLength = (USHORT) (num_bytes
			- sizeof(USB_DESCRIPTOR_REQUEST));

	// Now issue the get descriptor request.
	//
	success = DeviceIoControl(h_hub_device,
			IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION, p_config_desc_req,
			num_bytes, p_config_desc_req, num_bytes, &num_bytes_returned, NULL);

	if (!success)
	{
		free(p_config_desc_req);
		return false;
	}

	if (num_bytes != num_bytes_returned)
	{
		free(p_config_desc_req);
		return false;
	}

	if (p_config_desc->wTotalLength
			!= (num_bytes - sizeof(USB_DESCRIPTOR_REQUEST)))
	{
		free(p_config_desc_req);
		return false;
	}

	// Populate return values
	if (NULL != pp_descriptor)
	{
		*pp_descriptor = p_config_desc_req->Data;
	}
	else
	{
		free(p_config_desc_req);
	}

	if (NULL != p_descriptor_length)
	{
		*p_descriptor_length = num_bytes_returned
				- sizeof(USB_DESCRIPTOR_REQUEST);
	}

	return true;
}

static void usb_free_descriptor(uint8_t * p_descriptor)
{
	if (NULL != p_descriptor)
	{
		size_t tOffset;
		PUSB_DESCRIPTOR_REQUEST descReq;

		tOffset = offsetof(USB_DESCRIPTOR_REQUEST, Data);
		descReq = (PUSB_DESCRIPTOR_REQUEST) (p_descriptor - tOffset);
		free(descReq);
	}

	return;
}

static bool usb_get_descriptors_from_node(pusb_device_info_t p_device_info,
		size_t connection_node)
{
	bool success = FALSE;
	uint8_t * desc = NULL;
	uint16_t descLength = 0;

	// Get device descriptor from device
	success = usb_get_device_descriptor(p_device_info->h_hub, connection_node,
			0, &desc, &descLength);
	if (true == success)
	{
		pusb_configuration_descriptor_entry_t *p_configuration_node_tail =
				&p_device_info->p_configuration_descriptors;
		uint8_t config_num, num_configurations;

		// Copy contents of retrieved device descriptor
		p_device_info->device_descriptor = *((PUSB_DEVICE_DESCRIPTOR) desc);

		// Release device descriptor (we don't need it any more)
		usb_free_descriptor(desc); // device
		desc = NULL;

		// Load number of configurations available
		num_configurations =
				p_device_info->device_descriptor.bNumConfigurations;

		// Initialize configurations and strings
		*p_configuration_node_tail = NULL;

		// Iterate configurations
		for (config_num = 0; config_num < num_configurations; config_num++)
		{
			// Get configuration descriptor from device
			success = usb_get_config_descriptor(p_device_info->h_hub,
					connection_node, config_num, &desc, &descLength);
			if (true == success)
			{
				// Allocate memory for it
				*p_configuration_node_tail =
						(pusb_configuration_descriptor_entry_t) malloc(
								sizeof(usb_configuration_descriptor_entry_t)
										+ descLength);
				if (NULL != *p_configuration_node_tail)
				{
					pusb_configuration_descriptor_entry_t p_node =
							*p_configuration_node_tail;

					// Populate node...
					p_node->p_next = NULL;
					p_node->configuration_index = config_num;
					memcpy(p_node->configuration_descriptor, desc, descLength);

					// Now retrieve all the device string descriptors we can
					p_node->p_string_descriptors = GetAllStringDescriptors(
							p_device_info->h_hub, connection_node,
							&p_device_info->device_descriptor,
							(PUSB_CONFIGURATION_DESCRIPTOR) desc);

					// Prepare for next p_node
					p_configuration_node_tail = &(p_node->p_next);
				}

				// Release configuration descriptor (we don't need it any more)
				usb_free_descriptor(desc); // configuration
			}
		}
	}
	else
	{
		// Initialize device descriptor (nothing)
		memset(&p_device_info->device_descriptor, 0,
				sizeof(p_device_info->device_descriptor));
	}

	return success;
}

static bool usb_enumerate_ports(HANDLE h_hub, uint8_t num_ports,
		USB_ENUM_ITEM_CALLBACK enum_item_callback, void * p_enum_item_arg)
{
	bool status = false;
	uint8_t index;
	usb_enum_info_t info;
	pusb_device_info_t p_device = &info.u.device;

	// Loop over all ports of the hub.
	//
	// Port indices are 1 based, not 0 based.
	//
	for (index = 1; index <= num_ports; index++)
	{
		PUSB_NODE_CONNECTION_INFORMATION p_connection_info_ex =
				&p_device->connection_info;
		bool continue_enumeration = TRUE;

		// initialize structure
		memset(&info, 0, sizeof(usb_enum_info_t));

		// Set device item type
		info.type = USB_DEVICE;

		// Set handle
		p_device->h_hub = h_hub;

		// Get this hub's connection info
		GetConnectionInfoFromHub(p_device->h_hub, p_connection_info_ex, index);

		// Check if device if connected. If so gather what descriptors
		// we can from the device.
		if (p_connection_info_ex->ConnectionStatus == DeviceConnected)
		{
			status = usb_get_descriptors_from_node(p_device, index);
		}
		else
		{
			// all is well
			status = true;
		}

		// Signal enumerated item call-back
		if (NULL != enum_item_callback)
		{
			continue_enumeration = enum_item_callback(&info, p_enum_item_arg);
		}

		// Now we can free any configuration descriptors we may have acquired
		if (p_device->p_configuration_descriptors != NULL)
		{
			do
			{
				pusb_configuration_descriptor_entry_t p_next_node,
						p_config_node;

				// Point to top node
				p_config_node = p_device->p_configuration_descriptors;

				// First we can free any string descriptors we may have acquired
				if (p_config_node->p_string_descriptors != NULL)
				{
					do
					{
						pusb_string_descriptor_entry_t p_next_node,
								p_string_node;

						// Point to top node
						p_string_node = p_config_node->p_string_descriptors;

						// Remember next node
						p_next_node = p_string_node->p_next;

						// free node
						free(p_string_node);

						// Move next node to the top
						p_config_node->p_string_descriptors = p_next_node;

					} while (p_config_node->p_string_descriptors != NULL);
				}

				// Remember next node
				p_next_node = p_config_node->p_next;

				// free node
				free(p_config_node);

				// Move next node to the top
				p_device->p_configuration_descriptors = p_next_node;

			} while (p_device->p_configuration_descriptors != NULL);
		}

		// Is the caller done?
		if (continue_enumeration == false)
		{
			break;
		}

		// If the device connected to the port is an external hub, get the
		// name of the external hub and recursively enumerate it.
		//
		if (p_connection_info_ex->DeviceIsHub)
		{
			// Enumerate external associated with this port
			status = usb_enumerate_hubs(h_hub,
					p_connection_info_ex->ConnectionIndex, enum_item_callback,
					p_enum_item_arg);
		}

	}

	return status;
}

static bool usb_enumerate_hubs(HANDLE h_host_controller,
		size_t connection_index, USB_ENUM_ITEM_CALLBACK enum_item_callback,
		void * p_enum_item_arg)
{
	bool success;
	ULONG num_bytes;
	bool status = false;
	PTSTR p_device_name;
	size_t device_name_size;
	usb_enum_info_t info;
	pusb_hub_info_t p_hub;

	// initialize structure
	memset(&info, 0, sizeof(usb_enum_info_t));

	// Based on type, point to appropriate structure
	if (connection_index == 0)
	{
		// Set device item type
		info.type = USB_ROOT_HUB;

		p_hub = &info.u.root_hub;

		// Get the name of the root hub for this host
		// controller and then enumerate the root hub.
		//
		p_hub->p_hub_name = GetRootHubName(h_host_controller);

	}
	else
	{
		// Set device item type
		info.type = USB_EXTERNAL_HUB;

		p_hub = &info.u.external_hub.hub;

		// Get the name of the ext hub for this hub
		// and then enumerate the ext hub.
		//
		p_hub->p_hub_name = GetExternalHubName(h_host_controller,
				connection_index);
	}

	if (p_hub->p_hub_name == NULL)
	{
		// Error!
		return false;
	}

	// Allocate a temp buffer for the full hub device name.
	//
	device_name_size = _tcslen(p_hub->p_hub_name) + _tcslen(_T("\\\\.\\")) + 1;
	p_device_name = (PTSTR) malloc(device_name_size * sizeof(TCHAR));

	if (p_device_name == NULL)
	{
		goto EnumerateHubExit;
	}

	// Create the full hub device name
	//
	strncpy(p_device_name, _T("\\\\.\\"), device_name_size);
	strncat(p_device_name, p_hub->p_hub_name, device_name_size);

	// Try to hub the open device
	//
	p_hub->h_hub = CreateFile(p_device_name, GENERIC_WRITE, FILE_SHARE_WRITE,
			NULL, OPEN_EXISTING, 0, NULL);

	// Done with temp buffer for full hub device name
	//
	free(p_device_name);
	p_device_name = NULL;

	if (p_hub->h_hub == INVALID_HANDLE_VALUE)
	{
		goto EnumerateHubExit;
	}

	// USB_HUB_CAPABILITIES_EX is only available in Vista and later headers
#if (_WIN32_WINNT >= 0x0600)
	//
	// Now query USBHUB for the USB_HUB_CAPABILTIES_EX structure for this hub.
	//
	success = DeviceIoControl(p_hub->h_hub, IOCTL_USB_GET_HUB_CAPABILITIES_EX,
			&p_hub->HubCapsEx, sizeof(IOCTL_USB_GET_HUB_CAPABILITIES_EX),
			&p_hub->HubCapsEx, sizeof(IOCTL_USB_GET_HUB_CAPABILITIES_EX),
			&num_bytes, NULL);
#else
	//
	// Now query USBHUB for the USB_HUB_CAPABILTIES structure for this hub.
	//
	success = DeviceIoControl(p_hub->h_hub, IOCTL_USB_GET_HUB_CAPABILITIES,
			&p_hub->hub_caps, sizeof(USB_HUB_CAPABILITIES), &p_hub->hub_caps,
			sizeof(USB_HUB_CAPABILITIES), &num_bytes, NULL);
#endif

	if (!success)
	{
		goto EnumerateHubExit;
	}

	//
	// Now query USBHUB for the USB_NODE_INFORMATION structure for this hub.
	// This will tell us the number of downstream ports to enumerate, among
	// other things.
	//
	success = DeviceIoControl(p_hub->h_hub, IOCTL_USB_GET_NODE_INFORMATION,
			&p_hub->node_info, sizeof(USB_NODE_INFORMATION), &p_hub->node_info,
			sizeof(USB_NODE_INFORMATION), &num_bytes, NULL);

	if (!success)
	{
		goto EnumerateHubExit;
	}

	if (USB_EXTERNAL_HUB == info.type)
	{
		// Get this external hub's connection info
		GetConnectionInfoFromHub(p_hub->h_hub,
				&info.u.external_hub.device_info.connection_info,
				connection_index);
	}

	// Signal enumerated item call-back
	if (NULL != enum_item_callback)
	{
		bool cont;
		cont = enum_item_callback(&info, p_enum_item_arg);
		if (cont == true)
		{
			// Enumerate ports associated with this hub
			status =
					usb_enumerate_ports(
							p_hub->h_hub,
							p_hub->node_info.u.HubInformation.HubDescriptor.bNumberOfPorts,
							enum_item_callback, p_enum_item_arg);
		}
		else
		{
			status = true;
		}
	}

	EnumerateHubExit:

	if (p_hub->p_hub_name != NULL)
	{
		free(p_hub->p_hub_name);
		p_hub->p_hub_name = NULL;
	}
	if (p_hub->h_hub != INVALID_HANDLE_VALUE)
	{
		CloseHandle(p_hub->h_hub);
		p_hub->h_hub = INVALID_HANDLE_VALUE;
	}

	return status;
}

/*
 * Public Implementation
 */

bool usb_enumerate(USB_ENUM_ITEM_CALLBACK usb_enum_item_callback,
		void *p_usb_enum_item_callback_arg)
{
	HDEVINFO dev_info;
	bool status = false;

	// Check input
	if (NULL == usb_enum_item_callback)
	{
		return false;
	}

	// Iterate over host controllers using the GUID based interface
	//GUID_CLASS_USB_HOST_CONTROLLER

	dev_info = SetupDiGetClassDevs(
			(LPGUID) &GUID_DEVINTERFACE_USB_HOST_CONTROLLER, NULL, NULL,
			(DIGCF_PRESENT | DIGCF_DEVICEINTERFACE));

	if (dev_info != INVALID_HANDLE_VALUE)
	{
		SP_DEVICE_INTERFACE_DATA device_info_data;
		DWORD index;
		device_info_data.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

		for (index = 0;
				SetupDiEnumDeviceInterfaces(dev_info, 0,
						(LPGUID) &GUID_DEVINTERFACE_USB_HOST_CONTROLLER, index,
						&device_info_data); index++)
		{
			usb_enum_info_t enum_info;
			pusb_host_controller_info_t p_hc_info = &enum_info.u.host_controller;
			PSP_DEVICE_INTERFACE_DETAIL_DATA p_device_detail_data;
			DWORD length;

			// initialize structure
			memset(&enum_info, 0, sizeof(usb_enum_info_t));

			// Set device item type
			enum_info.type = USB_HOST_CONTROLLER;

			SetupDiGetDeviceInterfaceDetail(dev_info, &device_info_data, NULL,
					0, &length, NULL);

			p_device_detail_data = GlobalAlloc(GPTR, length);

			p_device_detail_data->cbSize =
					sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

			SetupDiGetDeviceInterfaceDetail(dev_info, &device_info_data,
					p_device_detail_data, length, &length, NULL);

			// Open the host controller
			p_hc_info->h_host_controller = CreateFile(
					p_device_detail_data->DevicePath, GENERIC_WRITE,
					FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);

			// If the handle is valid, then we've successfully opened a Host
			// Controller.  Display some info about the Host Controller itself,
			// then enumerate the Root Hub attached to the Host Controller.
			//
			if (p_hc_info->h_host_controller != INVALID_HANDLE_VALUE)
			{
				// Obtain the driver key name for this host controller.
				//
				p_hc_info->p_driver_key = GetHCDDriverKeyName(
						p_hc_info->h_host_controller);
				if (NULL != p_hc_info->p_driver_key)
				{
#if 0
					PTSTR deviceId;

					// Obtain the device id string for this host controller.
					// (Note: this a tmp global string buffer, make a copy of
					// this string if it will used later.)
					//
					deviceId = DriverNameToDeviceDesc(p_hc_info->p_driver_key, TRUE);

					if (deviceId)
					{
						ULONG ven = 0, dev = 0, subsys = 0, rev = 0;

						sscanf(deviceId,
								_T("PCI\\VEN_%lx&DEV_%lx&SUBSYS_%lx&REV_%lx"), &ven,
								&dev, &subsys, &rev);

						p_hc_info->VendorID = ven;
						p_hc_info->DeviceID = dev;
						p_hc_info->SubSysID = subsys;
						p_hc_info->Revision = rev;
					}
#endif
				}
				// Signal enumerated item call-back
				if (NULL != usb_enum_item_callback)
				{
					bool continue_enumerating;
					continue_enumerating = usb_enum_item_callback(&enum_info,
							p_usb_enum_item_callback_arg);
					if (continue_enumerating == true)
					{
						// Enumerate hubs associated with this controller
						status = usb_enumerate_hubs(
								p_hc_info->h_host_controller, 0 /*root*/,
								usb_enum_item_callback,
								p_usb_enum_item_callback_arg);
					}
					else
					{
						status = true;
					}
				}

				CloseHandle(p_hc_info->h_host_controller);
			}

			GlobalFree(p_device_detail_data);
		}

		SetupDiDestroyDeviceInfoList(dev_info);
	}

	return status;
}
