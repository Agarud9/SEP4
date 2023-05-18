#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include "./include/co2.h"
#include "./include/medianCalculator.h"
#include <mh_z19.h>
#include <ATMEGA_FreeRTOS.h>
#include <task.h>


static int16_t co2s[10] = {-404, -404, -404, -404, -404, -404, -404, -404, -404, -404};
static int indexOfLatestCO2 = 0;
static bool isProblem;

void co2_create(){
	mh_z19_initialise(ser_USART3);

	//printf("Initialization of mh_z19 (CO2 sensor) was successful!\n");
}

static void co2_measure(){
    mh_z19_returnCode_t rc = mh_z19_takeMeassuring();;
	
	if (rc != MHZ19_OK)
	{
		//printf("Measure of mh_z19 (CO2 sensor) failed!\n");
		isProblem = true;
	}
	else{
		//printf("Measure of mh_z19 (CO2 sensor) was successful!\n");
	}
}

static void co2_getLatestCO2(){
	uint16_t ppm;
	uint16_t* ptr = &ppm;
    mh_z19_returnCode_t rc = mh_z19_getCo2Ppm(ptr);

	if (rc != MHZ19_OK)
	{
		//printf("Getting of the measured mh_z19 (CO2 sensor) failed!\n");
		//printf("Returned value: %d", rc);
		isProblem = true;
	}
	else{
        printf("Latest co2: %u\n", ppm);
        co2s[indexOfLatestCO2++] = ppm;
        if (indexOfLatestCO2 == 10)
            indexOfLatestCO2 = 0;
	}
}

int16_t co2_getCO2Median()
{
	return medianCalculator_calculateMedian(co2s, 10);
}

void co2_task(void* pvParameters){
	// Remove compiler warnings
	(void)pvParameters;
	
	TickType_t xLastWakeTime;
	xLastWakeTime = xTaskGetTickCount();
	const TickType_t xFrequency1 = 300/portTICK_PERIOD_MS; // 300 ms
	const TickType_t xFrequency2 = 30000/portTICK_PERIOD_MS; // 30000 ms

	//loop
	for (;;)
	{
		//printf("CO2 Task started\n");
		
		isProblem = false;
		
		//measure co2
		co2_measure();
		xTaskDelayUntil(&xLastWakeTime, xFrequency1);
		
		if (isProblem)
			continue;
		
		//add latest co2 to the array
		co2_getLatestCO2();
		//wait 30 seconds for next measurement
		xTaskDelayUntil(&xLastWakeTime, xFrequency2);
	}
}
