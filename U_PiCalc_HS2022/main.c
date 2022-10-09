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
//void vKettenTask(void* pvParameters);
//void vTimeTask(void* pvParameters);

// TaskHandle_t	control;
// TaskHandle_t	display;
//TaskHandle_t	leibniz;
//TaskHandle_t	ketten;
//TaskHandle_t	time;

/*
//EventGroup for ButtonEvents.
EventGroupHandle_t egButtonEvents = NULL;
#define BUTTON1_SHORT	0x01
#define BUTTON1_LONG	0x02
#define BUTTON2_SHORT	0x04
#define BUTTON2_LONG	0x08
#define BUTTON3_SHORT	0x10
#define BUTTON3_LONG	0x20
#define BUTTON4_SHORT	0x40
#define BUTTON4_LONG	0x80*/
#define BUTTON_ALL		0xFF

float pi4 = 1;									//Startwert für Leibnizberechnung
float pi = 0;
uint32_t n = 3;									//Nenner Leibnizzahl startwert; unsigned int 32Bit
char pistring[12];


int main(void) {
	vInitClock();
	vInitDisplay();
	
	xTaskCreate(vControllerTask, (const char *) "control_tsk", configMINIMAL_STACK_SIZE+150, NULL, 3, NULL);		//höchste Prio da Controller
	xTaskCreate(vDisplayTask, (const char *) "display_tsk", configMINIMAL_STACK_SIZE+150, NULL, 2, NULL);			//Prio 1 da wenig kritisch
	xTaskCreate(vLeibnizTask, (const char *) "leibniz_tsk", configMINIMAL_STACK_SIZE+150, NULL, 1, NULL);			//Prio 1,
	//xTaskCreate(vKettenTask, (const char *) "ketten_tsk", configMINIMAL_STACK_SIZE+150, NULL, 1, &ketten);				//
	//xTaskCreate(vTimeTask, (const char *) "time_tsk", configMINIMAL_STACK_SIZE+150, NULL, 2, &time);					//
	
	vDisplayClear();
	vDisplayWriteStringAtPos(0,0,"PI-Calc HS2022");
	vDisplayWriteStringAtPos(1,0,"leer1");							//
	vDisplayWriteStringAtPos(2,0,"leer2");							//
	vDisplayWriteStringAtPos(3,0,"Strt|Stp|Rst|switch");			//
	//vTaskDelay(10);
	vTaskStartScheduler();
	return 0;
}

void vControllerTask(void* pvParameters) {
	initButtons();
	for(;;) {
		vDisplayClear();
		updateButtons();
		if(getButtonPress(BUTTON1) == SHORT_PRESSED) {
			//char pistring[12];
			//sprintf(&pistring[0], "PI: %.8f", M_PI);
			//vDisplayWriteStringAtPos(1,0, "%s", pistring);
		}
		if(getButtonPress(BUTTON2) == SHORT_PRESSED) {
			
		}
		if(getButtonPress(BUTTON3) == SHORT_PRESSED) {
			
		}
		if(getButtonPress(BUTTON4) == SHORT_PRESSED) {							//Wechsel der Algorithmen durch inaktiv setzen des jeweilig anderen
			
		}
		if(getButtonPress(BUTTON1) == LONG_PRESSED) {
			
		}
		if(getButtonPress(BUTTON2) == LONG_PRESSED) {
			
		}
		if(getButtonPress(BUTTON3) == LONG_PRESSED) {
			
		}
		if(getButtonPress(BUTTON4) == LONG_PRESSED) {
			
		}
		//vTaskDelay(10/portTICK_RATE_MS);
		vTaskDelay(100);
	}
}

void vDisplayTask(void* pvParameters) {
	char temp[12] = "";
	for (;;) {
		sprintf(temp,"%.8f",pi4 * 4);
		//sprintf(&pistring[12], "PI: %.8f", pi4*4);
		vDisplayClear();												//clear Display vor neuschreiben
		vDisplayWriteStringAtPos(0,0,"Titel");							//Zeile,
		vDisplayWriteStringAtPos(1,0,"leer1");							//
		vDisplayWriteStringAtPos(2,0,"leer2345");					//
		vDisplayWriteStringAtPos(3,0, "%s", temp);					//
		vTaskDelay(200);
	}
}

void vLeibnizTask(void* pvParameters) {					//Berechnung Pi/4
	float piHilfe = pi4;
	for(;;) {
		piHilfe = piHilfe - (1.0/n);				//1.0 nötig dass es float ist, bei nur 1 istes Int
		n = n + 2;
		piHilfe = piHilfe + (1.0/n);
		pi4 = piHilfe;
		n = n + 2;
		vTaskDelay(1);
	}
}
/*
void vKettenTask(void* pvParameters) {
	//float Pi
	for(;;) {
		
		vTaskDelay(100);
	}
}

void vTimeTask(void* pvParameters) {			//PI = 3.141592653589793
	for(;;) {
		vTaskDelay(100 / portTICK_RATE_MS);
	}
}*/