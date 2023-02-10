#include <avr/eeprom.h>
#include <avr/wdt.h>
#include <avr/power.h>
#include <util/delay.h>

#include "main.h"
/*
#define NSWITCHES 3
static const uint8_t swbit[NSWITCHES] = {7, 6, 5};
*/
#define NSWITCHES 4
static const uint8_t swbit[NSWITCHES] = {4, 5, 6, 7};

#define SW_ERASE_CMD 15
#define SW_SETUP_CMD 0

static uint8_t s_mode = 0;

// get dip switch selection
uint8_t getswi(void)
{
	static uint8_t r = 255;

	if( r == 255 ) {
		uint8_t i;
		for( i = 0; i < NSWITCHES; ++i ) {
			PORTD |= _BV(swbit[i]);
		}

		_delay_ms(1);
		r = 0;

		for( i = 0; i < NSWITCHES; ++i ) {
			if( !(PIND & _BV(swbit[i])) ) r |= _BV(i);
		}
	}

	return r;
}

// set entire eeprom to zeroes
void eeprom_erase(void)
{
	uint16_t ea;

	for( ea = 0; ea <= E2END; ++ea ) {
		eeprom_update_byte((void*)ea, 0);
	}
}

int main(void)
{
	clock_prescale_set(clock_div_1);

	if( getswi() == SW_ERASE_CMD ) {
		eeprom_erase();
	} else
	if( getswi() == SW_SETUP_CMD ) {
		s_mode = 1;
		s_main();
	} else {
		s_mode = 0;
		k_main();
	}

	while( 1 ) {
		wdt_reset();
	}
}

uint16_t CALLBACK_USB_GetDescriptor(const uint16_t wValue, const uint16_t wIndex, const void** const DescriptorAddress)
{
	if( s_mode ) {
		return s_CALLBACK_USB_GetDescriptor(wValue, wIndex, DescriptorAddress);
	} else {
		return k_CALLBACK_USB_GetDescriptor(wValue, wIndex, DescriptorAddress);
	}
}

void EVENT_USB_Device_ConfigurationChanged(void)
{
	if( s_mode ) {
		s_EVENT_USB_Device_ConfigurationChanged();
	} else {
		k_EVENT_USB_Device_ConfigurationChanged();
	}
}

void EVENT_USB_Device_ControlRequest(void)
{
	if( s_mode ) {
		s_EVENT_USB_Device_ControlRequest();
	} else {
		k_EVENT_USB_Device_ControlRequest();
	}
}
