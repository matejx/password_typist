#include "k_descriptors.h"

const USB_Descriptor_HIDReport_Datatype_t PROGMEM KeyboardReport[] =
{
	HID_RI_USAGE_PAGE(8, 0x01), // Generic Desktop
	HID_RI_USAGE(8, 0x06), // Keyboard
	HID_RI_COLLECTION(8, 0x01), // Application
	HID_RI_USAGE_PAGE(8, 0x07), // Key Codes
	HID_RI_USAGE_MINIMUM(8, 0xE0), // Keyboard Left Control
	HID_RI_USAGE_MAXIMUM(8, 0xE7), // Keyboard Right GUI
	HID_RI_LOGICAL_MINIMUM(8, 0x00),
	HID_RI_LOGICAL_MAXIMUM(8, 0x01),
	HID_RI_REPORT_SIZE(8, 0x01),
	HID_RI_REPORT_COUNT(8, 0x08),
	HID_RI_INPUT(8, HID_IOF_DATA | HID_IOF_VARIABLE | HID_IOF_ABSOLUTE),
	HID_RI_REPORT_COUNT(8, 0x01),
	HID_RI_REPORT_SIZE(8, 0x08),
	HID_RI_INPUT(8, HID_IOF_CONSTANT),
	HID_RI_USAGE_PAGE(8, 0x08), // LEDs
	HID_RI_USAGE_MINIMUM(8, 0x01), // Num Lock
	HID_RI_USAGE_MAXIMUM(8, 0x05), // Kana
	HID_RI_REPORT_COUNT(8, 0x05),
	HID_RI_REPORT_SIZE(8, 0x01),
	HID_RI_OUTPUT(8, HID_IOF_DATA | HID_IOF_VARIABLE | HID_IOF_ABSOLUTE | HID_IOF_NON_VOLATILE),
	HID_RI_REPORT_COUNT(8, 0x01),
	HID_RI_REPORT_SIZE(8, 0x03),
	HID_RI_OUTPUT(8, HID_IOF_CONSTANT),
	HID_RI_LOGICAL_MINIMUM(8, 0x00),
	HID_RI_LOGICAL_MAXIMUM(8, 0x65),
	HID_RI_USAGE_PAGE(8, 0x07), // Keyboard
	HID_RI_USAGE_MINIMUM(8, 0x00), // Reserved (no event indicated)
	HID_RI_USAGE_MAXIMUM(8, 0x65), // Keyboard Application
	HID_RI_REPORT_COUNT(8, 0x06),
	HID_RI_REPORT_SIZE(8, 0x08),
	HID_RI_INPUT(8, HID_IOF_DATA | HID_IOF_ARRAY | HID_IOF_ABSOLUTE),
	HID_RI_END_COLLECTION(0),
};

const USB_Descriptor_Device_t PROGMEM k_DeviceDescriptor =
{
	.Header                 = {.Size = sizeof(USB_Descriptor_Device_t), .Type = DTYPE_Device},

	.USBSpecification       = VERSION_BCD(1,1,0),
	.Class                  = USB_CSCP_NoDeviceClass,
	.SubClass               = USB_CSCP_NoDeviceSubclass,
	.Protocol               = USB_CSCP_NoDeviceProtocol,

	.Endpoint0Size          = FIXED_CONTROL_ENDPOINT_SIZE,

	.VendorID               = 0x03EB,
	.ProductID              = 0x2042,
	.ReleaseNumber          = VERSION_BCD(0,0,1),

	.ManufacturerStrIndex   = STRING_ID_Manufacturer,
	.ProductStrIndex        = STRING_ID_Product,
	.SerialNumStrIndex      = NO_DESCRIPTOR,

	.NumberOfConfigurations = FIXED_NUM_CONFIGURATIONS
};

const USB_Descriptor_Configuration_t PROGMEM k_ConfigurationDescriptor =
{
	.Config =
		{
			.Header                 = {.Size = sizeof(USB_Descriptor_Configuration_Header_t), .Type = DTYPE_Configuration},

			.TotalConfigurationSize = sizeof(USB_Descriptor_Configuration_t),
			.TotalInterfaces        = 1,

			.ConfigurationNumber    = 1,
			.ConfigurationStrIndex  = NO_DESCRIPTOR,

			.ConfigAttributes       = (USB_CONFIG_ATTR_RESERVED | USB_CONFIG_ATTR_SELFPOWERED),

			.MaxPowerConsumption    = USB_CONFIG_POWER_MA(100)
		},

	.HID_Interface =
		{
			.Header                 = {.Size = sizeof(USB_Descriptor_Interface_t), .Type = DTYPE_Interface},

			.InterfaceNumber        = INTERFACE_ID_Keyboard,
			.AlternateSetting       = 0x00,

			.TotalEndpoints         = 2,

			.Class                  = HID_CSCP_HIDClass,
			.SubClass               = HID_CSCP_BootSubclass,
			.Protocol               = HID_CSCP_KeyboardBootProtocol,

			.InterfaceStrIndex      = NO_DESCRIPTOR
		},

	.HID_KeyboardHID =
		{
			.Header                 = {.Size = sizeof(USB_HID_Descriptor_HID_t), .Type = HID_DTYPE_HID},

			.HIDSpec                = VERSION_BCD(1,1,1),
			.CountryCode            = 0x00,
			.TotalReportDescriptors = 1,
			.HIDReportType          = HID_DTYPE_Report,
			.HIDReportLength        = sizeof(KeyboardReport)
		},

	.HID_ReportINEndpoint =
		{
			.Header                 = {.Size = sizeof(USB_Descriptor_Endpoint_t), .Type = DTYPE_Endpoint},

			.EndpointAddress        = KEYBOARD_IN_EPADDR,
			.Attributes             = (EP_TYPE_INTERRUPT | ENDPOINT_ATTR_NO_SYNC | ENDPOINT_USAGE_DATA),
			.EndpointSize           = KEYBOARD_EPSIZE,
			.PollingIntervalMS      = 0x05
		},

	.HID_ReportOUTEndpoint =
		{
			.Header                 = {.Size = sizeof(USB_Descriptor_Endpoint_t), .Type = DTYPE_Endpoint},

			.EndpointAddress        = KEYBOARD_OUT_EPADDR,
			.Attributes             = (EP_TYPE_INTERRUPT | ENDPOINT_ATTR_NO_SYNC | ENDPOINT_USAGE_DATA),
			.EndpointSize           = KEYBOARD_EPSIZE,
			.PollingIntervalMS      = 0x05
		}
};

const USB_Descriptor_String_t PROGMEM k_LanguageString = USB_STRING_DESCRIPTOR_ARRAY(LANGUAGE_ID_ENG);

const USB_Descriptor_String_t PROGMEM k_ManufacturerString = USB_STRING_DESCRIPTOR(L"Atmel");

const USB_Descriptor_String_t PROGMEM k_ProductString = USB_STRING_DESCRIPTOR(L"pwd keyboard");

uint16_t k_CALLBACK_USB_GetDescriptor(const uint16_t wValue, const uint16_t wIndex, const void** const DescriptorAddress)
{
	const uint8_t  DescriptorType   = (wValue >> 8);
	const uint8_t  DescriptorNumber = (wValue & 0xFF);

	const void* Address = NULL;
	uint16_t    Size    = NO_DESCRIPTOR;

	switch (DescriptorType)
	{
		case DTYPE_Device:
			Address = &k_DeviceDescriptor;
			Size    = sizeof(USB_Descriptor_Device_t);
			break;
		case DTYPE_Configuration:
			Address = &k_ConfigurationDescriptor;
			Size    = sizeof(USB_Descriptor_Configuration_t);
			break;
		case DTYPE_String:
			switch (DescriptorNumber)
			{
				case STRING_ID_Language:
					Address = &k_LanguageString;
					Size    = pgm_read_byte(&k_LanguageString.Header.Size);
					break;
				case STRING_ID_Manufacturer:
					Address = &k_ManufacturerString;
					Size    = pgm_read_byte(&k_ManufacturerString.Header.Size);
					break;
				case STRING_ID_Product:
					Address = &k_ProductString;
					Size    = pgm_read_byte(&k_ProductString.Header.Size);
					break;
			}

			break;
		case HID_DTYPE_HID:
			Address = &k_ConfigurationDescriptor.HID_KeyboardHID;
			Size    = sizeof(USB_HID_Descriptor_HID_t);
			break;
		case HID_DTYPE_Report:
			Address = &KeyboardReport;
			Size    = sizeof(KeyboardReport);
			break;
	}

	*DescriptorAddress = Address;
	return Size;
}
