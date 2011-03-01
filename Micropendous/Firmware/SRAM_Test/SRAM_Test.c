/*
             LUFA Library
     Copyright (C) Dean Camera, 2010.

  dean [at] fourwalledcubicle [dot] com
           www.lufa-lib.org
*/

/*
  Copyright 2010  Dean Camera (dean [at] fourwalledcubicle [dot] com)

  Altered for SRAM_Test by Opendous Inc. 2011-02
  For more information visit:  www.Micropendous.org/SRAM

  Permission to use, copy, modify, distribute, and sell this
  software and its documentation for any purpose is hereby granted
  without fee, provided that the above copyright notice appear in
  all copies and that both that the copyright notice and this
  permission notice and warranty disclaimer appear in supporting
  documentation, and that the name of the author not be used in
  advertising or publicity pertaining to distribution of the
  software without specific, written prior permission.

  The author disclaim all warranties with regard to this
  software, including all implied warranties of merchantability
  and fitness.  In no event shall the author be liable for any
  special, indirect or consequential damages or any damages
  whatsoever resulting from loss of use, data or profits, whether
  in an action of contract, negligence or other tortious action,
  arising out of or in connection with the use or performance of
  this software.
*/

/** \file
 *
 *  Main source file for the USBtoSerial project. This file contains the main tasks of
 *  the demo and is responsible for the initial application hardware configuration.
 */

#include "SRAM_Test.h"

/* Global pointer to the start of external memory */
static uint8_t* ExtMemArray = (uint8_t*)EXT_SRAM_START;
static uint16_t extSramStart = (uint16_t)EXT_SRAM_START;
static uint16_t extSramEnd = (uint16_t)EXT_SRAM_END;

volatile uint16_t EMAindex = 0;
volatile uint16_t firstSRAMfailAddress = 0;
volatile uint8_t SRAMTestStatus = 0;
volatile uint8_t FillingRAM = 0;
volatile uint8_t currentHelpPage = 0;


/** Circular buffer to hold data from the host before it is sent to the device via the serial port. */
RingBuff_t HostToDevice_Buffer;

/** Circular buffer to hold data from the serial port before it is sent to the host. */
RingBuff_t DeviceToHost_Buffer;

/** LUFA CDC Class driver interface configuration and state information. This structure is
 *  passed to all CDC Class driver functions, so that multiple instances of the same class
 *  within a device can be differentiated from one another.
 */
USB_ClassInfo_CDC_Device_t VirtualSerial_CDC_Interface =
	{
		.Config =
			{
				.ControlInterfaceNumber         = 0,

				.DataINEndpointNumber           = CDC_TX_EPNUM,
				.DataINEndpointSize             = CDC_TXRX_EPSIZE,
				.DataINEndpointDoubleBank       = false,

				.DataOUTEndpointNumber          = CDC_RX_EPNUM,
				.DataOUTEndpointSize            = CDC_TXRX_EPSIZE,
				.DataOUTEndpointDoubleBank      = false,

				.NotificationEndpointNumber     = CDC_NOTIFICATION_EPNUM,
				.NotificationEndpointSize       = CDC_NOTIFICATION_EPSIZE,
				.NotificationEndpointDoubleBank = false,
			},
	};

/** Main program entry point. This routine contains the overall program flow, including initial
 *  setup of all components and the main program loop.
 */
int main(void)
{
	SetupHardware();

	RingBuffer_InitBuffer(&HostToDevice_Buffer);
	RingBuffer_InitBuffer(&DeviceToHost_Buffer);

	LEDs_SetAllLEDs(LEDMASK_USB_NOTREADY);
	sei();


	for (;;)
	{
		// Read bytes from the USB OUT endpoint into the local data buffer
		for (uint8_t DataBytesRem = CDC_Device_BytesReceived(&VirtualSerial_CDC_Interface); DataBytesRem != 0; DataBytesRem--) {
			// Only try to read in bytes from the CDC interface if the transmit buffer is not full
			if (!(RingBuffer_IsFull(&HostToDevice_Buffer)))
			{
				int16_t ReceivedByte = CDC_Device_ReceiveByte(&VirtualSerial_CDC_Interface);

				// Read bytes from the USB OUT endpoint into the local data buffer
				if (!(ReceivedByte < 0))
					RingBuffer_Insert(&HostToDevice_Buffer, ReceivedByte);
			}
		}

		// Read bytes from the local data buffer into the USB IN endpoint
		RingBuff_Count_t BufferCount = RingBuffer_GetCount(&DeviceToHost_Buffer);
		while (BufferCount--)
			CDC_Device_SendByte(&VirtualSerial_CDC_Interface, RingBuffer_Remove(&DeviceToHost_Buffer));

		CDC_Device_USBTask(&VirtualSerial_CDC_Interface);
		USB_USBTask();

		MainTask();
	}
}

/** Configures the board hardware and chip peripherals for the demo's functionality. */
void SetupHardware(void)
{
	/* Disable watchdog if enabled by bootloader/fuses */
	MCUSR &= ~(1 << WDRF);
	wdt_disable();

	/* Disable clock division */
	clock_prescale_set(clock_div_1);

	/* disable JTAG to allow corresponding pins to be used - PF4, PF5, PF6, PF7 */
	/* TODO - remove this if you want to use your JTAG debugger to debug this firmware */
	#if (defined(__AVR_AT90USB1287__) || defined(__AVR_AT90USB647__) ||  \
			defined(__AVR_AT90USB1286__) || defined(__AVR_AT90USB646__) ||  \
			defined(__AVR_ATmega16U4__)  || defined(__AVR_ATmega32U4__) ||  \
			defined(__AVR_ATmega32U6__))
		// note the JTD bit must be written twice within 4 clock cycles to disable JTAG
		// you must also set the IVSEL bit at the same time, which requires IVCE to be set first
		// port pull-up resistors are enabled - PUD(Pull Up Disable) = 0
		MCUCR = (1 << JTD) | (1 << IVCE) | (0 << PUD);
		MCUCR = (1 << JTD) | (0 << IVSEL) | (0 << IVCE) | (0 << PUD);
	#endif

	/* Hardware Initialization */
	/* enable Ports based on which IC is being used */
	/* For more information look over the corresponding AVR's datasheet in the
		'I/O Ports' Chapter under subheading 'Ports as General Digital I/O' */
	#if (defined(__AVR_AT90USB162__) || defined(__AVR_AT90USB82__) || \
			defined(__AVR_ATmega16U2__) || defined(__AVR_ATmega32U2__))
		DDRD = 0;
		PORTD = 0;
		DDRB = 0;
		PORTB = 0;
		DDRC = 0;
		PORTC |= (0 << PC2) | (0 << PC4) | (0 << PC5) | (0 << PC6) | (0 << PC7); //only PC2,4,5,6,7 are pins
		// be careful using PortC as PC0 is used for the Crystal and PC1 is nRESET
	#endif

	#if (defined(__AVR_ATmega16U4__)  || defined(__AVR_ATmega32U4__))
		DDRD = 0;
		PORTD = 0;
		DDRB = 0;
		PORTB = 0;
		DDRC = 0;
		PORTC = (0 << PC6) | (0 << PC7); //only PC6,7 are pins
		DDRE = 0;
		PORTE = (0 << PE2) | (0 << PE6); //only PE2,6 are pins
		DDRF = 0;
		PORTF = (0 << PF0) | (0 << PF1) | (0 << PF4) | (0 << PF5) | (0 << PF6) | (0 << PF7); // only PF0,1,4,5,6,7 are pins
	#endif

	#if (defined(__AVR_AT90USB1287__) || defined(__AVR_AT90USB647__) ||  \
			defined(__AVR_AT90USB1286__) || defined(__AVR_AT90USB646__) ||  \
			defined(__AVR_ATmega32U6__))
		DDRA = 0;
		PORTA = 0;
		DDRB = 0;
		PORTB = 0;
		DDRC = 0;
		PORTC = 0;
		DDRD = 0;
		PORTD = 0;
		DDRE = 0;
		PORTE = 0;
		DDRF = 0;
		PORTF = 0;
		#if (BOARD == BOARD_MICROPENDOUS)
		// set PortB pin 1 to an output as it connects to an LED on the Micropendous
		DDRB |= (1 << PB1);
		// Set PE4=1 to disable external SRAM, PE6=0 to disable TXB0108, PE7=1 to select USB-B connector
		DDRE |= ((1 << PE4) | (1 << PE6) | (1 << PE7));
		PORTE |= ((1 << PE4) | (1 << PE7));
		PORTE &= ~(1 << PE6);
		#else // other boards such as the Micropendous3 or Micropendous4
		// Set PE6=1 to disable external SRAM
		DDRE |= (1 << PE6);
		PORTE |= (1 << PE6);
		#endif
	#endif

	/* Initialize stdout and stdin for printf and scanf */
	fdevopen(sendData, getData);

	/* Enable External SRAM */
	DISABLE_VOLTAGE_TXRX;
	PORTE_EXT_SRAM_SETUP;
	ENABLE_EXT_SRAM;
	// enable external SRAM with 0 wait states
	XMCRA = ((1 << SRE));
	XMCRB = 0;

	// perform pattern tests on the external SRAM
	SELECT_EXT_SRAM_BANK0; // set bank 0 as current SRAM bank
	SRAMTestStatus = TestEXTSRAM(ExtMemArray, extSramStart, extSramEnd); // test it
	SELECT_EXT_SRAM_BANK1; // set bank 1 as current SRAM bank
	SRAMTestStatus = TestEXTSRAM(ExtMemArray, extSramStart, extSramEnd); // test it
	SELECT_EXT_SRAM_BANK0;

	/* Hardware Initialization */
	LEDs_Init();
	SELECT_USB_B;
	USB_Init();
}

/** Event handler for the library USB Connection event. */
void EVENT_USB_Device_Connect(void)
{
	LEDs_SetAllLEDs(LEDMASK_USB_ENUMERATING);
}

/** Event handler for the library USB Disconnection event. */
void EVENT_USB_Device_Disconnect(void)
{
	LEDs_SetAllLEDs(LEDMASK_USB_NOTREADY);
}

/** Event handler for the library USB Configuration Changed event. */
void EVENT_USB_Device_ConfigurationChanged(void)
{
	bool ConfigSuccess = true;

	ConfigSuccess &= CDC_Device_ConfigureEndpoints(&VirtualSerial_CDC_Interface);

	LEDs_SetAllLEDs(ConfigSuccess ? LEDMASK_USB_READY : LEDMASK_USB_ERROR);
}

/** Event handler for the library USB Control Request reception event. */
void EVENT_USB_Device_ControlRequest(void)
{
	CDC_Device_ProcessControlRequest(&VirtualSerial_CDC_Interface);
}


/** Event handler for the CDC Class driver Line Encoding Changed event.
 *
 *  \param[in] CDCInterfaceInfo  Pointer to the CDC class interface configuration structure being referenced
 */
void EVENT_CDC_Device_LineEncodingChanged(USB_ClassInfo_CDC_Device_t* const CDCInterfaceInfo)
{
	/* TODO - Use Virtual Serial Port settings to control your application.
		Think of this as another data channel.  Use it for control/status messaging.
		Leave the data channel (MainTask putchar/getchar) for data only.
	*/

	switch (CDCInterfaceInfo->State.LineEncoding.ParityType)
	{
		case CDC_PARITY_Odd:
			// do something here
			break;
		case CDC_PARITY_Even:
			// and/or maybe here
			break;
		case CDC_PARITY_None:
			// maybe something here
			break;
		case CDC_PARITY_Mark:
			// something here could work
			break;
		case CDC_PARITY_Space:
			// you guessed it, something could go here
			break;
	}


	switch (CDCInterfaceInfo->State.LineEncoding.CharFormat)
	{
		case CDC_LINEENCODING_OneStopBit:
			// do something here
			break;
		case CDC_LINEENCODING_OneAndAHalfStopBits:
			// and/or maybe do something here
			break;
		case CDC_LINEENCODING_TwoStopBits:
			// something here could work
			break;
	}


	switch (CDCInterfaceInfo->State.LineEncoding.DataBits)
	{
		case 6:
			// possibly something
			break;
		case 7:
			// possibly something
			break;
		case 8:
			// possibly something
			break;
	}


	switch (CDCInterfaceInfo->State.LineEncoding.BaudRateBPS)
	{
		case 9600:
			// possibly something
			break;
		case 14400:
			// possibly something
			break;
		case 19200:
			// possibly something
			break;
		case 38400:
			// possibly something
			break;
		case 57600:
			// possibly something
			break;
		case 115200:
			// possibly something
			break;
	}

}


/** CDC class driver event for a control line state change on a CDC interface. This event fires each time the host requests a
 *  control line state change (containing the virtual serial control line states, such as DTR). The new control line states
 *  are available in the ControlLineStates.HostToDevice value inside the CDC interface structure passed as a parameter, set as
 *  a mask of CDC_CONTROL_LINE_OUT_* masks.  1 is for 'Set'(Low) and 0 is for 'Clear'(High) as these are active low signals.
 *  \param[in,out] CDCInterfaceInfo  Pointer to a structure containing a CDC Class configuration and state
*/
void EVENT_CDC_Device_ControLineStateChanged(USB_ClassInfo_CDC_Device_t* const CDCInterfaceInfo)
{
	/* TODO - Use Virtual Serial Port settings to control your application.
		Think of this as another data channel.  Use it for control/status messaging.
		Leave the data channel (MainTask putchar/getchar) for data only.
	*/

	if ((CDCInterfaceInfo->State.ControlLineStates.HostToDevice) & CDC_CONTROL_LINE_OUT_RTS) {
		// Host has set the RTS line
	} else {
		// Host has cleared the RTS line
	}

	if ((CDCInterfaceInfo->State.ControlLineStates.HostToDevice) & CDC_CONTROL_LINE_OUT_DTR) {
		// Host has set the DTR line
	} else {
		// Host has cleared the DTR line
	}
}


/* Use this function to make sure data exists.
	Need this function as RingBuff will crash if getData is called on
	an empty buffer
*/
uint8_t haveData(void) {
	return (uint8_t)(!RingBuffer_IsEmpty(&HostToDevice_Buffer));
}

/* In order to use printf functions, need a function that will send data over
	the USB Virtual Serial Port link
	return 0 on success, else failure and ensure binary compatibility
*/
static int sendData(char c, FILE *stream) {
	// most terminals require \r\n
	// however, do not include this conversion as it will break binary transfers 
	//if (c == '\n') {
	//	sendData('\r', stream);
	//}
	if (!RingBuffer_IsFull(&DeviceToHost_Buffer)) {
		RingBuffer_Insert(&DeviceToHost_Buffer, (uint8_t)c);
		return 0;
	}  else {
		return 1;
	}
}

/* Function to receive data from the USB Virtual Serial Port link */
int getData(FILE *__stream) {
	//if (something) return _FDEV_ERR; // cannot implement as GetElement has no error condition
	if (haveData() == 0) {
		return _FDEV_EOF;
	}
	return (int)RingBuffer_Remove(&HostToDevice_Buffer);
}


/* Test a SRAM address range with various patterns.
	Return 0 if all addresses work correctly, otherwise return test number that failed
*/
uint16_t TestEXTSRAM(uint8_t* array, uint16_t startAddress, uint16_t endAddress)
{
	uint16_t i = 0;
	uint16_t ArraySize = (uint16_t)(endAddress - startAddress);

	// Test1 - test upward count
	for (i = 0; i < ArraySize; i++) {
		array[i] = (uint8_t)i;
	}
	for (i = 0; i < ArraySize; i++) {
		if (array[i] != (uint8_t)i) {
			firstSRAMfailAddress = (uint16_t)(&array[i]);
			return 1;
		}
	}

	// Test2 - test downward count
	for (i = ArraySize; i > 0; i--) {
		array[i] = (uint8_t)i;
	}
	for (i = ArraySize; i > 0; i--) {
		if (array[i] != (uint8_t)i) {
			firstSRAMfailAddress = (uint16_t)(&array[i]);
			return 2;
		}
	}

	// Test3 - test 0xFF = 0b11111111
	for (i = 0; i < ArraySize; i++) {
		array[i] = 0xFF;
	}
	for (i = 0; i < ArraySize; i++) {
		if (array[i] != 0xFF) {
			firstSRAMfailAddress = (uint16_t)(&array[i]);
			return 3;
		}
	}	

	// Test4 - test 0xFF = 0b00000000
	for (i = 0; i < ArraySize; i++) {
		array[i] = 0x00;
	}
	for (i = 0; i < ArraySize; i++) {
		if (array[i] != 0x00) {
			firstSRAMfailAddress = (uint16_t)(&array[i]);
			return 4;
		}
	}	

	// Test5 - test 0xAA = 0b10101010
	for (i = 0; i < ArraySize; i++) {
		array[i] = 0xAA;
	}
	for (i = 0; i < ArraySize; i++) {
		if (array[i] != 0xAA) {
			firstSRAMfailAddress = (uint16_t)(&array[i]);
			return 5;
		}
	}	

	// Test 6 - test 0x55 = 0b01010101
	for (i = 0; i < ArraySize; i++) {
		array[i] = 0x55;
	}
	for (i = 0; i < ArraySize; i++) {
		if (array[i] != 0x55) {
			firstSRAMfailAddress = (uint16_t)(&array[i]);
			return 6;
		}
	}	

	// Test7 - test upward count again
	for (i = 0; i < ArraySize; i++) {
		array[i] = (uint8_t)i;
	}
	for (i = 0; i < ArraySize; i++) {
		if (array[i] != (uint8_t)i) {
			firstSRAMfailAddress = (uint16_t)(&array[i]);
			return 7;
		}
	}

	return 0;
}


/* MainTask will run once initialization is complete */
void MainTask(void)
{
	uint8_t  tempByte1 = 0;
	uint8_t  tempByte2 = 0;
	uint8_t  tempByte3 = 0;
	uint16_t  tempWord = 0;
	uint8_t*  tempBytePtr = 0;

	while (haveData()) {
		tempByte1 = getchar();

		if (tempByte1 == 'n') { // get the next byte of data from external SRAM array - use i cmd to get first
			printf("%3u\r\n", ExtMemArray[EMAindex]);
			EMAindex++;
		} else if ((tempByte1 == 'h') || (tempByte1 == '?')) { // print help
			if (currentHelpPage == 0) {
				printf_P(PSTR("\nAvailable Commnds\r\n"));
				printf_P(PSTR("o - print info\r\n"));
				printf_P(PSTR("\tPress h again for next page\r\n"));
				currentHelpPage = 1;
			} else if (currentHelpPage == 1) {
				printf_P(PSTR("t - SRAM test status\r\n"));
				printf_P(PSTR("s - size of SRAM\r\n"));
				printf_P(PSTR("\tPress h again for next page\r\n"));
				currentHelpPage = 2;
			} else if (currentHelpPage == 2) {
				printf_P(PSTR("b - current bank\r\n"));
				printf_P(PSTR("0 - select bank 0\r\n"));
				printf_P(PSTR("\tPress h again for next page\r\n"));
				currentHelpPage = 3;
			} else if (currentHelpPage == 3) {
				printf_P(PSTR("1 - select bank 1\r\n"));
				printf_P(PSTR("f??? - fill array at index ?? with data byte ? and return status\r\n"));
				printf_P(PSTR("\tPress h again for next page\r\n"));
				currentHelpPage = 4;
			} else if (currentHelpPage == 4) {
				printf_P(PSTR("g??? - fill array at index ?? with data byte ? but no status msg\r\n"));
				printf_P(PSTR("a?? - data at address ?? \r\n"));
				printf_P(PSTR("\tPress h again for next page\r\n"));
				currentHelpPage = 5;
			} else {
				printf_P(PSTR("i?? - data at SRAM array index ?? \r\n"));
				printf_P(PSTR("n - data at next SRAM array index - set start index with i?? \r\n"));
				printf_P(PSTR("\tThis is the end of the help page.\r\n\r\n"));
				currentHelpPage = 0;
			}
		} else if (tempByte1 == 'o') { // print info regarding SRAM
			printf_P(PSTR("SRAM StartAddr = "));
			printf("%5u,", (uint16_t)EXT_SRAM_START);
			printf_P(PSTR(" EndAddr = "));
			printf("%5u,", (uint16_t)EXT_SRAM_END);
			printf_P(PSTR(" Size = "));
			printf("%5u", (uint16_t)EXT_SRAM_SIZE);
			printf_P(PSTR(" bytes\r\n"));
		} else if (tempByte1 == 't') { // print whether SRAM failed internal testing
			if (SRAMTestStatus){
				printf_P(PSTR("SRAM_Fail_Addr = "));
				printf("%5u,", firstSRAMfailAddress);
				printf_P(PSTR(" Failed_Test "));
				printf("%u\r\n", SRAMTestStatus);
			} else {
				printf_P(PSTR("External_SRAM_Passed_Pattern_Tests\r\n"));
			}
		} else if (tempByte1 == 's') { // size of SRAM
			printf("%5u\r\n", ((uint16_t)EXT_SRAM_SIZE));
		} else if (tempByte1 == 'f') { // 0x67='f' start filling SRAM, each subsequent byte will fill next index, need to fill all of it
			tempByte2 = getchar();
			tempByte3 = getchar();
			tempByte1 = getchar();
			tempWord = (((uint16_t)tempByte2 << 8) + ((uint16_t)tempByte3));
			ExtMemArray[tempWord] = tempByte1;
			printf_P(PSTR("Filled EMA["));
			printf("%5u", tempWord);
			printf_P(PSTR("] with "));
			printf("%3u\r\n", ExtMemArray[tempWord]);
        } else if (tempByte1 == 'g') { // start filling SRAM, each subsequent byte will fill next index, need to fill all of it
			tempByte2 = getchar();
			tempByte3 = getchar();
			tempByte1 = getchar();
			tempWord = (((uint16_t)tempByte2 << 8) + ((uint16_t)tempByte3));
			ExtMemArray[tempWord] = tempByte1;
		} else if (tempByte1 == 'b') { // print current SRAM bank
			printf_P(PSTR("SRAM_Bank = "));
			printf("%u\r\n", (uint8_t)CURRENT_SRAM_BANK);
		} else if (tempByte1 == '0') { // select SRAM bank 0 for usage
			SELECT_EXT_SRAM_BANK0;
		} else if (tempByte1 == '1') { // select SRAM bank 1 for usage
			SELECT_EXT_SRAM_BANK1;
		} else if (tempByte1 == 'a') { // get the data byte from memory location defined by subsequent 2 bytes (word)
			tempByte2 = getchar();
			tempByte3 = getchar();
			tempWord = (((uint16_t)tempByte2 << 8) + ((uint16_t)tempByte3));
			tempBytePtr = (uint8_t*)tempWord;
			printf("Addr: %5u: = %3u\r\n", tempWord, ((uint8_t)(*tempBytePtr)));
		} else if (tempByte1 == 'i') { // 0x69='i' get the data byte from ExtMemArray at index defined by subsequent 2 bytes (word)
			tempByte2 = getchar();
			tempByte3 = getchar();
			tempWord = (((uint16_t)tempByte2 << 8) + ((uint16_t)tempByte3));
			EMAindex = tempWord;
			printf_P(PSTR("EMA["));
			printf("%5u", tempWord);
			printf_P(PSTR("] = Addr:"));
			printf("%5u = %3u\r\n", ((uint16_t)&ExtMemArray[EMAindex]), ExtMemArray[EMAindex]);
		} else {
			printf("%3u", (uint8_t)'f');
			printf_P(PSTR("-ERROR\r\n"));
		}
	}

}
