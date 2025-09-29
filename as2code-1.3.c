/*
 * microwaveAssessment.c
 * Author : <Your Name + Student ID>
 */ 

#include <stdbool.h>
#include <stdint.h>
#include <avr/interrupt.h>

/* Internal Library Includes */
#include "serialio.h"
#include "terminalio.h"
#include "ledmatrix.h"

#define F_CPU 8000000UL 
#include <util/delay.h>

// ENUMs / 'enumerations' basically give names to integer constants; by default values start at 0 and auto-increment.
// This lets you use readable labels (like MODE_QUICK, MODE_POPCORN) instead of magic numbers (0,1,2,...), though under the hood they are still integers
// Cooking mode enumeration.
typedef enum {
	MODE_QUICK,
	MODE_POPCORN,
	MODE_BEVERAGE,
	MODE_DEFROST
} Mode;

// Power level enumeration.
typedef enum {
	POWER_10,
	POWER_50,
	POWER_100
} PowerLevel;

// GLOBAL FLAGS - these can be accessed from anywhere in the file, making them useful for storing global state information: 
// These global variables are here to help you track the state of your microwave, feel free to add more as you see fit.
Mode mode;   // Quick, Popcorn, Beverage or Defrost mode.
uint8_t time_remaining; // Integer value between 0 and 9.
PowerLevel power_level; // 10% / 50% / 100%.
bool is_running; // Tracks whether the microwave is counting down.
bool is_paused;

uint8_t cooking_time[10] = { 63,6,91,79,102,109,125,7,127,111 };
uint8_t mode_identify[4] = { 103,115,124,94 };

volatile uint8_t power_cont = 0;
uint8_t p_0 = 0b100;
uint8_t p_1 = 0b110;
uint8_t p_2 = 0b111;

ISR(PCINT2_vect){             
	uint8_t pinc4_ago = 1;
	uint8_t pinc4_now = (PINC & (1<<PINC4)) ? 1 : 0;
	if(pinc4_ago == !pinc4_now)	power_cont++;
}

static volatile uint8_t cc;    //初始化cc变量，只初始化一次并且可以传参给isr

ISR(TIMER2_COMPA_vect){
	cc = !cc;
}



void initialise_hardware(void) {
	DDRA = 0xFF;			
	DDRC |= (1<<DDRC7)|(1<<DDRC3)|(1<<DDRC2)|(1<<DDRC1)|(1<<DDRC0);	
	DDRD |= (1<<DDRD7)|(1<<DDRD6)|(1<<DDRD5);
	_delay_ms(100);					//此处的delay是用来消除上电的抖动的，不会影响ssd和led的亮度
	PCICR  |=  (1<<PCIE2);          // 打开 PCI2 组（PCINT23..16）
	PCMSK2 |=  (1<<PCINT20);		// 打开 PINC4 具体使能
	TCCR2A = (1<<WGM21);
	TCCR2B = (1<<CS22);
	TIMSK2 = (1<<OCIE2A);
	OCR2A = 124;
	sei();
} 

void mode_selection(void){
	uint8_t num_fromlsb = (PINC>>5)&(0b00000011);
	uint8_t mask_code = 1<<(num_fromlsb); 
	PORTC = (PORTC&0xF0)|mask_code;
}

void power_led_light(PowerLevel power_level){
	
/*	uint8_t p_0 = 0b100;
	uint8_t p_1 = 0b110;
	uint8_t p_2 = 0b111;
*/	
	//PORTD = (PORTD & ~((1<<PIND5)|(1<<PIND6)|(1<<PIND7)))  | ((p_0 & 0x07) << PIND5);
	if (power_level==POWER_10){
		PORTD = (PORTD & ~((1<<PIND5)|(1<<PIND6)|(1<<PIND7)))  | ((p_0 & 0x07) << PIND5);
		}
	else if (power_level==POWER_50){
		PORTD = (PORTD & ~((1<<PIND5)|(1<<PIND6)|(1<<PIND7)))  | ((p_1 & 0x07) << PIND5);
		}
	else{
		PORTD = (PORTD & ~((1<<PIND5)|(1<<PIND6)|(1<<PIND7)))  | ((p_2 & 0x07) << PIND5);
		}
}


void show_power(){
	uint8_t power_mode = power_cont%3;
	power_level = power_mode;
	power_led_light(power_level);
	
}

uint8_t index_ct;
uint8_t index_mi;

void display_digit(uint8_t index_ct, uint8_t index_mi, uint8_t cc)
{
	PORTC = (0x7F&PINC)|((0x01&cc)<<7);         //设置位选cc
	if (cc==0){
		PORTA = cooking_time[index_ct];	
	} 
	else{
		PORTA = mode_identify[index_mi];
	}
}


void ssd_display(){
	
	uint8_t s0_s1 = ((PINC>>5)&0x03);
	
	if(s0_s1 == 0) 
	{
		index_ct = 5;
		index_mi = 0;
		display_digit(index_ct,index_mi,cc);
	}
	else if (s0_s1 == 1)
	{
		index_ct = 7;
		index_mi = 1;
		display_digit(index_ct,index_mi,cc);
	}
	else if (s0_s1 == 2)
	{
		index_ct = 3;
		index_mi = 2;
		display_digit(index_ct,index_mi,cc);
	}
	else if (s0_s1 == 3)
	{
		index_ct = 9;
		index_mi = 3;
		display_digit(index_ct,index_mi,cc);
	}
	//cc = !cc;
	//_delay_ms(0.5);
}


void run_microwave() {
	// Feel free to edit this as much as you see fit.
	
	if (!is_running && !is_paused) {
		// IDLE STATE - microwave ready for mode selection and configuration.
		
	} else if (is_running) {
		// RUNNING STATE - timer actively counting down.
		
	} else if (is_paused) {
		// PAUSED STATE - timer stopped, awaiting resume or reset.
		
	} 
}


int main(void)
{
	// Method call to where you can ready all your one-time hardware related settings.
	power_cont = 0;
	initialise_hardware();
	
	// Initialises Microwave State to Quick Cook Mode, 5 seconds remaining, 10% Power, not running nor paused.
	mode = MODE_QUICK;
	time_remaining = 7;
	power_level = POWER_10;
	is_running = false;
	is_paused = false;
		
	// Main execution loop - runs continuously.
	// Handles all microwave operations through state machine.
	while (true) {
		run_microwave();
		mode_selection();
		show_power();
		ssd_display();
	}
}

