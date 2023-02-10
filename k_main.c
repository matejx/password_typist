#include <avr/eeprom.h>
#include <avr/wdt.h>

#include "k_descriptors.h"
#include "main.h"

#include <LUFA/Drivers/USB/USB.h>

static bool UsingReportProtocol = true; // meaningless (desc declared report proto = boot proto)

static uint16_t idle_rate = 500;
static uint16_t idle_cnt = 0;
static uint16_t sof_cnt = 0;

void rep_size_check(void)
{
	switch(0) {case 0:case sizeof(USB_KeyboardReport_Data_t) == 8:;}
}

// index of char in string or 0xff if not found
uint8_t istrchr(const char* s, const char c)
{
	uint8_t i = 0;

	while( *s ) {
		if( *s == c ) return i;
		++i;
		++s;
	}

	return 0xff;
}

// char to keyboard scan code
uint8_t c2ksc(const char c, uint8_t* ksc, uint8_t* mod)
{
	if( (c < 32) || (c > 126) ) return 0;

	if( (c >= 'a') && (c <= 'z') ) {
		*ksc = HID_KEYBOARD_SC_A + (c - 'a');
		*mod = 0;
		return 1;
	}

	if( (c >= 'A') && (c <= 'Z') ) {
		*ksc = HID_KEYBOARD_SC_A + (c - 'A');
		*mod = HID_KEYBOARD_MODIFIER_LEFTSHIFT;
		return 1;
	}

	uint8_t i;

	const char* nums = "1234567890!\"#$%&/()=";
	i = istrchr(nums, c);
	if( i < strlen(nums) ) {
		*ksc = HID_KEYBOARD_SC_1_AND_EXCLAMATION + (i % 10);
		*mod = (i < 10) ? 0 : HID_KEYBOARD_MODIFIER_LEFTSHIFT;
		return 1;
	}

	// Warning: caret ^, tilde ~ and accent ` not supported
	const char* spec = " '?+*,;<.:>-_\\|[]{}@";
	const uint8_t specksc[20] = {0x2c,0x2d,0x2d,0x2e,0x2e,0x36,0x36,0x36,0x37,0x37,0x37,0x38,0x38,0x14,0x1a,0x09,0x0a,0x05,0x11,0x19};
	const uint8_t specmod[20] = {0   ,0   ,2   ,0   ,2   ,0   ,2   ,64  ,0   ,2   ,64  ,0   ,2   ,64  ,64  ,64  ,64  ,64  ,64  ,64  };
	i = istrchr(spec, c);
	if( i < strlen(spec) ) {
		*ksc = specksc[i];
		*mod = specmod[i];
		return 1;
	}

	return 0;
}

// Fills the given HID report data structure with the next HID report to send to the host.
void CreateKeyboardReport(USB_KeyboardReport_Data_t* const rep)
{
	static uint8_t i = 0;
	static uint8_t odd = 1;

	odd = !odd;
	if( odd ) return;

	if( i >= PWD_SIZE ) return;

	uint8_t c = eeprom_read_byte((void*)(PWD_SIZE * getswi() + i));
	uint8_t ksc, mod;
	if( c2ksc(c, &ksc, &mod) ) {
		rep->Modifier = mod;
		rep->KeyCode[0] = ksc;
		++i;
	}
}

// Processes a received LED report, and updates the board LEDs states to match.
void ProcessLEDReport(const uint8_t LEDReport)
{
}

// Sends the next HID report to the host, via the keyboard data endpoint.
void SendNextReport(void)
{
	// Select the Keyboard Report Endpoint
	Endpoint_SelectEndpoint(KEYBOARD_IN_EPADDR);

	// Check if Keyboard Endpoint Ready for Read/Write and if we should send a new report
	if (Endpoint_IsReadWriteAllowed())
	{
		static USB_KeyboardReport_Data_t prev;

		USB_KeyboardReport_Data_t rep;
		memset(&rep, 0, sizeof(USB_KeyboardReport_Data_t));
		if( sof_cnt > 1000 ) { CreateKeyboardReport(&rep); }

		if( 0 == memcmp(&prev, &rep, sizeof(USB_KeyboardReport_Data_t)) ) {
			if( idle_cnt ) return;
		}
		idle_cnt = idle_rate;
		memcpy(&prev, &rep, sizeof(USB_KeyboardReport_Data_t));

		// Write Keyboard Report Data
		Endpoint_Write_Stream_LE(&rep, sizeof(USB_KeyboardReport_Data_t), NULL);

		// Finalize the stream transfer to send the last packet
		Endpoint_ClearIN();
	}
}

// Reads the next LED status report from the host from the LED data endpoint, if one has been sent.
void ReceiveNextReport(void)
{
	// Select the Keyboard LED Report Endpoint
	Endpoint_SelectEndpoint(KEYBOARD_OUT_EPADDR);

	// Check if Keyboard LED Endpoint contains a packet
	if (Endpoint_IsOUTReceived())
	{
		// Check to see if the packet contains data
		if (Endpoint_IsReadWriteAllowed())
		{
			// Read in the LED report from the host
			uint8_t LEDReport = Endpoint_Read_8();

			// Process the read LED report from the host
			ProcessLEDReport(LEDReport);
		}

		// Handshake the OUT Endpoint - clear endpoint and ready for next report
		Endpoint_ClearOUT();
	}
}

// Function to manage HID report generation and transmission to the host, when in report mode.
void HID_Task(void)
{
	// Device must be connected and configured for the task to run
	if (USB_DeviceState != DEVICE_STATE_Configured) return;

	// Send the next keypress report to the host
	SendNextReport();

	// Process the LED report sent from the host
	ReceiveNextReport();
}

/* Event handler for the USB_ConfigurationChanged event. This is fired when the host sets the current configuration
	of the USB device after enumeration, and configures the keyboard device endpoints. */
void k_EVENT_USB_Device_ConfigurationChanged(void)
{
	bool ConfigSuccess = true;

	// Setup HID Report Endpoints
	ConfigSuccess &= Endpoint_ConfigureEndpoint(KEYBOARD_IN_EPADDR, EP_TYPE_INTERRUPT, KEYBOARD_EPSIZE, 1);
	ConfigSuccess &= Endpoint_ConfigureEndpoint(KEYBOARD_OUT_EPADDR, EP_TYPE_INTERRUPT, KEYBOARD_EPSIZE, 1);

	// Turn on Start-of-Frame events for tracking HID report period expiry
	USB_Device_EnableSOFEvents();
}

/* Event handler for the USB_ControlRequest event. This is used to catch and process control requests sent to
	the device from the USB host before passing along unhandled control requests to the library for processing
	internally. */
void k_EVENT_USB_Device_ControlRequest(void)
{
	// Handle HID Class specific requests
	switch (USB_ControlRequest.bRequest)
	{
		case HID_REQ_GetReport:
			if (USB_ControlRequest.bmRequestType == (REQDIR_DEVICETOHOST | REQTYPE_CLASS | REQREC_INTERFACE))
			{
				USB_KeyboardReport_Data_t rep;
				memset(&rep, 0, sizeof(USB_KeyboardReport_Data_t));

				CreateKeyboardReport(&rep);

				Endpoint_ClearSETUP();

				// Write the report data to the control endpoint
				Endpoint_Write_Control_Stream_LE(&rep, sizeof(USB_KeyboardReport_Data_t));
				Endpoint_ClearOUT();
			}

			break;
		case HID_REQ_SetReport:
			if (USB_ControlRequest.bmRequestType == (REQDIR_HOSTTODEVICE | REQTYPE_CLASS | REQREC_INTERFACE))
			{
				Endpoint_ClearSETUP();

				// Wait until the LED report has been sent by the host
				while (!(Endpoint_IsOUTReceived()))
				{
					if (USB_DeviceState == DEVICE_STATE_Unattached)
					  return;
				}

				// Read in the LED report from the host
				uint8_t LEDStatus = Endpoint_Read_8();

				Endpoint_ClearOUT();
				Endpoint_ClearStatusStage();

				// Process the incoming LED report
				ProcessLEDReport(LEDStatus);
			}

			break;
		case HID_REQ_GetProtocol:
			if (USB_ControlRequest.bmRequestType == (REQDIR_DEVICETOHOST | REQTYPE_CLASS | REQREC_INTERFACE))
			{
				Endpoint_ClearSETUP();

				// Write the current protocol flag to the host
				Endpoint_Write_8(UsingReportProtocol);

				Endpoint_ClearIN();
				Endpoint_ClearStatusStage();
			}

			break;
		case HID_REQ_SetProtocol:
			if (USB_ControlRequest.bmRequestType == (REQDIR_HOSTTODEVICE | REQTYPE_CLASS | REQREC_INTERFACE))
			{
				Endpoint_ClearSETUP();
				Endpoint_ClearStatusStage();

				// Set or clear the flag depending on what the host indicates that the current Protocol should be
				UsingReportProtocol = (USB_ControlRequest.wValue != 0);
			}

			break;
		case HID_REQ_SetIdle:
			if (USB_ControlRequest.bmRequestType == (REQDIR_HOSTTODEVICE | REQTYPE_CLASS | REQREC_INTERFACE))
			{
				Endpoint_ClearSETUP();
				Endpoint_ClearStatusStage();

				// Get idle period in MSB, idle_rate must be multiplied by 4 to get number of milliseconds
				idle_rate = ((USB_ControlRequest.wValue & 0xFF00) >> 6);
			}

			break;
		case HID_REQ_GetIdle:
			if (USB_ControlRequest.bmRequestType == (REQDIR_DEVICETOHOST | REQTYPE_CLASS | REQREC_INTERFACE))
			{
				Endpoint_ClearSETUP();

				// Write the current idle duration to the host, must be divided by 4 before sent to host
				Endpoint_Write_8(idle_rate >> 2);

				Endpoint_ClearIN();
				Endpoint_ClearStatusStage();
			}

			break;
	}
}

// Event handler for the USB device Start Of Frame event.
void EVENT_USB_Device_StartOfFrame(void)
{
	++sof_cnt;
	if (idle_cnt) --idle_cnt;
}

int k_main(void)
{
	USB_Init();
	sei();

	while( 1 ) {
		wdt_reset();
		HID_Task();
		USB_USBTask();
	}
}
