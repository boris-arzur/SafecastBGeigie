/*
             LUFA Library
     Copyright (C) Dean Camera, 2009.
              
  dean [at] fourwalledcubicle [dot] com
      www.fourwalledcubicle.com
*/

/*
  Copyright 2009  Dean Camera (dean [at] fourwalledcubicle [dot] com)

  Permission to use, copy, modify, and distribute this software
  and its documentation for any purpose and without fee is hereby
  granted, provided that the above copyright notice appear in all
  copies and that both that the copyright notice and this
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
 *  Main source file for the MouseHost demo. This file contains the main tasks of
 *  the demo and is responsible for the initial application hardware configuration.
 */
 
#include "MouseHost.h"

/** Main program entry point. This routine configures the hardware required by the application, then
 *  enters a loop to run the application tasks in sequence.
 */
int main(void)
{
	SetupHardware();

	puts_P(PSTR(ESC_FG_CYAN "Mouse HID Host Demo running.\r\n" ESC_FG_WHITE));

	LEDs_SetAllLEDs(LEDMASK_USB_NOTREADY);

	for (;;)
	{
		Mouse_HID_Task();
		USB_USBTask();
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
	
	/* Hardware Initialization */
	SerialStream_Init(9600, false);
	LEDs_Init();
	USB_Init();
}

/** Event handler for the USB_DeviceAttached event. This indicates that a device has been attached to the host, and
 *  starts the library USB task to begin the enumeration and USB management process.
 */
void EVENT_USB_Host_DeviceAttached(void)
{
	puts_P(PSTR(ESC_FG_GREEN "Device Attached.\r\n" ESC_FG_WHITE));
	LEDs_SetAllLEDs(LEDMASK_USB_ENUMERATING);
}

/** Event handler for the USB_DeviceUnattached event. This indicates that a device has been removed from the host, and
 *  stops the library USB task management process.
 */
void EVENT_USB_Host_DeviceUnattached(void)
{
	puts_P(PSTR(ESC_FG_GREEN "Device Unattached.\r\n" ESC_FG_WHITE));
	LEDs_SetAllLEDs(LEDMASK_USB_NOTREADY);
}

/** Event handler for the USB_DeviceEnumerationComplete event. This indicates that a device has been successfully
 *  enumerated by the host and is now ready to be used by the application.
 */
void EVENT_USB_Host_DeviceEnumerationComplete(void)
{
	LEDs_SetAllLEDs(LEDMASK_USB_READY);
}

/** Event handler for the USB_HostError event. This indicates that a hardware error occurred while in host mode. */
void EVENT_USB_Host_HostError(const uint8_t ErrorCode)
{
	USB_ShutDown();

	printf_P(PSTR(ESC_FG_RED "Host Mode Error\r\n"
	                         " -- Error Code %d\r\n" ESC_FG_WHITE), ErrorCode);

	LEDs_SetAllLEDs(LEDMASK_USB_ERROR);
	for(;;);
}

/** Event handler for the USB_DeviceEnumerationFailed event. This indicates that a problem occurred while
 *  enumerating an attached USB device.
 */
void EVENT_USB_Host_DeviceEnumerationFailed(const uint8_t ErrorCode, const uint8_t SubErrorCode)
{
	printf_P(PSTR(ESC_FG_RED "Dev Enum Error\r\n"
	                         " -- Error Code %d\r\n"
	                         " -- Sub Error Code %d\r\n"
	                         " -- In State %d\r\n" ESC_FG_WHITE), ErrorCode, SubErrorCode, USB_HostState);

	LEDs_SetAllLEDs(LEDMASK_USB_ERROR);
}

/** Reads in and processes the next report from the attached device, displaying the report
 *  contents on the board LEDs and via the serial port.
 */
void ReadNextReport(void)
{
	USB_MouseReport_Data_t MouseReport;
	uint8_t                LEDMask = LEDS_NO_LEDS;

	/* Select mouse data pipe */
	Pipe_SelectPipe(MOUSE_DATAPIPE);	

	/* Unfreeze keyboard data pipe */
	Pipe_Unfreeze();

	/* Check to see if a packet has been received */
	if (!(Pipe_IsINReceived()))
	{
		/* No packet received (no movement), turn off LEDs */
		LEDs_SetAllLEDs(LEDS_NO_LEDS);
	
		/* Refreeze HID data IN pipe */
		Pipe_Freeze();
			
		return;
	}

	/* Ensure pipe contains data before trying to read from it */
	if (Pipe_IsReadWriteAllowed())
	{
		/* Read in mouse report data */
		Pipe_Read_Stream_LE(&MouseReport, sizeof(MouseReport));				

		/* Alter status LEDs according to mouse X movement */
		if (MouseReport.X > 0)
		  LEDMask |= LEDS_LED1;
		else if (MouseReport.X < 0)
		  LEDMask |= LEDS_LED2;
			
		/* Alter status LEDs according to mouse Y movement */
		if (MouseReport.Y > 0)
		  LEDMask |= LEDS_LED3;
		else if (MouseReport.Y < 0)
		  LEDMask |= LEDS_LED4;

		/* Alter status LEDs according to mouse button position */
		if (MouseReport.Button)
		  LEDMask  = LEDS_ALL_LEDS;
		
		LEDs_SetAllLEDs(LEDMask);
		
		/* Print mouse report data through the serial port */
		printf_P(PSTR("dX:%2d dY:%2d Button:%d\r\n"), MouseReport.X,
													  MouseReport.Y,
													  MouseReport.Button);
	}

	/* Clear the IN endpoint, ready for next data packet */
	Pipe_ClearIN();

	/* Refreeze mouse data pipe */
	Pipe_Freeze();
}

/** Task to set the configuration of the attached device after it has been enumerated, and to read and process
 *  HID reports from the device and display the results onto the board LEDs.
 */
void Mouse_HID_Task(void)
{
	uint8_t ErrorCode;

	/* Switch to determine what user-application handled host state the host state machine is in */
	switch (USB_HostState)
	{
		case HOST_STATE_Addressed:
			puts_P(PSTR("Getting Config Data.\r\n"));
		
			/* Get and process the configuration descriptor data */
			if ((ErrorCode = ProcessConfigurationDescriptor()) != SuccessfulConfigRead)
			{
				if (ErrorCode == ControlError)
				  puts_P(PSTR(ESC_FG_RED "Control Error (Get Configuration).\r\n"));
				else
				  puts_P(PSTR(ESC_FG_RED "Invalid Device.\r\n"));

				printf_P(PSTR(" -- Error Code: %d\r\n" ESC_FG_WHITE), ErrorCode);
				
				/* Indicate error status */
				LEDs_SetAllLEDs(LEDMASK_USB_ERROR);

				/* Wait until USB device disconnected */
				USB_HostState = HOST_STATE_WaitForDeviceRemoval;
				break;
			}

			/* Set the device configuration to the first configuration (rarely do devices use multiple configurations) */
			if ((ErrorCode = USB_Host_SetDeviceConfiguration(1)) != HOST_SENDCONTROL_Successful)
			{
				printf_P(PSTR(ESC_FG_RED "Control Error (Set Configuration).\r\n"
				                         " -- Error Code: %d\r\n" ESC_FG_WHITE), ErrorCode);

				/* Indicate error status */
				LEDs_SetAllLEDs(LEDMASK_USB_ERROR);
				
				/* Wait until USB device disconnected */
				USB_HostState = HOST_STATE_WaitForDeviceRemoval;
				break;
			}
			
			/* HID class request to set the mouse protocol to the Boot Protocol */
			USB_ControlRequest = (USB_Request_Header_t)
				{
					.bmRequestType = (REQDIR_HOSTTODEVICE | REQTYPE_CLASS | REQREC_INTERFACE),
					.bRequest      = REQ_SetProtocol,
					.wValue        = 0,
					.wIndex        = 0,
					.wLength       = 0,
				};

			/* Select the control pipe for the request transfer */
			Pipe_SelectPipe(PIPE_CONTROLPIPE);

			/* Send the request, display error and wait for device detach if request fails */
			if ((ErrorCode = USB_Host_SendControlRequest(NULL)) != HOST_SENDCONTROL_Successful)
			{
				printf_P(PSTR(ESC_FG_RED "Control Error (Set Protocol).\r\n"
				                         " -- Error Code: %d\r\n" ESC_FG_WHITE), ErrorCode);

				/* Indicate error status */
				LEDs_SetAllLEDs(LEDMASK_USB_ERROR);
				
				/* Wait until USB device disconnected */
				USB_HostState = HOST_STATE_WaitForDeviceRemoval;
				break;
			}

			puts_P(PSTR("Mouse Enumerated.\r\n"));

			USB_HostState = HOST_STATE_Configured;
			break;
		case HOST_STATE_Configured:
			/* If a report has been received, read and process it */
			ReadNextReport();

			break;
	}
}
