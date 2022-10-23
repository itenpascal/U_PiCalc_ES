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

//Function declaration
void vControllerTask(void* pvParameters);	// Prototypen der Funktionstasks vor main
void vDisplayTask(void* pvParameters);
void vLeibnizTask(void* pvParameters);
void vWallisschesTask(void* pvParameters);
void vZeitTask(void* pvParameters);

//TaskHandle_t	control;					
//TaskHandle_t	display;
TaskHandle_t	leibniz;					// Für die Steuerung der Tasks da, zB. zum Suspenden oder Resumen
TaskHandle_t	wallis;						// Für die Steuerung der Tasks da, zB. zum Suspenden oder Resumen
TaskHandle_t	zeit;


//EventGroup for ButtonEvents.
EventGroupHandle_t egEventBits = NULL;
#define STARTSTOPP		0x01				// Button1_Short
#define RESET			0x02				// BUTTON1_LONG
#define PI_COLLECT		0x04				// BUTTON2_SHORT ; Bit für Pausieren berechnen damit Daten sicher für Anzeige abgegriffen werden können
#define PI_EVEN			0x08				// BUTTON2_LONG
#define BREAK			0x10				// BUTTON3_SHORT
//#define BUTTON3_LONG	0x20
#define ALGORITHMUS		0x40				// BUTTON4_SHORT
//#define BUTTON4_LONG	0x80
#define BUTTON_ALL		0xFF

float pi = 1;								// Startwert für Leibnizberechnung
uint32_t dauer = 0;							// Variable für Zeitberechnung von Start PI Berechnung bis gefunden



int main(void) {
	vInitClock();
	vInitDisplay();
	
	xTaskCreate(vControllerTask, (const char *) "control_tsk", configMINIMAL_STACK_SIZE+150, NULL, 3, NULL);			// höchste Prio da Tastenerkennung wichtig
	xTaskCreate(vDisplayTask, (const char *) "display_tsk", configMINIMAL_STACK_SIZE+100, NULL, 1, NULL);				// Anzeige weniger Kritisch, aber hat mehr priorität als die Berechnenden Tasks
	xTaskCreate(vLeibnizTask, (const char *) "leibniz_tsk", configMINIMAL_STACK_SIZE, NULL, 0, &leibniz);				// Prio 1 da wenig kritisch
	xTaskCreate(vWallisschesTask, (const char *) "wallis_tsk", configMINIMAL_STACK_SIZE, NULL, 0, &wallis);				// Prio 1 da wenig kritisch
	xTaskCreate(vZeitTask, (const char *) "zeit_tsk", configMINIMAL_STACK_SIZE, NULL, 2, &zeit);						// 2.höchste Prio, da Zeitkritisch, Tasten ändern etwas, wegen dem weniger kritisch das dieser tiefer
	
	vDisplayClear();
	vDisplayWriteStringAtPos(0,0,"PI-Calc ET-2009 ES");
	vDisplayWriteStringAtPos(1,0,"Pascal Iten");											
	vDisplayWriteStringAtPos(2,0,"           ");											
	vDisplayWriteStringAtPos(3,0,"start| <= | <= | <= ");									
	//vTaskDelay(10);																		// nicht Nötig, da Display schreibTask nur schreibt, wenn Freigabe von Berechnung Task, und hier noch nicht gestartet
	vTaskStartScheduler();																	// Scheduler starten für Task bearbeitungen durch diesen
	return 0;
}

void vControllerTask(void* pvParameters) {
	egEventBits = xEventGroupCreate();
	initButtons();																			//Initialize Buttons
	for(;;) {
		updateButtons();																	
		if(getButtonPress(BUTTON1) == SHORT_PRESSED) {									// Start
			xEventGroupSetBits(egEventBits, STARTSTOPP);									// setzt Eventbit auf 1 = start Rechnen
			xEventGroupClearBits(egEventBits, PI_EVEN);										// Rücksetzen Bit für überprüfung ob PI Berechnung gleich Pi Vorgabe
			vTaskResume(zeit);
		}
		if(getButtonPress(BUTTON2) == SHORT_PRESSED) {									// Stopp - Unterbrechen
			xEventGroupClearBits(egEventBits, STARTSTOPP);									// setzt Eventbitt wieder auf 0 = blockieren Rechnen
		}
		if(getButtonPress(BUTTON3) == SHORT_PRESSED) {									// Reset der Berechnung der Zahl Pi
			pi = 1;																			// Startwert Pi wieder auf 1 setzen
			xEventGroupSetBits(egEventBits, RESET);
			xEventGroupClearBits(egEventBits, PI_EVEN);			
			dauer = 0;																		// dauer wieder reseten auf 0
			vTaskResume(zeit);
		}
		if(getButtonPress(BUTTON4) == SHORT_PRESSED) {									// Switch - Wechsel der Algorithmen durch Suspend setzen des jeweilig anderen
			xEventGroupSetBits(egEventBits, RESET);											// Macht das bei einem Wechsel auch wieder von vorne angefange wird zu zählen
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
		vTaskDelay(10);																		// alle 10ms abfragen
	}
}

void vDisplayTask(void* pvParameters) {
	char tempPi[12] = "";
	char wahlAlgo[15] = "";
	for (;;) {
		xEventGroupClearBits(egEventBits, BREAK);											// setzte das Pausenbit zurück, für signalisation für Datenabholung
		xEventGroupWaitBits(egEventBits, PI_COLLECT, false, true, portMAX_DELAY);			// Bestätigung von Algorithmus Tasks das Pause
		if (xEventGroupGetBits(egEventBits) & ALGORITHMUS) {
			sprintf(wahlAlgo,"%.12s","Leibniz");
		} else {
			sprintf(wahlAlgo,"%.12s","Wallissche");
		}
			sprintf(tempPi,"%.7f",pi);														// Datenabholung muss berechnung stoppen, abholen, weiterlaufen
			xEventGroupClearBits(egEventBits, PI_COLLECT);									// Bit zurücksetzen wegen abholung PI damit Pause
			xEventGroupSetBits(egEventBits, BREAK);											// setzen des Pausenbits
			vDisplayClear();																// clear Display vor neuschreiben
			vDisplayWriteStringAtPos(0,0,"Modus: %s", wahlAlgo);							// Leibniz / Wallissches Modus
			vDisplayWriteStringAtPos(1,0,"Zeit bis Pi: %ds", dauer/1000);					// Berechnung Zeit PI, 1kHz Ticks
			vDisplayWriteStringAtPos(2,0, "PI: %s", tempPi);								// Feld fpr Anzeige aktueller Pi Wert
			vDisplayWriteStringAtPos(3,0, "strt|stp|reset|swtch");							// Start | Stopp | Reset | Switch
			vTaskDelay(500);																// Warten ~500ms bis wieder Anzeige Updaten 
			//vTaskDelay(100/portTICK_RATE_MS);
	}
}

void vLeibnizTask(void* pvParameters) {														// Berechnung Pi/4
	//vTaskSuspend(leibniz);																// initial state of vWallisschesTask Task shall be off	
	float piHilfe = 1;
	uint32_t n = 3;																			// Nenner Leibnizzahl startwert; unsigned int 32Bit
	long vergleich = 0;
	const long pi5Stellen = 314159;															// Variable für zum vergleichen
	xEventGroupSetBits(egEventBits, ALGORITHMUS);											// Anfangs setzen, da dieser Anfangs nicht suspendet ist, damit richiges Angezeigt wird
	for(;;) {
		if (xEventGroupGetBits(egEventBits) & RESET) {										// Rücksetzaufgabe
			piHilfe =1; 
			n = 3;
			xEventGroupSetBits(egEventBits, ALGORITHMUS);									// in der Resetfunktion, damit nicht jeder Durchlauf gesetzt wird
			xEventGroupClearBits(egEventBits, 0x03);										// Bits STARTSTOPP & RESET Clearen
			pi = 1;
			xEventGroupSetBits(egEventBits, PI_COLLECT);
		}
		if(xEventGroupGetBits(egEventBits) & STARTSTOPP) {
			if(xEventGroupGetBits(egEventBits) & BREAK) {									// wenn Pausenbit gesetzt dann rechnen erlaubt
				piHilfe = piHilfe - (1.0/n);												// 1.0 nötig dass es float ist, bei nur 1 istes Int
				n = n + 2;																	// aufzählen da so Algorithmus funktioniert
				pi = piHilfe*4;	
				vergleich = pi * 100000;													//damit die Nötigen Stellen ohne Komma nachfolgend verglichen werden können (long int schneidet nach Komma ab)
				if (vergleich == pi5Stellen) {
					xEventGroupSetBits(egEventBits,PI_EVEN);								// Bit setzen für übereinstimmung der Pi Werte
				}
				piHilfe = piHilfe + (1.0/n);											
				//pi = piHilfe*4;															// Rundenwert an Pi weitergeben für darstellung
				n = n + 2;
			} else {
			xEventGroupSetBits(egEventBits, PI_COLLECT);									// Wenn nicht am Berechnen/Break gesetzt, Freigabe für Datenabholung and DisplayTask geben
			} 
		}
// 		vergleich = pi * 100000;																//damit die Nötigen Stellen ohne Komma nachfolgend verglichen werden können (long int schneidet nach Komma ab)
//   		if (vergleich == pi5Stellen) {
// 	  		xEventGroupSetBits(egEventBits,PI_EVEN);											// Bit setzen für übereinstimmung der Pi Werte
// 		}
	}
}

void vWallisschesTask(void* pvParameters) {													// https://matheguru.com/allgemein/die-kreiszahl-pi.html
	vTaskSuspend(wallis);																	// initial state of vWallisschesTask Task shall be off
	float piHilfe = 2;																		// Hilfvariable zum Rechnen
	float piSave = 1;																		// Hilfsvariable zum Rechnen
	long vergleich = 0;																		// Pi in int mit gewünschter Genauigkeit mit 5 Nachkommastellen wandeln
	const long pi5Stellen = 314159;																// pi zum Vergleichen
	for(;;) {
		if (xEventGroupGetBits(egEventBits) & RESET) {										// Wenn Resettaste gedrückt wurde ...
			piHilfe = 2;																	// Rücksetzen
			piSave = 1;																		// Reücksetzen
			xEventGroupClearBits(egEventBits, ALGORITHMUS);									// in der Resetfunktion, damit nicht jeder Durchlauf gesetzt wird
			xEventGroupClearBits(egEventBits, 0x03);										// Bits STARTSTOPP & RESET Clearen
			pi = 1;
			xEventGroupSetBits(egEventBits, PI_COLLECT);									// Lesen beim Displaytask erlauben
		}
		if(xEventGroupGetBits(egEventBits) & STARTSTOPP) {									
			if(xEventGroupGetBits(egEventBits) & BREAK) {
				piSave = piSave*(piHilfe/(piHilfe - 1));									// vorheriger wert mit neuem Berechneten Wert Multiplizieren
				piSave = piSave*(piHilfe/(piHilfe + 1));
				piHilfe = piHilfe + 2;														// Hilfswert um 2 inklementieren für die nächste Runde
				pi = piSave*2;																// Ausgeben Pi Wert
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

 void vZeitTask(void* pvParameters) {															// Task für Berechnung der Zeit bis Pi errreicht wurde
  	TickType_t xLastWakeTime;																	// Nötig für vTaskDelayUntil damit auf den gewünschten Zeitpunkt wieder Task gestartet wird
  	const TickType_t xFrequency = 10;
  	xLastWakeTime = xTaskGetTickCount();
 	uint32_t start = 0;																			// Rechen Variablen für Zeit von Anfang bis Pi Erreicht
 	uint32_t stop = 0;
	uint32_t pause = 0;																			// Für wenn Stop gedrück wird dass diese Zeit abgezogen werden kann.
	uint32_t pauseKlein = 0;
	uint32_t pauseGross = 0;
	int hilfe = 0;
  	for (;;) {
  		vTaskDelayUntil(&xLastWakeTime, xFrequency);											// Task Pausieren bis gewünschte Zeit abgelaufen
			dauer = 0;
 		if (start == 0) {																		// Wenn Berechnung wieder geresetet wurde, damit nur beim ersten Durchgang dieser Wert geschrieben wird
			xEventGroupWaitBits(egEventBits, STARTSTOPP, false, true, portMAX_DELAY);			// aufs StartBit warten
 			start = xTaskGetTickCount();													// aktueller Tickwert nehmen
			pause = 0;
		}
		if ((xEventGroupGetBits(egEventBits) & STARTSTOPP) != 1) {								// Wenn Stop gedrückt wurden
			if (pauseKlein == 0) {																// Wenn Berechnung wieder geresetet wurde, damit nur bei der ersten Pause dieser Wert geschrieben wird
				pauseKlein = xTaskGetTickCount();										// aktueller Tickwert nehmen
				hilfe = 1;																		
			}
		}
		if (hilfe == 1) {																		// damit erst aufgerufen wenn der erste Wert der Pause aufgezeichnet wurde
			if ((xEventGroupGetBits(egEventBits) & STARTSTOPP)) {								// wenn wieder Start gedrückt wurde		
				pauseGross = xTaskGetTickCount();
				pause = pauseGross - pauseKlein + pause;										// Aufsumieren
				hilfe = 0;
				pauseKlein = 0;
				pauseGross = 0;
			}
		}
 		stop = xTaskGetTickCount();														// aktueller Wert zum Vergleich
		dauer = stop - start - pause;															// Wert schreiben
		if (xEventGroupGetBits(egEventBits) & PI_EVEN) {										// Rücksetzen wenn Pi erreicht und dann Task Suspenden
			start = 0;
			stop = 0;
			pause = 0;
			pauseKlein = 0;
			pauseGross = 0;
			vTaskSuspend(zeit);
		} else if (xEventGroupGetBits(egEventBits) & RESET) {									// Wenn Reset gedrückt wurde Rücksetzen und Suspenden
			start = 0;
			stop = 0;
			pause = 0;
			pauseKlein = 0;
			pauseGross = 0;
			hilfe = 0;
			dauer = 0;
			vTaskSuspend(zeit);
		} else {
			
		}			
 	}
}	
	