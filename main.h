#ifndef MAIN_H
#define MAIN_H

#define PWD_SIZE 32
#define PWD_COUNT 8

uint8_t getswi(void);
void eeprom_erase(void);

int k_main(void);
uint16_t k_CALLBACK_USB_GetDescriptor(const uint16_t, const uint16_t, const void** const);
void k_EVENT_USB_Device_ConfigurationChanged(void);
void k_EVENT_USB_Device_ControlRequest(void);

int s_main(void);
uint16_t s_CALLBACK_USB_GetDescriptor(const uint16_t, const uint16_t, const void** const);
void s_EVENT_USB_Device_ConfigurationChanged(void);
void s_EVENT_USB_Device_ControlRequest(void);

#endif
