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
void vWallisschesTask(void* pvParameters);
//void vTimeTask(void* pvParameters);

TaskHandle_t	control;
TaskHandle_t	display;
TaskHandle_t	leibniz;
TaskHandle_t	wallis;
//TaskHandle_t	time;


//EventGroup for ButtonEvents.
EventGroupHandle_t egEventBits = NULL;
#define STARTSTOPP	0x01				// Button1_Short
#define RESET_SHORT	0x02				// BUTTON1_LONG
#define PI_COLLECT	0x04				// BUTTON2_SHORT ; Bit für Pausieren berechnen damit Daten sicher für Anzeige abgegriffen werden können
#define BUTTON2_LONG	0x08
#define BREAK	0x10					// BUTTON3_SHORT
#define BUTTON3_LONG	0x20
#define ALGORITHMUS	0x40				// BUTTON4_SHORT
#define BUTTON4_LONG	0x80
#define BUTTON_ALL		0xFF

float pi = 1;																											// Startwert für Leibnizberechnung
uint32_t n = 3;																											// Nenner Leibnizzahl startwert; unsigned int 32Bit


int main(void) {
	vInitClock();
	vInitDisplay();
	
	xTaskCreate(vControllerTask, (const char *) "control_tsk", configMINIMAL_STACK_SIZE+150, NULL, 3, &control);		// höchste Prio da Controller
	xTaskCreate(vDisplayTask, (const char *) "display_tsk", configMINIMAL_STACK_SIZE+150, NULL, 2, &display);			// Prio 1 da wenig kritisch
	xTaskCreate(vLeibnizTask, (const char *) "leibniz_tsk", configMINIMAL_STACK_SIZE+150, NULL, 1, &leibniz);			// Prio 1,
	xTaskCreate(vWallisschesTask, (const char *) "wallis_tsk", configMINIMAL_STACK_SIZE+150, NULL, 1, &wallis);			//
	//xTaskCreate(vTimeTask, (const char *) "time_tsk", configMINIMAL_STACK_SIZE+150, NULL, 2, &time);					//
	
	vDisplayClear();
	vDisplayWriteStringAtPos(0,0,"PI-Calc ET-2009 ES");
	vDisplayWriteStringAtPos(1,0,"Pascal Iten");							//
	vDisplayWriteStringAtPos(2,0,"leer2");							//
	vDisplayWriteStringAtPos(3,0,"start| <= | <= | <= ");				//
	//vTaskDelay(10);
	vTaskStartScheduler();
	return 0;
}

void vControllerTask(void* pvParameters) {
	egEventBits = xEventGroupCreate();
	initButtons(); //Initialize Buttons
	for(;;) {
		//vDisplayClear();																// aktuell auskommentiert, da dazwischenfunken mit der Anzeige
		updateButtons();
		if(getButtonPress(BUTTON1) == SHORT_PRESSED) {									// Start
			xEventGroupSetBits(egEventBits, STARTSTOPP);								// setzt Eventbitt auf 1 = start Rechnen
		}
		if(getButtonPress(BUTTON2) == SHORT_PRESSED) {									// Stopp - Unterbrechen
			xEventGroupClearBits(egEventBits, STARTSTOPP);								// setzt Eventbitt wieder auf 0 = blockieren Rechnen
			//xEventGroupSetBits(egEventBits, BUTTON2_SHORT);
		}
		if(getButtonPress(BUTTON3) == SHORT_PRESSED) {									// Reset der Berechnung der Zahl Pi
			xEventGroupClearBits(egEventBits, STARTSTOPP);
			xEventGroupSetBits(egEventBits, RESET_SHORT);
			pi = 1;
		}
		if(getButtonPress(BUTTON4) == SHORT_PRESSED) {									// Switch - Wechsel der Algorithmen durch Suspend setzen des jeweilig anderen
			//xEventGroupSetBits(egEventBits, BUTTON4_SHORT);
			xEventGroupSetBits(egEventBits, RESET_SHORT);								// Macht das bei einem Wechsel auch wieder von vorne angefange wird zu zählen
			eTaskState state = eTaskGetState(leibniz);									// Wechseln zwischen den Berechnung Tasks
			if(state == eSuspended) {
				vTaskSuspend(wallis);
				vTaskResume(leibniz);	
			} else {
				vTaskSuspend(leibniz);
				vTaskResume(wallis);
			}
		}
		if(getButtonPress(BUTTON1) == LONG_PRESSED) {
			//xEventGroupSetBits(egEventBits, BUTTON1_LONG);
		}
		if(getButtonPress(BUTTON2) == LONG_PRESSED) {
			//xEventGroupSetBits(egEventBits, BUTTON2_LONG);
		}
		if(getButtonPress(BUTTON3) == LONG_PRESSED) {
			//xEventGroupSetBits(egEventBits, BUTTON3_LONG);
		}
		if(getButtonPress(BUTTON4) == LONG_PRESSED) {
			//xEventGroupSetBits(egEventBits, BUTTON4_LONG);
			//char pistring[12];
			//sprintf(&pistring[0], "PI: %.8f", M_PI);
			//vDisplayWriteStringAtPos(1,0, "%s", pistring);
		}
		//vTaskDelay(10/portTICK_RATE_MS);
		vTaskDelay(10);																	// alle 10ms abfragen
	}
}

void vDisplayTask(void* pvParameters) {
	char tempPi[12] = "";
	char wahlAlgo[15] = "";
	for (;;) {
		xEventGroupClearBits(egEventBits, BREAK);										// legt das Stoppbit kurzzeitig
		xEventGroupWaitBits(egEventBits, PI_COLLECT, false, true, portMAX_DELAY);		// Bestätigung von Algorithmus Tasks das Pause
		if (xEventGroupGetBits(egEventBits) & ALGORITHMUS) {
			sprintf(wahlAlgo,"%.12s","Leibniz");
		} else {
			sprintf(wahlAlgo,"%.12s","Wallissche");
		}
			sprintf(tempPi,"%.7f",pi);													// Datenabholung muss berechnung stoppen, abholen, weiterlaufen
			vDisplayClear();															// clear Display vor neuschreiben
			vDisplayWriteStringAtPos(0,0,"Titel");										// Zeile,
			vDisplayWriteStringAtPos(1,0,"%s", wahlAlgo);								// Leibniz wallissches
			vDisplayWriteStringAtPos(2,0, "PI:    %s", tempPi);							//
			vDisplayWriteStringAtPos(3,0, "strt|stp|reset|swtch");						// Start | Stopp | Reset | Switch
			xEventGroupSetBits(egEventBits, BREAK);										// 
			xEventGroupClearBits(egEventBits, PI_COLLECT);								//
			vTaskDelay(500);
			//vTaskDelay(100/portTICK_RATE_MS);
	}
}

void vLeibnizTask(void* pvParameters) {													// Berechnung Pi/4
	//vTaskSuspend(leibniz);															// initial state of vWallisschesTask Task shall be off	
	float piHilfe = 1;
	xEventGroupSetBits(egEventBits, ALGORITHMUS);										// Anfangs setzen, da dieser Anfangs nicht suspendet ist, damit richiges Angezeigt wird
	for(;;) {
		if (xEventGroupGetBits(egEventBits) & RESET_SHORT) {
			piHilfe =1; 
			n = 3;
			xEventGroupSetBits(egEventBits, ALGORITHMUS);								// in der Resetfunktion, damit nicht jeder Durchlauf gesetzt wird
			xEventGroupClearBits(egEventBits, RESET_SHORT);
		}
		if(xEventGroupGetBits(egEventBits) & STARTSTOPP) {
			if(xEventGroupGetBits(egEventBits) & BREAK) {
				piHilfe = piHilfe - (1.0/n);											// 1.0 nötig dass es float ist, bei nur 1 istes Int
					n = n + 2;
					piHilfe = piHilfe + (1.0/n);
					pi = piHilfe*4;
					n = n + 2;
			} else {
			xEventGroupSetBits(egEventBits, PI_COLLECT);
			} 
		}
	}
}

void vWallisschesTask(void* pvParameters) {											// https://matheguru.com/allgemein/die-kreiszahl-pi.html
	vTaskSuspend(wallis);															// initial state of vWallisschesTask Task shall be off
	float piHilfe = 2;
	float piSave = 1;
	
	for(;;) {
		if (xEventGroupGetBits(egEventBits) & RESET_SHORT) {
			piHilfe = 2;
			piSave = 1;
			xEventGroupClearBits(egEventBits, ALGORITHMUS);								// in der Resetfunktion, damit nicht jeder Durchlauf gesetzt wird
			xEventGroupClearBits(egEventBits, RESET_SHORT);
		}
		if(xEventGroupGetBits(egEventBits) & STARTSTOPP) {
			if(xEventGroupGetBits(egEventBits) & BREAK) {
				//xEventGroupWaitBits(egEventBits, PI_COLLECT, false, true, portMAX_DELAY);
				piSave = piSave*(piHilfe/(piHilfe - 1));
				piSave = piSave*(piHilfe/(piHilfe + 1));
				piHilfe = piHilfe + 2;
				pi = piSave*2;
			} else {
				xEventGroupSetBits(egEventBits, PI_COLLECT);
			}
		}
	}
}
/*
void vTimeTask(void* pvParameters) {			// PI = 3.14159 2653589793
	for(;;) {
		vTaskDelay(100 / portTICK_RATE_MS);
	}
}*/


/*
if(xEventGroupGetBits(egEventBits) & BUTTON2_SHORT) {
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
