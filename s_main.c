#include <avr/io.h>
#include <avr/wdt.h>

#include "s_descriptors.h"
#include "circbuf8.h"
#include "main.h"

#include <LUFA/Drivers/USB/USB.h>

uint8_t txbuf[64];
uint8_t rxbuf[64];
static volatile struct cbuf8_t cdc_rxq;
static volatile struct cbuf8_t cdc_txq;

static CDC_LineEncoding_t LineEncoding = {
	.BaudRateBPS = 0,
	.CharFormat  = CDC_LINEENCODING_OneStopBit,
	.ParityType  = CDC_PARITY_None,
	.DataBits    = 8
};

uint8_t ptoi(const char c)
{
	if( c >= '0' && c <= '9' ) return c - '0';
	if( c >= 'a' && c <= 'z' ) return c - 'a' + 10;

	return 0;
}

char itop(const uint8_t a)
{
	if( a < 10 ) return ('0' + a);
	if( a < 36 ) return ('a' + a - 10);

	return '?';
}

/* Event handler for the USB_ConfigurationChanged event. This is fired when the
	host set the current configuration of the USB device after enumeration - the
	device endpoints are configured and the CDC management task started. */
void s_EVENT_USB_Device_ConfigurationChanged(void)
{
	bool ConfigSuccess = true;

	// Setup CDC Data Endpoints
	ConfigSuccess &= Endpoint_ConfigureEndpoint(CDC_NOTIFICATION_EPADDR, EP_TYPE_INTERRUPT, CDC_NOTIFICATION_EPSIZE, 1);
	ConfigSuccess &= Endpoint_ConfigureEndpoint(CDC_TX_EPADDR, EP_TYPE_BULK, CDC_TXRX_EPSIZE, 1);
	ConfigSuccess &= Endpoint_ConfigureEndpoint(CDC_RX_EPADDR, EP_TYPE_BULK,  CDC_TXRX_EPSIZE, 1);

	// Reset line encoding baud rate so that the host knows to send new values
	LineEncoding.BaudRateBPS = 0;
}

/* Event handler for the USB_ControlRequest event. This is used to catch and
	process control requests sent to the device from the USB host before passing
	along unhandled control requests to the library for processing internally. */
void s_EVENT_USB_Device_ControlRequest(void)
{
	// Process CDC specific control requests
	switch (USB_ControlRequest.bRequest)
	{
		case CDC_REQ_GetLineEncoding:
			if (USB_ControlRequest.bmRequestType == (REQDIR_DEVICETOHOST | REQTYPE_CLASS | REQREC_INTERFACE))
			{
				Endpoint_ClearSETUP();

				// Write the line coding data to the control endpoint
				Endpoint_Write_Control_Stream_LE(&LineEncoding, sizeof(CDC_LineEncoding_t));
				Endpoint_ClearOUT();
			}

			break;
		case CDC_REQ_SetLineEncoding:
			if (USB_ControlRequest.bmRequestType == (REQDIR_HOSTTODEVICE | REQTYPE_CLASS | REQREC_INTERFACE))
			{
				Endpoint_ClearSETUP();

				// Read the line coding data in from the host into the global struct
				Endpoint_Read_Control_Stream_LE(&LineEncoding, sizeof(CDC_LineEncoding_t));
				Endpoint_ClearIN();
			}

			break;
		case CDC_REQ_SetControlLineState:
			if (USB_ControlRequest.bmRequestType == (REQDIR_HOSTTODEVICE | REQTYPE_CLASS | REQREC_INTERFACE))
			{
				Endpoint_ClearSETUP();
				Endpoint_ClearStatusStage();

				/* NOTE: Here you can read in the line state mask from the host, to get
					the current state of the output handshake lines. The mask is read in
					from the wValue parameter in USB_ControlRequest, and can be masked
					against the CONTROL_LINE_OUT_* masks to determine the RTS and DTR line
					states using the following code: */
			}

			break;
	}
}

// Function to manage CDC data transmission and reception to and from the host.
void CDC_Task(void)
{
	if (USB_DeviceState != DEVICE_STATE_Configured) return;

	uint8_t s[CDC_TXRX_EPSIZE];
	uint8_t i, d;
	for( i = 0; i < sizeof(s); ++i ) {
	  if( !cbuf8_get(&cdc_txq, &d) ) break;
		s[i] = d;
	}

	if( i && LineEncoding.BaudRateBPS ) {
		Endpoint_SelectEndpoint(CDC_TX_EPADDR);
		Endpoint_Write_Stream_LE(s, i, NULL);
		bool IsFull = (Endpoint_BytesInEndpoint() == CDC_TXRX_EPSIZE);
		Endpoint_ClearIN();
		if( IsFull ) {
			Endpoint_WaitUntilReady();
			Endpoint_ClearIN();
		}
	}

	Endpoint_SelectEndpoint(CDC_RX_EPADDR);
	if( Endpoint_IsOUTReceived() ) {
		d = Endpoint_BytesInEndpoint();
		if( d > sizeof(s) ) d = sizeof(s);
		Endpoint_Read_Stream_LE(s, d, NULL);
		Endpoint_ClearOUT();
		for( i = 0; i < d; ++i ) cbuf8_put(&cdc_rxq, s[i]);
	}
}

void Serial_SendByte(uint8_t a)
{
	cbuf8_put(&cdc_txq, a);
}

void Serial_SendString(const char* s)
{
	while( *s ) {
		Serial_SendByte(*s);
		++s;
	}
}

void Ser_Task(void)
{
	static uint8_t sbuf[40];
	static uint8_t slen = 0;

	uint8_t d;
	if( cbuf8_get(&cdc_rxq, &d) ) {

		if( slen >= sizeof(sbuf) ) { slen = 0; }

		if( (d == '\r') || (d == '\n') ) {
			if( slen == 0 ) return;
			while( slen < sizeof(sbuf) ) { sbuf[slen++] = 0; }

			uint8_t n = ptoi(sbuf[1]);
			if( (sbuf[0] == 'p') && (n > 0) && (n < PWD_COUNT) && (sbuf[2] == '=') ) {
				eeprom_update_block(sbuf+3, (void*)(PWD_SIZE * n), PWD_SIZE);
				Serial_SendString("sto\r\n");
			} else
			if( (sbuf[0] == 'l') && (n > 0) && (n < PWD_COUNT) && (sbuf[2] == '?') ) {
				uint8_t i;
				for( i = 0; i < PWD_SIZE; ++i ) {
					d = eeprom_read_byte((void*)(PWD_SIZE * n + i));
					if( (d < ' ') || (d > '}') ) break;
					Serial_SendByte(d);
				}
				Serial_SendString("\r\n");
			} else
			if( (sbuf[0] == 'c') && (sbuf[1] == '!') ) {
				eeprom_erase();
				Serial_SendString("clr\r\n");
			} else {
				Serial_SendString("err\r\n");
			}
		} else
		if( d == 0x7f ) { // backspace
			if( slen ) { --slen; }
		} else { // store character
			if( (d >= 32) && (d <= 126) ) { sbuf[slen++] = d; }
		}
	}
}

int s_main(void)
{
	cbuf8_clear(&cdc_rxq, rxbuf, sizeof(rxbuf));
	cbuf8_clear(&cdc_txq, txbuf, sizeof(txbuf));

	USB_Init();
	sei();

	while( 1 ) {
		wdt_reset();
		CDC_Task();
		USB_USBTask();
		Ser_Task();
	}
}
