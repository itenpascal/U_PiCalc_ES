/*
 * U_PiCalc_HS2022.c
 *
 * Created: 20.03.2018 18:32:07
 * Author : Iten Pascal				1kHz Ticks
 */ 

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
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
void vZeitTask(void* pvParameters);

TaskHandle_t	control;
TaskHandle_t	display;
TaskHandle_t	leibniz;
TaskHandle_t	wallis;
TaskHandle_t	zeit;


//EventGroup for ButtonEvents.
EventGroupHandle_t egEventBits = NULL;
#define STARTSTOPP	0x01				// Button1_Short
#define RESET_SHORT	0x02				// BUTTON1_LONG
#define PI_COLLECT	0x04				// BUTTON2_SHORT ; Bit für Pausieren berechnen damit Daten sicher für Anzeige abgegriffen werden können
#define PI_EVEN	0x08					// BUTTON2_LONG
#define BREAK	0x10					// BUTTON3_SHORT
#define BUTTON3_LONG	0x20
#define ALGORITHMUS	0x40				// BUTTON4_SHORT
#define BUTTON4_LONG	0x80
#define BUTTON_ALL		0xFF

float pi = 1;																											// Startwert für Leibnizberechnung
uint32_t dauer = 0;
//uint32_t n = 3;																											// Nenner Leibnizzahl startwert; unsigned int 32Bit


int main(void) {
	vInitClock();
	vInitDisplay();
	
	xTaskCreate(vControllerTask, (const char *) "control_tsk", configMINIMAL_STACK_SIZE+150, NULL, 3, &control);		// höchste Prio da Tastenerkennung wichtig
	xTaskCreate(vDisplayTask, (const char *) "display_tsk", configMINIMAL_STACK_SIZE+100, NULL, 2, &display);			// Prio 1 da wenig kritisch
	xTaskCreate(vLeibnizTask, (const char *) "leibniz_tsk", configMINIMAL_STACK_SIZE, NULL, 1, &leibniz);			// Prio 1,
	xTaskCreate(vWallisschesTask, (const char *) "wallis_tsk", configMINIMAL_STACK_SIZE, NULL, 1, &wallis);			//
	xTaskCreate(vZeitTask, (const char *) "zeit_tsk", configMINIMAL_STACK_SIZE, NULL, 2, &zeit);					//
	
	vDisplayClear();
	vDisplayWriteStringAtPos(0,0,"PI-Calc ET-2009 ES");
	vDisplayWriteStringAtPos(1,0,"Pascal Iten");											//
	vDisplayWriteStringAtPos(2,0,"           ");											//
	vDisplayWriteStringAtPos(3,0,"start| <= | <= | <= ");									//
	//vTaskDelay(10);
	vTaskStartScheduler();
	return 0;
}

void vControllerTask(void* pvParameters) {
	egEventBits = xEventGroupCreate();
	initButtons(); //Initialize Buttons
	for(;;) {
		//vDisplayClear();																	// aktuell auskommentiert, da dazwischenfunken mit der Anzeige
		updateButtons();
		if(getButtonPress(BUTTON1) == SHORT_PRESSED) {										// Start
			xEventGroupSetBits(egEventBits, STARTSTOPP);									// setzt Eventbitt auf 1 = start Rechnen
			xEventGroupClearBits(egEventBits, PI_EVEN);		
			vTaskResume(zeit);
		}
		if(getButtonPress(BUTTON2) == SHORT_PRESSED) {										// Stopp - Unterbrechen
			xEventGroupClearBits(egEventBits, STARTSTOPP);									// setzt Eventbitt wieder auf 0 = blockieren Rechnen
			//vTaskSuspend(zeit);
		}
		if(getButtonPress(BUTTON3) == SHORT_PRESSED) {										// Reset der Berechnung der Zahl Pi
			pi = 1;

			//xEventGroupSetBits(egEventBits, STARTSTOPP);
			xEventGroupSetBits(egEventBits, RESET_SHORT);
			xEventGroupClearBits(egEventBits, PI_EVEN);			
			dauer = 0;																		// dauer wieder reseten auf 0
			vTaskResume(zeit);
			
		}
		if(getButtonPress(BUTTON4) == SHORT_PRESSED) {										// Switch - Wechsel der Algorithmen durch Suspend setzen des jeweilig anderen
			//xEventGroupSetBits(egEventBits, BUTTON4_SHORT);
			
			xEventGroupSetBits(egEventBits, RESET_SHORT);									// Macht das bei einem Wechsel auch wieder von vorne angefange wird zu zählen
			xEventGroupClearBits(egEventBits, PI_EVEN);	
			eTaskState state = eTaskGetState(leibniz);										// Wechseln zwischen den Berechnung Tasks
			if(state == eSuspended) {	
				vTaskSuspend(wallis);
				vTaskResume(leibniz);	
			} else {
				vTaskSuspend(leibniz);
				vTaskResume(wallis);
			}
			dauer = 0;																		// dauer wieder reseten auf 0
			vTaskResume(zeit);
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
		vTaskDelay(10);																		// alle 10ms abfragen
	}
}

void vDisplayTask(void* pvParameters) {
	char tempPi[12] = "";
	char wahlAlgo[15] = "";
	for (;;) {
		xEventGroupClearBits(egEventBits, BREAK);											// legt das Stoppbit kurzzeitig
		xEventGroupWaitBits(egEventBits, PI_COLLECT, false, true, portMAX_DELAY);			// Bestätigung von Algorithmus Tasks das Pause
		if (xEventGroupGetBits(egEventBits) & ALGORITHMUS) {
			sprintf(wahlAlgo,"%.12s","Leibniz");
		} else {
			sprintf(wahlAlgo,"%.12s","Wallissche");
		}
			sprintf(tempPi,"%.7f",pi);														// Datenabholung muss berechnung stoppen, abholen, weiterlaufen
// 			if (xEventGroupGetBits(egEventBits) & RESET_SHORT) {
// 				printf(tempPi,"2BStarted");
// 			}
			vDisplayClear();																// clear Display vor neuschreiben
			vDisplayWriteStringAtPos(0,0,"Modus: %s", wahlAlgo);							// Leibniz / Wallissches
			vDisplayWriteStringAtPos(1,0,"Zeit bis Pi: %ds", dauer/1000);
			vDisplayWriteStringAtPos(2,0, "PI: %s", tempPi);								//
			vDisplayWriteStringAtPos(3,0, "strt|stp|reset|swtch");							// Start | Stopp | Reset | Switch
			xEventGroupSetBits(egEventBits, BREAK);											// 
			xEventGroupClearBits(egEventBits, PI_COLLECT);									//
			vTaskDelay(200);	
			//vTaskDelay(100/portTICK_RATE_MS);
	}
}

void vLeibnizTask(void* pvParameters) {														// Berechnung Pi/4
	//vTaskSuspend(leibniz);																// initial state of vWallisschesTask Task shall be off	
	float piHilfe = 1;
	uint32_t n = 3;	
	long vergleich = 0;
	long pi5Stellen = 314159;
	xEventGroupSetBits(egEventBits, ALGORITHMUS);											// Anfangs setzen, da dieser Anfangs nicht suspendet ist, damit richiges Angezeigt wird
	for(;;) {
		if (xEventGroupGetBits(egEventBits) & RESET_SHORT) {
			piHilfe =1; 
			n = 3;
			xEventGroupSetBits(egEventBits, ALGORITHMUS);									// in der Resetfunktion, damit nicht jeder Durchlauf gesetzt wird
			xEventGroupClearBits(egEventBits, 0x03);										// Bits STARTSTOPP & RESET_SHORT Clearen
			xEventGroupSetBits(egEventBits, PI_COLLECT);
			pi = 1;
		}
		if(xEventGroupGetBits(egEventBits) & STARTSTOPP) {
			if(xEventGroupGetBits(egEventBits) & BREAK) {
				piHilfe = piHilfe - (1.0/n);												// 1.0 nötig dass es float ist, bei nur 1 istes Int
					n = n + 2;																// aufzählen da so Algorithmus funktioniert
					piHilfe = piHilfe + (1.0/n);											
					pi = piHilfe*4;															// Rundenwert an Pi weitergeben für darstellung
					n = n + 2;
			} else {
			xEventGroupSetBits(egEventBits, PI_COLLECT);
			} 
		}
		vergleich = pi * 100000;
  		if (vergleich == pi5Stellen) {
	  		xEventGroupSetBits(egEventBits,PI_EVEN);
		}
	}
}

void vWallisschesTask(void* pvParameters) {													// https://matheguru.com/allgemein/die-kreiszahl-pi.html
	vTaskSuspend(wallis);																	// initial state of vWallisschesTask Task shall be off
	float piHilfe = 2;																		// Hilfvariable zum Rechnen
	float piSave = 1;																		// Hilfsvariable zum Rechnen
	long vergleich = 0;																		// Pi in int mit gewünschter Genauigkeit mit 5 Nachkommastellen wandeln
	long pi5Stellen = 314159;																// pi zum Vergleichen
	for(;;) {
		if (xEventGroupGetBits(egEventBits) & RESET_SHORT) {								// Wenn Resettaste gedrückt wurde ...
			piHilfe = 2;																	// Rücksetzen
			piSave = 1;																		// Reücksetzen
			xEventGroupClearBits(egEventBits, ALGORITHMUS);									// in der Resetfunktion, damit nicht jeder Durchlauf gesetzt wird
			xEventGroupClearBits(egEventBits, 0x03);										// Bits STARTSTOPP & RESET_SHORT Clearen
			xEventGroupSetBits(egEventBits, PI_COLLECT);									// Lesen beim Displaytask erlauben
			pi = 1;
		}
		if(xEventGroupGetBits(egEventBits) & STARTSTOPP) {
			if(xEventGroupGetBits(egEventBits) & BREAK) {
				piSave = piSave*(piHilfe/(piHilfe - 1));
				piSave = piSave*(piHilfe/(piHilfe + 1));
				piHilfe = piHilfe + 2;
				pi = piSave*2;
			} else {
				xEventGroupSetBits(egEventBits, PI_COLLECT);
			}
		}
		vergleich = pi * 100000;
 		if (vergleich == pi5Stellen) {
 			xEventGroupSetBits(egEventBits,PI_EVEN);
		}
	}
}

 void vZeitTask(void* pvParameters) {															// 
  	TickType_t xLastWakeTime;
  	const TickType_t xFrequency = 10;
  	xLastWakeTime = xTaskGetTickCount();
 	uint32_t start = 0;					// unsigned volatile
 	uint32_t stop = 0;
	uint32_t pause = 0;
	uint32_t pauseKlein = 0;
	uint32_t pauseGross = 0;
	int hilfe = 0;
 
  	for (;;) {
  		vTaskDelayUntil(&xLastWakeTime, xFrequency);
		//xEventGroupWaitBits(egEventBits, STARTSTOPP, false, true, portMAX_DELAY);				// 
 		if (start == 0) {
			xEventGroupWaitBits(egEventBits, STARTSTOPP, false, true, portMAX_DELAY);			// 
 			start = xTaskGetTickCountFromISR();
		}
		if ((xEventGroupGetBits(egEventBits) & STARTSTOPP) != 1) {
			if (pauseKlein == 0) {
				pauseKlein = xTaskGetTickCountFromISR();
				hilfe = 1;
			}
		}
		if (hilfe == 1) {
			if ((xEventGroupGetBits(egEventBits) & STARTSTOPP)) {
				if (pauseGross == 0) {
					pauseGross = xTaskGetTickCountFromISR();
					pause = pauseGross - pauseKlein + pause;
					hilfe = 0;
					pauseKlein = 0;
					pauseGross = 0;
				}
			}
		}
 		stop = xTaskGetTickCountFromISR();
		if (xEventGroupGetBits(egEventBits) & PI_EVEN) {
			start = 0;
			stop = 0;
			pause = 0;
			vTaskSuspend(zeit);
		} else if (xEventGroupGetBits(egEventBits) & RESET_SHORT) {
			start = 0;
			stop = 0;
			pause = 0;
			vTaskSuspend(zeit);
		} else {
			dauer = stop - start - pause;
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


// vTaskDelayUntil		unabhängig von der Last des Tasks immer gleich lange warten lassen