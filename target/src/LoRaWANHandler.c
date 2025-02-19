#include <stddef.h>
#include <stdio.h>

#include <ATMEGA_FreeRTOS.h>
#include <event_groups.h>

#include <lora_driver.h>
#include <status_leds.h>
#include <message_buffer.h>

#include "./include/dataHandler.h"


// Parameters for OTAA join
#define LORA_appEUI "05ABE2835032EC3E"
#define LORA_appKEY "B90973872CFD40F5E380185AD43FC18C"

void lora_handler_task( void *pvParameters );

static lora_driver_payload_t _uplink_payload; //Define the uplink payload

extern MessageBufferHandle_t downLinkMessageBuffer;
extern EventGroupHandle_t limitsEventGroup;

//bit for limitsEventGroup
#define BIT_LIMITS_DIFFER (1 << 0)
 
void lora_handler_initialise(UBaseType_t lora_handler_task_priority, MessageBufferHandle_t downlinkMessageBuffer)
{
	xTaskCreate(
	lora_handler_task
	,  "LRHand"  // A name just for humans
	,  configMINIMAL_STACK_SIZE+200  // This stack size can be checked & adjusted by reading the Stack Highwater
	,  NULL
	,  lora_handler_task_priority  // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
	,  NULL );

}

static void _lora_setup(void)
{
	char _out_buf[20];
	lora_driver_returnCode_t rc;
	status_leds_slowBlink(led_ST2); // OPTIONAL: Led the green led blink slowly while we are setting up LoRa

	// Factory reset the transceiver
	printf("FactoryReset >%s<\n", lora_driver_mapReturnCodeToText(lora_driver_rn2483FactoryReset()));
	
	// Configure to EU868 LoRaWAN standards
	printf("Configure to EU868 >%s<\n", lora_driver_mapReturnCodeToText(lora_driver_configureToEu868()));

	// Get the transceivers HW EUI
	rc = lora_driver_getRn2483Hweui(_out_buf);
	printf("Get HWEUI >%s<: %s\n",lora_driver_mapReturnCodeToText(rc), _out_buf);

	// Set the HWEUI as DevEUI in the LoRaWAN software stack in the transceiver
	printf("Set DevEUI: %s >%s<\n", _out_buf, lora_driver_mapReturnCodeToText(lora_driver_setDeviceIdentifier(_out_buf)));

	// Set Over The Air Activation parameters to be ready to join the LoRaWAN
	printf("Set OTAA Identity appEUI:%s appKEY:%s devEUI:%s >%s<\n", LORA_appEUI, LORA_appKEY, _out_buf, lora_driver_mapReturnCodeToText(lora_driver_setOtaaIdentity(LORA_appEUI,LORA_appKEY,_out_buf)));

	// Save all the MAC settings in the transceiver
	printf("Save mac >%s<\n",lora_driver_mapReturnCodeToText(lora_driver_saveMac()));

	// Enable Adaptive Data Rate
	printf("Set Adaptive Data Rate: ON >%s<\n", lora_driver_mapReturnCodeToText(lora_driver_setAdaptiveDataRate(LORA_ON)));

	// Set receiver window1 delay to 500 ms - this is needed if down-link messages will be used
	printf("Set Receiver Delay: %d ms >%s<\n", 500, lora_driver_mapReturnCodeToText(lora_driver_setReceiveDelay(500)));

	// Join the LoRaWAN
	uint8_t maxJoinTriesLeft = 10;
	
	do {
		rc = lora_driver_join(LORA_OTAA);
		printf("Join Network TriesLeft:%d >%s<\n", maxJoinTriesLeft, lora_driver_mapReturnCodeToText(rc));

		if ( rc != LORA_ACCEPTED)
		{
			// Make the red led pulse to tell something went wrong
			status_leds_longPuls(led_ST1); // OPTIONAL
			// Wait 5 sec and lets try again
			vTaskDelay(pdMS_TO_TICKS(5000UL));
		}
		else
		{
			break;
		}
	} while (--maxJoinTriesLeft);

	if (rc == LORA_ACCEPTED)
	{
		// Connected to LoRaWAN :-)
		// Make the green led steady
		status_leds_ledOn(led_ST2); // OPTIONAL
	}
	else
	{
		// Something went wrong
		// Turn off the green led
		status_leds_ledOff(led_ST2); // OPTIONAL
		// Make the red led blink fast to tell something went wrong
		status_leds_fastBlink(led_ST1); // OPTIONAL

		// Lets stay here
		while (1)
		{
			taskYIELD();
		}
	}
}

/*-----------------------------------------------------------*/
void lora_handler_task( void *pvParameters )
{
	// Hardware reset of LoRaWAN transceiver
	lora_driver_resetRn2483(1);
	vTaskDelay(2);
	lora_driver_resetRn2483(0);
	// Give it a chance to wakeup
	vTaskDelay(150);

	lora_driver_flushBuffers(); // get rid of first version string from module after reset!

	_lora_setup();

	_uplink_payload.len = 6;
	_uplink_payload.portNo = 2;

	//Setting up for the downlink
	
	lora_driver_payload_t downlinkPayload;

	TickType_t xLastWakeTime;
	const TickType_t xFrequency = pdMS_TO_TICKS(300000UL); // Upload message every 5 minutes (300000 ms)
	xLastWakeTime = xTaskGetTickCount();
	
	for(;;)
	{
		xTaskDelayUntil( &xLastWakeTime, xFrequency );
		
		struct MeasuredData measuredData = dataHandler_getData();

		int16_t temp = measuredData.temperature; // The REAL temp
		uint16_t hum = measuredData.humidity; // The REAL humidity
		uint16_t co2_ppm = measuredData.co2; // The REAL CO2

		
		printf("Real temperature in LoRaWAN Handler: %d\n", temp);
		printf("Real humidity in LoRaWAN Handler: %d\n", hum);

		_uplink_payload.bytes[0] = temp >> 8;
		_uplink_payload.bytes[1] = temp & 0xFF;
		_uplink_payload.bytes[2] = hum >> 8;
		_uplink_payload.bytes[3] = hum & 0xFF;
		_uplink_payload.bytes[4] = co2_ppm >> 8;
		_uplink_payload.bytes[5] = co2_ppm & 0xFF;

		lora_driver_returnCode_t rc;

		status_leds_shortPuls(led_ST4);  // OPTIONAL
		
		if((rc = lora_driver_sendUploadMessage(false, &_uplink_payload)) == LORA_MAC_TX_OK)
		{
			printf("*****Uplink sent*****: >%s<\n", lora_driver_mapReturnCodeToText(rc));
		}
		else if(rc == LORA_MAC_RX) //There is a message to be received
		{	
			printf("****Uplink sent*****\n");
			xMessageBufferReceive(downLinkMessageBuffer, &downlinkPayload, sizeof(lora_driver_payload_t), portMAX_DELAY);
			printf("Message length: %d \n",downlinkPayload.len);
			uint16_t minTemperatureLimit = (downlinkPayload.bytes[0] << 8) + downlinkPayload.bytes[1];
			uint16_t maxTemperatureLimit = (downlinkPayload.bytes[2] << 8) + downlinkPayload.bytes[3];
			printf("Received downlink: %d + %d\n", minTemperatureLimit, maxTemperatureLimit);
			dataHandler_setLimits(minTemperatureLimit, maxTemperatureLimit);

			// set bit
			xEventGroupSetBits(limitsEventGroup, BIT_LIMITS_DIFFER);
		}
	}
}