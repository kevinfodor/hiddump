/*
 ==============================================================================
 Name        : usb_debug.c
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

// Standard includes
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

// Windows includes
#include <windows.h>

// WDK includes
#include <usbiodef.h>
#include <usbioctl.h>
#include <usb.h>
#include <usb100.h>
#include <hidusage.h>
#include <hidpi.h>
#include <hidsdi.h>

// Project includes
#include "utils.h"
#include "output.h"
#include "hexdump.h"
#include "usb_defs.h"
#include "usb_enum.h"
#include "usb_hid.h"

// Module include
#include "usb_debug.h"

// The maximum number of hubs that can be daisy chained is five. The USB root
// hub which is part of the USB controller will count as one, so you can
// connect a chain of a maximum of four external hubs to a USB port.
#define USB_MAX_NUM_HUBS    (1 + 4)

// Step level for each Hub/Port
#define STEP (2)

// Macro to determine the print level of a hub
#define LEVEL(hub_index) (((hub_index) + 1) * (2 * STEP) - STEP)

typedef struct _enum_print_struct
{
	// Control flags
	bool show_descriptors;

	// Statistics
	uint_least8_t num_host_controllers;
	uint_least8_t num_hubs, num_hubs_connected;
	uint_least8_t num_ports, num_ports_connected;

	// Print level control
	uint_least8_t hub_index; // Current hub index
	uint_least8_t hub_num_ports[USB_MAX_NUM_HUBS]; // Num ports on a hub

} enum_print_info_t, *penum_print_info_t;

// Local declarations
static const char * get_report_type_as_string(hid_report_type_t hid_report_type);

static void print_hid_data(phid_data_t const p_hid_data, uint32_t idx,
		uint32_t total);

static void print_hidp_button_caps(PHIDP_BUTTON_CAPS const p_hidp_button_caps,
		uint32_t idx, uint32_t total);

static void print_hidp_value_caps(PHIDP_VALUE_CAPS const p_hidp_value_caps,
		uint32_t idx, uint32_t total);

static void print_hid_strings(HANDLE h_device);

static void print_hidp_caps(PHIDP_CAPS const p_hidp_caps);

static void print_hidd_attributes(PHIDD_ATTRIBUTES const p_phidd_attributes);

static bool print_enum_callback(pusb_enum_info_t const p_usb_enum_info,
		void *pvArg);

static void print_hid_report_descriptor(
		pusb_hid_report_descriptor_t const p_descriptor);

static void print_hid_descriptor(pusb_hid_descriptor_t const p_descriptor);

static void print_endpoint_descriptor(
		PUSB_ENDPOINT_DESCRIPTOR const p_descriptor);

static void print_string_descriptor(PUSB_STRING_DESCRIPTOR const p_descriptor);

static void print_unknown_descriptor(PUSB_COMMON_DESCRIPTOR const p_descriptor);

static void print_interface_descriptor(
		PUSB_INTERFACE_DESCRIPTOR const p_descriptor);

static void print_config_descriptor(
		PUSB_CONFIGURATION_DESCRIPTOR const p_descriptor);

static void print_device_descriptor(PUSB_DEVICE_DESCRIPTOR const p_descriptor);

// Implementation

static const char * get_report_type_as_string(hid_report_type_t hid_report_type)
{
	const char * p_string = "Unknown";

	switch (hid_report_type)
	{
	case HID_REPORT_TYPE_INPUT:
		p_string = MACRO_TO_STRING(HID_REPORT_TYPE_INPUT);
		break;

	case HID_REPORT_TYPE_OUTPUT:
		p_string = MACRO_TO_STRING(HID_REPORT_TYPE_OUTPUT);
		break;

	case HID_REPORT_TYPE_FEATURE:
		p_string = MACRO_TO_STRING(HID_REPORT_TYPE_FEATURE);
		break;

	default:
		break;

	}

	return p_string;
}

static void print_hid_data(phid_data_t const p_hid_data, uint32_t idx,
		uint32_t total)
{
	HEADER_ARRAY("HID_DATA", idx, total);

	printf("Usage Page: 0x%x\n", p_hid_data->usage_page);
	printf("Report Id: %u\n", p_hid_data->report_id);

	if (p_hid_data->is_button)
	{
		puts("A Button:");
		printf("Usage Minimum: %u\n", p_hid_data->button.usage_min);
		printf("Usage Maximum: %u\n", p_hid_data->button.usage_max);
		printf("Max Usage Length: %u\n", p_hid_data->button.max_usage_length);

	}
	else
	{
		puts("A Value:");
		printf("Usage: %u\n", p_hid_data->value.usage);
		printf("Value: %u (0x%x)\n", p_hid_data->value.value,
				p_hid_data->value.value);
		printf("Scaled Value: %d\n", p_hid_data->value.scaled_value);

	}

	return;
}

static void print_hidp_button_caps(PHIDP_BUTTON_CAPS const p_hidp_button_caps,
		uint32_t idx, uint32_t total)
{
	HEADER_ARRAY("HIDP_BUTTON_CAPS", idx, total);

	printf("Usage Page: 0x%x\n", p_hidp_button_caps->UsagePage);
	printf("Report Id: %u\n", p_hidp_button_caps->ReportID);

	printf("Bit Field: %u\n", p_hidp_button_caps->BitField);

	if (p_hidp_button_caps->IsRange)
	{
		puts("Range:");
		printf("Usage Minimum: %u\n", p_hidp_button_caps->Range.UsageMin);
		printf("Usage Maximum: %u\n", p_hidp_button_caps->Range.UsageMax);
		printf("Data Index Minimum: %u\n",
				p_hidp_button_caps->Range.DataIndexMin);
		printf("Data Index Maximum: %u\n",
				p_hidp_button_caps->Range.DataIndexMax);
		printf("Designator Index Minimum: %u\n",
				p_hidp_button_caps->Range.DesignatorMin);
		printf("Designator Index Maximum: %u\n",
				p_hidp_button_caps->Range.DesignatorMax);
	}
	else
	{
		puts("NotRange:");
		printf("Usage: %u\n", p_hidp_button_caps->NotRange.Usage);
		printf("Data Index: %u\n", p_hidp_button_caps->NotRange.DataIndex);
		printf("Designator Index: %u\n",
				p_hidp_button_caps->NotRange.DesignatorIndex);

	}

	return;
}

static void print_hidp_value_caps(PHIDP_VALUE_CAPS const p_hidp_value_caps,
		uint32_t idx, uint32_t total)
{
	HEADER_ARRAY("HIDP_VALUE_CAPS", idx, total);

	printf("Usage Page: 0x%x\n", p_hidp_value_caps->UsagePage);
	printf("Report Id: %u\n", p_hidp_value_caps->ReportID);

	printf("Report Size (bits): %u\n", p_hidp_value_caps->BitSize);
	printf("Report Count: %u\n", p_hidp_value_caps->ReportCount);

	printf("Bit Field: %u\n", p_hidp_value_caps->BitField);

	printf("Logical Minimum: %ld\n", p_hidp_value_caps->LogicalMin);
	printf("Logical Maximum: %ld\n", p_hidp_value_caps->LogicalMax);
	printf("Physical Minimum: %ld\n", p_hidp_value_caps->PhysicalMin);
	printf("Physical Maximum: %ld\n", p_hidp_value_caps->PhysicalMax);

	if (p_hidp_value_caps->IsRange)
	{
		puts("Range:");
		printf("Usage Minimum: %u\n", p_hidp_value_caps->Range.UsageMin);
		printf("Usage Maximum: %u\n", p_hidp_value_caps->Range.UsageMax);
		printf("Data Index Minimum: %u\n",
				p_hidp_value_caps->Range.DataIndexMin);
		printf("Data Index Maximum: %u\n",
				p_hidp_value_caps->Range.DataIndexMax);
		printf("Designator Index Minimum: %u\n",
				p_hidp_value_caps->Range.DesignatorMin);
		printf("Designator Index Maximum: %u\n",
				p_hidp_value_caps->Range.DesignatorMax);
	}
	else
	{
		puts("NotRange:");
		printf("Usage: %u\n", p_hidp_value_caps->NotRange.Usage);
		printf("Data Index: %u\n", p_hidp_value_caps->NotRange.DataIndex);
		printf("Designator Index: %u\n",
				p_hidp_value_caps->NotRange.DesignatorIndex);

	}

	return;
}

static void print_hid_strings(HANDLE h_device)
{
	WCHAR wc_buffer[MAXIMUM_USB_STRING_LENGTH];
	char mbc_buffer[MAXIMUM_USB_STRING_LENGTH];
	int num_written;
	BOOL success;

	HEADER("HID_STRINGS");

	success = HidD_GetManufacturerString(h_device, wc_buffer,
			sizeof(wc_buffer));
	if (success == TRUE)
	{
		num_written = WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK, wc_buffer,
				wcslen(wc_buffer), mbc_buffer, sizeof(mbc_buffer), NULL, NULL);
		mbc_buffer[num_written] = '\0';
		printf("HidD_GetManufacturerString: '%s'\n", mbc_buffer);
	}

	success = HidD_GetProductString(h_device, wc_buffer, sizeof(wc_buffer));
	if (success == TRUE)
	{
		num_written = WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK, wc_buffer,
				wcslen(wc_buffer), mbc_buffer, sizeof(mbc_buffer), NULL, NULL);
		mbc_buffer[num_written] = '\0';
		printf("HidD_GetProductString: '%s'\n", mbc_buffer);
	}

	success = HidD_GetPhysicalDescriptor(h_device, wc_buffer,
			sizeof(wc_buffer));
	if (success == TRUE)
	{
		num_written = WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK, wc_buffer,
				wcslen(wc_buffer), mbc_buffer, sizeof(mbc_buffer), NULL, NULL);
		mbc_buffer[num_written] = '\0';
		printf("HidD_GetPhysicalDescriptor: '%s'\n", mbc_buffer);
	}

	success = HidD_GetSerialNumberString(h_device, wc_buffer,
			sizeof(wc_buffer));
	if (success == TRUE)
	{
		num_written = WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK, wc_buffer,
				wcslen(wc_buffer), mbc_buffer, sizeof(mbc_buffer), NULL, NULL);
		mbc_buffer[num_written] = '\0';
		printf("HidD_GetSerialNumberString: '%s'\n", mbc_buffer);
	}

	return;
}

static void print_hidp_caps(PHIDP_CAPS const p_hidp_caps)
{
	HEADER("HIDP_CAPS");

	printf("Usage Page: 0x%x\n", p_hidp_caps->UsagePage);
	printf("Usage: 0x%x\n", p_hidp_caps->Usage);

	printf("\nNumber of collection nodes %d:\n",
			p_hidp_caps->NumberLinkCollectionNodes);

	printf("\nInput report byte length: %d\n",
			p_hidp_caps->InputReportByteLength);
	printf("Number of input button capabilities: %d\n",
			p_hidp_caps->NumberInputButtonCaps);
	printf("Number of input value capabilities: %d\n",
			p_hidp_caps->NumberInputValueCaps);
	printf("Number of input data indices: %d\n",
			p_hidp_caps->NumberInputDataIndices);

	printf("\nOutput report byte length: %d\n",
			p_hidp_caps->OutputReportByteLength);
	printf("Number of output button capabilities: %d\n",
			p_hidp_caps->NumberOutputButtonCaps);
	printf("Number of output value capabilities: %d\n",
			p_hidp_caps->NumberOutputValueCaps);
	printf("Number of output data indices: %d\n",
			p_hidp_caps->NumberOutputDataIndices);

	printf("\nFeature report byte length: %d\n",
			p_hidp_caps->FeatureReportByteLength);
	printf("Number of feature button capabilities: %d\n",
			p_hidp_caps->NumberFeatureButtonCaps);
	printf("Number of feature value capabilities: %d\n",
			p_hidp_caps->NumberFeatureValueCaps);
	printf("Number of feature data indices: %d\n",
			p_hidp_caps->NumberFeatureDataIndices);

	return;
}

static void print_hidd_attributes(PHIDD_ATTRIBUTES const p_phidd_attributes)
{
	HEADER("HIDD_ATTRIBUTES");

	printf("Size: %lu\n", p_phidd_attributes->Size);
	printf("Vendor ID: 0x%0x\n", p_phidd_attributes->VendorID);
	printf("Product ID: 0x%0x\n", p_phidd_attributes->ProductID);
	printf("Version Number  0x%0x\n", p_phidd_attributes->VersionNumber);

	return;
}

// Enumerated Item Call-back
static bool print_enum_callback(pusb_enum_info_t const p_usb_enum_info,
		void *pvArg)
{
	bool continue_enumerating = true;
	penum_print_info_t p_enum_print_info = (penum_print_info_t) pvArg;

	// Count and print based on type
	switch (p_usb_enum_info->type)
	{
	case USB_HOST_CONTROLLER:
	{
		pusb_host_controller_info_t p_host_controller =
				&p_usb_enum_info->u.host_controller;

		// Increment statistics
		p_enum_print_info->num_host_controllers++;

		// Initialize output values
		p_enum_print_info->hub_index = 0;
		memset(p_enum_print_info->hub_num_ports, 0,
				sizeof(p_enum_print_info->hub_num_ports));

		puts("");
		printf("%*sHost Controller(#%u): %s\n", 0, "",
				p_enum_print_info->num_host_controllers,
				p_host_controller->p_driver_key);
	}
		break;
	case USB_ROOT_HUB:
	{
		pusb_root_hub_info_t p_root_hub = &p_usb_enum_info->u.root_hub;

		// Increment statistics
		p_enum_print_info->num_hubs++;
		p_enum_print_info->num_hubs_connected++;

		// Initialize output values
		p_enum_print_info->hub_index = 0;
		memset(p_enum_print_info->hub_num_ports, 0,
				sizeof(p_enum_print_info->hub_num_ports));

		// Save number of ports off this hub
		p_enum_print_info->hub_num_ports[p_enum_print_info->hub_index] =
				p_root_hub->node_info.u.HubInformation.HubDescriptor.bNumberOfPorts;

		printf(
				"%*sRoot Hub(%u ports): %s\n",
				LEVEL(p_enum_print_info->hub_index),
				"",
				p_root_hub->node_info.u.HubInformation.HubDescriptor.bNumberOfPorts,
				p_root_hub->p_hub_name);

	}
		break;
	case USB_EXTERNAL_HUB:
	{
		pusb_external_hub_info_t p_ext_hub = &p_usb_enum_info->u.external_hub;

		// Increment statistics
		p_enum_print_info->num_hubs++;

		// Increment hub index(next hub)
		if (p_enum_print_info->hub_index < (USB_MAX_NUM_HUBS - 1))
		{
			p_enum_print_info->hub_index++;
		}

		// Save number of ports off this external hub.
		p_enum_print_info->hub_num_ports[p_enum_print_info->hub_index] =
				p_ext_hub->hub.node_info.u.HubInformation.HubDescriptor.bNumberOfPorts;

		printf(
				"%*sExternal Hub(%u ports): %s\n",
				LEVEL(p_enum_print_info->hub_index),
				"",
				p_ext_hub->hub.node_info.u.HubInformation.HubDescriptor.bNumberOfPorts,
				p_ext_hub->hub.p_hub_name);
		if (p_ext_hub->device_info.connection_info.ConnectionStatus
				== DeviceConnected)
		{
			p_enum_print_info->num_hubs_connected++;
		}
	}
		break;
	case USB_DEVICE:
	{
		pusb_device_info_t p_device = &p_usb_enum_info->u.device;
		int level;

		// Increment statistics
		p_enum_print_info->num_ports++;

		// Find hub this port is on
		do
		{
			// Have we printed all ports part of the current hub?
			if (p_enum_print_info->hub_num_ports[p_enum_print_info->hub_index]
					== 0)
			{
				// Go back to previous hub
				if (p_enum_print_info->hub_index > 0)
				{
					p_enum_print_info->hub_index--;
				}
			}
			else
			{
				// Still on this hub, decrement the number of ports remaining
				if (p_enum_print_info->hub_num_ports[p_enum_print_info->hub_index]
						> 0)
				{
					p_enum_print_info->hub_num_ports[p_enum_print_info->hub_index]--;
				}
				break;
			}

		} while (true);

		// Current hub
		level = LEVEL(p_enum_print_info->hub_index) + STEP;

		printf("%*sPort(#%lu): ", level, "",
				p_device->connection_info.ConnectionIndex);

		if (p_device->connection_info.ConnectionStatus == DeviceConnected)
		{
			p_enum_print_info->num_ports_connected++;
			printf("VID=0x%0x, PID=0x%0x\n",
					p_device->connection_info.DeviceDescriptor.idVendor,
					p_device->connection_info.DeviceDescriptor.idProduct);

			// Show descriptors?
			if (true == p_enum_print_info->show_descriptors)
			{
				// If we have descriptors, print them
				usb_print_descriptors(
						(const unsigned char *) &p_device->device_descriptor,
						p_device->device_descriptor.bLength);

				// Now we can print any configuration descriptors we may have acquired
				if (p_device->p_configuration_descriptors != NULL)
				{
					pusb_configuration_descriptor_entry_t p_config_node =
							p_device->p_configuration_descriptors;

					do
					{
						usb_print_descriptors(
								(const unsigned char *) p_config_node->configuration_descriptor,
								p_config_node->configuration_descriptor->wTotalLength);

						// Now we can print any string descriptors we may have acquired
						if (p_config_node->p_string_descriptors != NULL)
						{
							pusb_string_descriptor_entry_t p_string_node =
									p_config_node->p_string_descriptors;

							do
							{
								usb_print_descriptors(
										(const unsigned char *) p_string_node->string_descriptor,
										p_string_node->string_descriptor->bLength);

								p_string_node = p_string_node->p_next;

							} while (p_string_node != NULL);
						}

						p_config_node = p_config_node->p_next;

					} while (p_config_node != NULL);
				}
			}
		}
		else
		{
			printf("NOT CONNECTED\n");
		}
	}
		break;
	default:
		printf("Error! Unknown USB device type.\n");
		continue_enumerating = false;
		break;
	}

	return continue_enumerating;
}

static void print_hid_report_descriptor(
		pusb_hid_report_descriptor_t const p_descriptor)
{
	uint16_t row = 0;

	HEADER("USB_HID_REPORT_DESCRIPTOR");

	printf("bLength:          0x%02x\n", p_descriptor->bLength);
	printf("bDescriptorType:  0x%02x\n", p_descriptor->bDescriptorType);

	hex_dump(stdout, &row, (uint8_t const *) p_descriptor,
			p_descriptor->bLength - sizeof(p_descriptor));

	return;
}

static void print_hid_descriptor(pusb_hid_descriptor_t const p_descriptor)
{
	uint8_t index;

	HEADER("USB_HID_DESCRIPTOR");

	printf("bLength:          0x%02x\n", p_descriptor->bLength);
	printf("bDescriptorType:  0x%02x\n", p_descriptor->bDescriptorType);
	printf("bcdHID:           0x%04X\n", p_descriptor->bcdHID);
	printf("bCountryCode:     0x%02X\n", p_descriptor->bCountryCode);
	printf("bNumDescriptors:  0x%02X\n", p_descriptor->bNumDescriptors);

	for (index = 0; index < p_descriptor->bNumDescriptors; index++)
	{
		printf("bDescriptorType:    0x%02X\n",
				p_descriptor->optional_descriptors[index].bDescriptorType);
		printf("wDescriptorLength:  0x%04X\n",
				p_descriptor->optional_descriptors[index].wDescriptorLength);
	}

	return;
}

static void print_endpoint_descriptor(
		PUSB_ENDPOINT_DESCRIPTOR const p_descriptor)
{
	HEADER("USB_ENDPOINT_DESCRIPTOR");

	printf("bLength:          0x%02x\n", p_descriptor->bLength);
	printf("bDescriptorType:  0x%02x\n", p_descriptor->bDescriptorType);
	printf("bEndpointAddress: 0x%02x\n", p_descriptor->bEndpointAddress);
	printf("bmAttributes:     0x%02x\n", p_descriptor->bmAttributes);
	printf("wMaxPacketSize:   0x%02x\n", p_descriptor->wMaxPacketSize);
	printf("bInterval:        0x%02x\n", p_descriptor->bInterval);
//	printf("bRefresh:         0x%02x\n", p_descriptor->bRefresh);
//	printf("bSynchAddress:    0x%02x\n", p_descriptor->bSynchAddress);

	return;
}

static void print_string_descriptor(PUSB_STRING_DESCRIPTOR const p_descriptor)
{
	size_t length;
	char buf[MAXIMUM_USB_STRING_LENGTH + 1];

	HEADER("USB_STRING_DESCRIPTOR");

	printf("bLength:          0x%02x\n", p_descriptor->bLength);
	printf("bDescriptorType:  0x%02x\n", p_descriptor->bDescriptorType);

	// convert wchar string to multi-byte ascii string
	length = p_descriptor->bLength - sizeof(USB_COMMON_DESCRIPTOR);
	length /= sizeof(WCHAR);
	length =
			length > MAXIMUM_USB_STRING_LENGTH ?
					MAXIMUM_USB_STRING_LENGTH : length;
	length = wcstombs(buf, p_descriptor->bString, length);
	if (((size_t) -1) == length)
	{
		strcpy(buf, "Error!");
	}
	else
	{
		buf[length] = '\0';
	}
	printf("bString:          %s\n", buf);

	if (((size_t) -1) == length)
	{
		uint16_t row = 0;

		// display raw-hex dump of string
		hex_dump(stdout, &row, (uint8_t const *) p_descriptor->bString,
				p_descriptor->bLength - sizeof(USB_COMMON_DESCRIPTOR));
	}
	return;
}

static void print_unknown_descriptor(PUSB_COMMON_DESCRIPTOR const p_descriptor)
{
	uint16_t row = 0;

	HEADER("USB_UNKNOWN_DESCRIPTOR");

	printf("bLength:         0x%02x\n", p_descriptor->bLength);
	printf("bDescriptorType: 0x%02x\n", p_descriptor->bDescriptorType);

	hex_dump(stdout, &row, (uint8_t const *) p_descriptor,
			p_descriptor->bLength - sizeof(p_descriptor));

	return;
}

static void print_interface_descriptor(
		PUSB_INTERFACE_DESCRIPTOR const p_descriptor)
{
	HEADER("USB_INTERFACE_DESCRIPTOR");

	printf("bLength:            0x%02x\n", p_descriptor->bLength);
	printf("bDescriptorType:    0x%02x\n", p_descriptor->bDescriptorType);
	printf("bInterfaceNumber:   0x%02x\n", p_descriptor->bInterfaceNumber);
	printf("bAlternateSetting:  0x%02x\n", p_descriptor->bAlternateSetting);
	printf("bNumEndpoints:      0x%02x\n", p_descriptor->bNumEndpoints);
	printf("bInterfaceClass:    0x%02x\n", p_descriptor->bInterfaceClass);
	printf("bInterfaceSubClass: 0x%02x\n", p_descriptor->bInterfaceSubClass);
	printf("bInterfaceProtocol: 0x%02x\n", p_descriptor->bInterfaceProtocol);
	printf("iInterface:         0x%02x\n", p_descriptor->iInterface);

	return;
}

static void print_config_descriptor(
		PUSB_CONFIGURATION_DESCRIPTOR const p_descriptor)
{
	HEADER("USB_CONFIG_DESCRIPTOR");

	printf("bLength:             0x%02x\n", p_descriptor->bLength);
	printf("bDescriptorType:     0x%02x\n", p_descriptor->bDescriptorType);
	printf("wTotalLength:        0x%02x\n", p_descriptor->wTotalLength);
	printf("bNumInterfaces:      0x%02x\n", p_descriptor->bNumInterfaces);
	printf("bConfigurationValue: 0x%02x\n", p_descriptor->bConfigurationValue);
	printf("iConfiguration:      0x%02x\n", p_descriptor->iConfiguration);
	printf("bmAttributes:        0x%02x\n", p_descriptor->bmAttributes);
	printf("MaxPower:            0x%02x\n", p_descriptor->MaxPower);

	return;
}

static void print_device_descriptor(PUSB_DEVICE_DESCRIPTOR const p_descriptor)
{
	HEADER("USB_DEVICE_DESCRIPTOR");

	printf("bLength:            0x%02x\n", p_descriptor->bLength);
	printf("bDescriptorType:    0x%02x\n", p_descriptor->bDescriptorType);
	printf("bcdUSB:             0x%02x\n", p_descriptor->bcdUSB);
	printf("bDeviceClass:       0x%02x\n", p_descriptor->bDeviceClass);
	printf("bDeviceSubClass:    0x%02x\n", p_descriptor->bDeviceSubClass);
	printf("bDeviceProtocol:    0x%02x\n", p_descriptor->bDeviceProtocol);
	printf("bMaxPacketSize0:    0x%02x\n", p_descriptor->bMaxPacketSize0);
	printf("idVendor:           0x%02x\n", p_descriptor->idVendor);
	printf("idProduct:          0x%02x\n", p_descriptor->idProduct);
	printf("bcdDevice:          0x%02x\n", p_descriptor->bcdDevice);
	printf("iManufacturer:      0x%02x\n", p_descriptor->iManufacturer);
	printf("iProduct:           0x%02x\n", p_descriptor->iProduct);
	printf("iSerialNumber:      0x%02x\n", p_descriptor->iSerialNumber);
	printf("bNumConfigurations: 0x%02x\n", p_descriptor->bNumConfigurations);

	return;
}

void usb_print_hid_device(phid_device_t const p_device)
{
	uint32_t index;
	hid_report_type_t hid_report_type;

	HEADER("HID_DEVICE");

	print_hid_strings(p_device->h_device);
	print_hidd_attributes(&p_device->attributes);
	print_hidp_caps(&p_device->caps);

	for (hid_report_type = HID_REPORT_TYPE_FIRST;
			hid_report_type < HID_REPORT_TYPE_SIZE; hid_report_type++)
	{
		phid_report_t p_report = &p_device->report[hid_report_type];
		PHIDP_VALUE_CAPS p_value_caps;
		PHIDP_BUTTON_CAPS p_button_caps;
		phid_data_t p_hid_data;

		printf("\n%s\n\n", get_report_type_as_string(hid_report_type));

		p_button_caps = p_report->p_button_caps;
		for (index = 0; index < p_report->number_button_caps; index++)
		{
			print_hidp_button_caps(p_button_caps, index + 1,
					p_report->number_button_caps);
			p_button_caps++;
		}

		p_value_caps = p_report->p_value_caps;
		for (index = 0; index < p_report->number_value_caps; index++)
		{
			print_hidp_value_caps(p_value_caps, index + 1,
					p_report->number_value_caps);
			p_value_caps++;
		}

		p_hid_data = p_report->p_hid_data;
		for (index = 0; index < p_report->hid_data_length; index++)
		{
			print_hid_data(p_hid_data, index + 1, p_report->hid_data_length);
			p_hid_data++;
		}
	}

	return;
}

void usb_print_hid_report(phid_data_t const pData, char * p_buffer,
		size_t buffer_size)
{
	PCHAR pszWalk;
	PUSAGE pUsage;
	ULONG i;
	UINT iRemainingBuffer;
	UINT iStringLength;
	UINT j;

	//
	// For button data, all the usages in the usage list are to be displayed
	//

	if (pData->is_button)
	{
		if (FAILED(_snprintf (p_buffer,
						buffer_size,
						"Usage Page: 0x%x, Usages: ",
						pData->usage_page)))
		{
			for (j = 0; j < sizeof(p_buffer); j++)
			{
				p_buffer[j] = '\0';
			}
			return; // error case
		}

		iRemainingBuffer = 0;
		iStringLength = (UINT) strlen(p_buffer);
		pszWalk = p_buffer + iStringLength;
		if (iStringLength < buffer_size)
		{
			iRemainingBuffer = buffer_size - iStringLength;
		}

		for (i = 0, pUsage = pData->button.p_usages;
				i < pData->button.max_usage_length; i++, pUsage++)
		{
			if (0 == *pUsage)
			{
				break; // A usage of zero is a non button.
			}
			if (FAILED(_snprintf (pszWalk, iRemainingBuffer, " 0x%x", *pUsage)))
			{
				return; // error case
			}
			iRemainingBuffer -= (UINT) strlen(pszWalk);
			pszWalk += strlen(pszWalk);
		}
	}
	else
	{
		if (FAILED(_snprintf (p_buffer,
						buffer_size,
						"Usage Page: 0x%x, Usage: 0x%x, Scaled: %d \tValue: %lu \t(0x%x)",
						pData->usage_page,
						pData->value.usage,
						pData->value.scaled_value,
						pData->value.value, pData->value.value)))
		{
			return; // error case
		}
	}
}

void usb_print_descriptors(unsigned char const * p_data, size_t data_length)
{
	const USB_COMMON_DESCRIPTOR *p_header =
			(const USB_COMMON_DESCRIPTOR *) p_data;

	while (data_length)
	{
		// Decode descriptor based on type
		switch (p_header->bDescriptorType)
		{
		case USB_DEVICE_DESCRIPTOR_TYPE:
		{
			PUSB_DEVICE_DESCRIPTOR const p_descriptor =
					(PUSB_DEVICE_DESCRIPTOR const) p_header;
			size_t length = sizeof(USB_DEVICE_DESCRIPTOR);

			if (p_header->bLength == length)
			{
				print_device_descriptor(p_descriptor);
			}
		}
			break;
		case USB_CONFIGURATION_DESCRIPTOR_TYPE:
		{
			PUSB_CONFIGURATION_DESCRIPTOR p_descriptor =
					(PUSB_CONFIGURATION_DESCRIPTOR) p_header;
			size_t length = sizeof(USB_CONFIGURATION_DESCRIPTOR);

			if (p_header->bLength == length)
			{
				print_config_descriptor(p_descriptor);
			}
		}
			break;
		case USB_STRING_DESCRIPTOR_TYPE:
		{
			PUSB_STRING_DESCRIPTOR p_descriptor =
					(PUSB_STRING_DESCRIPTOR) p_header;
			size_t length = sizeof(USB_COMMON_DESCRIPTOR);

			if (p_header->bLength >= length)
			{
				print_string_descriptor(p_descriptor);
			}
			else
			{
				printf("Error! Invalid descriptor length of %u for type."
						" %u or greater was expected.\n", p_header->bLength,
						length);
			}
		}
			break;
		case USB_INTERFACE_DESCRIPTOR_TYPE:
		{
			PUSB_INTERFACE_DESCRIPTOR p_descriptor =
					(PUSB_INTERFACE_DESCRIPTOR) p_header;
			size_t length = sizeof(USB_INTERFACE_DESCRIPTOR);

			if (p_header->bLength == length)
			{
				print_interface_descriptor(p_descriptor);
			}
		}
			break;
		case USB_ENDPOINT_DESCRIPTOR_TYPE:
		{
			PUSB_ENDPOINT_DESCRIPTOR p_descriptor =
					(PUSB_ENDPOINT_DESCRIPTOR) p_header;
			size_t length = sizeof(USB_ENDPOINT_DESCRIPTOR);

			if (p_header->bLength == length)
			{
				print_endpoint_descriptor(p_descriptor);
			}
		}
			break;
		case USB_HID_DESCRIPTOR_TYPE:
		{
			pusb_hid_descriptor_t p_descriptor =
					(pusb_hid_descriptor_t) p_header;
			size_t length = sizeof(usb_hid_descriptor_t);

			if (p_header->bLength == length)
			{
				print_hid_descriptor(p_descriptor);
			}
			else
			{
				printf("Error! Invalid descriptor length of %u for type."
						" %u was expected.\n", p_header->bLength, length);
			}
		}
			break;
		case USB_HID_REPORT_DESCRIPTOR_TYPE:
		{
			pusb_hid_report_descriptor_t p_descriptor =
					(pusb_hid_report_descriptor_t) p_header;
			size_t length = sizeof(USB_COMMON_DESCRIPTOR);

			if (p_header->bLength >= length)
			{
				print_hid_report_descriptor(p_descriptor);
			}
			else
			{
				printf("Error! Invalid descriptor length of %u for type."
						" %u or greater was expected.\n", p_header->bLength,
						length);
			}
		}
			break;
		default:
		{
			PUSB_COMMON_DESCRIPTOR p_descriptor =
					(PUSB_COMMON_DESCRIPTOR) p_header;
			print_unknown_descriptor(p_descriptor);
		}
			break;
		}

		// Move on to the next descriptor
		if (data_length >= p_header->bLength)
		{
			data_length -= p_header->bLength;
			p_header = (const USB_COMMON_DESCRIPTOR *) ((const PUCHAR) p_header
					+ p_header->bLength);
		}
		else
		{
			break;
		}
	}

	return;
}

/*
 *
 */
void usb_print_enumeration(bool show_descriptors)
{
	bool success;
	enum_print_info_t enum_print_info;

	// Initialize enumeration data
	memset(&enum_print_info, 0, sizeof(enum_print_info));

	printf("\nEnumerating USB controllers and devices...\n");

	success = usb_enumerate(print_enum_callback, &enum_print_info);
	if (success == true)
	{
		printf("\nEnumerated - Controllers %u, Hubs %u, Ports %u.\n"
				"\tPorts Connected %u, Hubs Connected %u\n",
				enum_print_info.num_host_controllers, enum_print_info.num_hubs,
				enum_print_info.num_ports, enum_print_info.num_ports_connected,
				enum_print_info.num_hubs_connected);
	}
	else
	{
		printf("Error! Could not enumerate USB.\n");
	}

	return;
}
