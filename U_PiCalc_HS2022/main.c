/*
 * U_PiCalc_HS2022.c
 *
 * Created: 20.03.2018 18:32:07
 * Author : -
 */ 

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "avr_compiler.h"
#include "pmic_driver.h"
#include "TC_driver.h"
#include "clksys_driver.h"
#include "sleepConfig.h"
#include "port_driver.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "event_groups.h"
#include "stack_macros.h"

#include "mem_check.h"

#include "init.h"
#include "utils.h"
#include "errorHandler.h"
#include "NHD0420Driver.h"

#include "ButtonHandler.h"

//Funktions deklaration
void vControllerTask(void* pvParameters);
void vDisplayTask(void* pvParameters);
void vLeibnizTask(void* pvParameters);
void vKettenTask(void* pvParameters);
//void vTimeTask(void* pvParameters);

TaskHandle_t	control;
TaskHandle_t	display;
TaskHandle_t	leibniz;
TaskHandle_t	ketten;
//TaskHandle_t	time;


//EventGroup for ButtonEvents.
EventGroupHandle_t egButtonEvents = NULL;
#define BUTTON1_SHORT	0x01
#define BUTTON1_LONG	0x02
#define BUTTON2_SHORT	0x04
#define BUTTON2_LONG	0x08
#define BUTTON3_SHORT	0x10
#define BUTTON3_LONG	0x20
#define BUTTON4_SHORT	0x40
#define BUTTON4_LONG	0x80
#define BUTTON_ALL		0xFF

float pi = 1;									//Startwert für Leibnizberechnung

uint32_t n = 3;									//Nenner Leibnizzahl startwert; unsigned int 32Bit
//char pistring[12];

int main(void) {
	vInitClock();
	vInitDisplay();
	
	xTaskCreate(vControllerTask, (const char *) "control_tsk", configMINIMAL_STACK_SIZE+150, NULL, 3, &control);		//höchste Prio da Controller
	xTaskCreate(vDisplayTask, (const char *) "display_tsk", configMINIMAL_STACK_SIZE+150, NULL, 2, &display);			//Prio 1 da wenig kritisch
	xTaskCreate(vLeibnizTask, (const char *) "leibniz_tsk", configMINIMAL_STACK_SIZE+150, NULL, 1, &leibniz);			//Prio 1,
	xTaskCreate(vKettenTask, (const char *) "ketten_tsk", configMINIMAL_STACK_SIZE+150, NULL, 1, &ketten);				//
	//xTaskCreate(vTimeTask, (const char *) "time_tsk", configMINIMAL_STACK_SIZE+150, NULL, 2, &time);					//
	
	vDisplayClear();
	vDisplayWriteStringAtPos(0,0,"PI-Calc HS2022");
	vDisplayWriteStringAtPos(1,0,"leer1");							//
	vDisplayWriteStringAtPos(2,0,"leer2");							//
	vDisplayWriteStringAtPos(3,0,"strt|stp|reset|swtch");			//
	//vTaskDelay(10);
	vTaskStartScheduler();
	return 0;
}

void vControllerTask(void* pvParameters) {
	egButtonEvents = xEventGroupCreate();
	initButtons(); //Initialize Buttons
	for(;;) {
		//vDisplayClear();														// aktuell auskommentiert, da dazwischenfunken mit der Anzeige
		updateButtons();
		if(getButtonPress(BUTTON1) == SHORT_PRESSED) {							// Start
			xEventGroupSetBits(egButtonEvents, BUTTON1_SHORT);
																										//char pistring[12];
																										//sprintf(&pistring[0], "PI: %.8f", M_PI);
																										//vDisplayWriteStringAtPos(1,0, "%s", pistring);
		}
		if(getButtonPress(BUTTON2) == SHORT_PRESSED) {							// Stopp - Unterbrechen
			xEventGroupSetBits(egButtonEvents, BUTTON2_SHORT);
		}
		if(getButtonPress(BUTTON3) == SHORT_PRESSED) {							// Reset der Berechnung der Zahl Pi
			xEventGroupSetBits(egButtonEvents, BUTTON3_SHORT);
		}
		if(getButtonPress(BUTTON4) == SHORT_PRESSED) {							// Switch - Wechsel der Algorithmen durch Suspend setzen des jeweilig anderen
			//xEventGroupSetBits(egButtonEvents, BUTTON4_SHORT);
			eTaskState state = eTaskGetState(leibniz);							// HandleName und nicht TaskName
			if(state == eSuspended) {
				vTaskSuspend(ketten);
				vTaskResume(leibniz);
				
	
			} else {
				vTaskSuspend(leibniz);
				vTaskResume(ketten);
				
			}
		}
		if(getButtonPress(BUTTON1) == LONG_PRESSED) {
			xEventGroupSetBits(egButtonEvents, BUTTON1_LONG);
		}
		if(getButtonPress(BUTTON2) == LONG_PRESSED) {
			xEventGroupSetBits(egButtonEvents, BUTTON2_LONG);
		}
		if(getButtonPress(BUTTON3) == LONG_PRESSED) {
			xEventGroupSetBits(egButtonEvents, BUTTON3_LONG);
		}
		if(getButtonPress(BUTTON4) == LONG_PRESSED) {
			xEventGroupSetBits(egButtonEvents, BUTTON4_LONG);
		}
		//vTaskDelay(10/portTICK_RATE_MS);
		vTaskDelay(10);															// alle 10ms abfragen
	}
}

void vDisplayTask(void* pvParameters) {
	char temp[12] = "";
	for (;;) {
		sprintf(temp,"%.8f",pi);																//Datenabholung muss berechnung stoppen, abholen, weiterlaufen
		//sprintf(&pistring[12], "PI: %.8f", pi*4);
		vDisplayClear();												//clear Display vor neuschreiben
		vDisplayWriteStringAtPos(0,0,"Titel");							//Zeile,
		vDisplayWriteStringAtPos(1,0,"Algorithmus");					//Leibniz Ketten
		vDisplayWriteStringAtPos(2,0, "PI:    %s", temp);				//
		vDisplayWriteStringAtPos(3,0, "strt|stp|reset|swtch");			// Start | Stopp | Reset | Switch
		vTaskDelay(500);
	}
}

void vLeibnizTask(void* pvParameters) {					//Berechnung Pi/4								//auslesen in blockmeldung mit Eventbit(waitbit), Mit Suspense moden umschalten
	//vTaskSuspend(leibniz);											//initial state of vKettenTask Task shall be off
	float piHilfe = 1;
	
	for(;;) {
		piHilfe = piHilfe - (1.0/n);				//1.0 nötig dass es float ist, bei nur 1 istes Int
			n = n + 2;
			piHilfe = piHilfe + (1.0/n);
			pi = piHilfe*4;
			n = n + 2;
	}
}

void vKettenTask(void* pvParameters) {									// https://matheguru.com/allgemein/die-kreiszahl-pi.html
	vTaskSuspend(ketten);											//initial state of vKettenTask Task shall be off
	float piHilfe = 2;
	float piSave = 1;
	
	for(;;) {
		piSave = piSave*(piHilfe/(piHilfe - 1));
		piSave = piSave*(piHilfe/(piHilfe + 1));
		piHilfe = piHilfe + 2;
		pi = piSave*2;
	}
}
/*
void vTimeTask(void* pvParameters) {			//PI = 3.14159 2653589793
	for(;;) {
		vTaskDelay(100 / portTICK_RATE_MS);
	}
}*/


/*
if(xEventGroupGetBits(egButtonEvents) & BUTTON2_SHORT) {
*/

/*
			eTaskState state = eTaskGetState(blinkTask1Handle);
			if(state == eSuspended) {
				vTaskResume(blinkTask1Handle);
			} else {
				vTaskSuspend(blinkTask1Handle);
			}
			*/

/*
	vTaskSuspend(loadkillTaskHandle); //initial state of loadkill Task shall be off
*/