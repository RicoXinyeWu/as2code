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
PowerLevel power_level; // 10% / 50% / 100%.
bool is_running; // Tracks whether the microwave is counting down.
bool is_paused;

void b0_debounce_1ms(void);
void b1_debounce_1ms(void);


volatile uint8_t power_cont = 0;
uint8_t p_0 = 0b100;
uint8_t p_1 = 0b110;
uint8_t p_2 = 0b111;

ISR(INT2_vect){																	//用于判断B2下降沿的isr函数，当B2的电平发生变化的时候，检测是否为下降沿，如果是，那就实现power_cont++
	//uint8_t pinb2_ago = 1;
	//uint8_t pinb2_now = (PINB & (1<<PINB2)) ? 1 : 0;
	//if(pinb2_ago == !pinb2_now)	
	power_cont++;
}

static volatile uint8_t cc;    //初始化cc变量，只初始化一次并且可以传参给isr
volatile uint16_t howmany_ms = 0;   // 累计过了多少毫秒   注意这个uint16_t，是有可能被interrupt打断的 记得要用关闭全局中断
volatile uint8_t  reach_1s = 0;   // “到 1 秒了”的小事件计数

ISR(TIMER2_COMPA_vect){
	cc = !cc;
	b0_debounce_1ms();				//检测B0和B1的下降沿信号，并且消抖
	b1_debounce_1ms();
	howmany_ms++;
	if (howmany_ms>=1000){
		reach_1s++;							//满一秒的事件加1次，但不知道这个事件的次数会不会影响后面的逻辑关系
		howmany_ms = 0;
	}										// 满1000ms算1秒											// 扣掉1秒，保留碎片
	}

void initialise_hardware(void) {
	DDRA = 0xFF;			
	DDRC |= (1<<DDRC7)|(1<<DDRC3)|(1<<DDRC2)|(1<<DDRC1)|(1<<DDRC0);	
	DDRD |= (1<<DDRD7)|(1<<DDRD6)|(1<<DDRD5);
	_delay_ms(100);					//此处的delay是用来消除上电的抖动的，不会影响ssd和led的亮度
	//PCICR  |=  (1<<PCIE1);          // 打开 PCI2 组（PCINT23..16）
	//PCMSK2 |=  (1<<PCINT10);		// 打开 PINC4 具体使能
	TCCR2A = (1<<WGM21);
	TCCR2B = (1<<CS22);
	TIMSK2 = (1<<OCIE2A);
	OCR2A = 124;
	sei();
	EICRA |= (1<<ISC21);
	EIFR |= (1<<INTF2);
	EIMSK |= (1<<INT2);
} 
void power_led_light(PowerLevel power_level){
	
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


uint8_t index_mi = 0;
uint8_t s0_s1;
uint8_t time_remaining = 5; 
uint8_t cooking_time[10] = { 63,6,91,79,102,109,125,7,127,111 };
uint8_t init_time[4] = { 5,7,3,9 };
uint8_t mode_identify[4] = { 103,115,124,94 };
	
void mode_selection(void){
	s0_s1 = ((PINC>>5)&0x03);											//模式选择需要读取pin脚的值
	uint8_t mask_code = 1<<(s0_s1);
	PORTC = (PORTC&0xF0)|mask_code;										//得到末两位的s0和s1，这个末两位对应了一个值，可以用来后续操作
	index_mi = s0_s1;
	time_remaining = init_time[s0_s1];
}

void display_digit(uint8_t time_remaining, uint8_t index_mi, uint8_t cc)
{
	PORTC = (0x7F&PINC)|((0x01&cc)<<7);         //把位选cc输出到c端口上
	if (cc==0){									//当cc=0的时候，显示右边，cc=1的时候，显示左边，这里最好修改index的名字，改成time_remaining
		PORTA = cooking_time[time_remaining];	
	} 
	else{
		PORTA = mode_identify[index_mi];
	}
}

//void ssd_display(){
														//得到末两位的s0和s1，这个末两位对应了一个值，可以用来后续操作							// 直接使用 s0_s1 作为模式索引
	//display_digit(time_remaining, index_mi, cc);
	
//}


volatile uint8_t b0_released=0;
volatile uint8_t b1_released=0;

static inline uint8_t read_b0(void){
	return ((PINB&0x01)?1:0);	
}

void b0_debounce_1ms(void){
	static uint8_t candidate_0 = 0;
	static uint8_t reported_0  = 0;
	static uint8_t cnt_0 = 0;

	uint8_t now = read_b0();

	if(now == candidate_0){ if(cnt_0 < 255) cnt_0++; }
	else { cnt_0 = 0; candidate_0 = now; }

	if(cnt_0 >= 25){
		if(reported_0 == 1 && candidate_0 == 0){
			if(b0_released < 255) b0_released++;
		}
		reported_0 = candidate_0;
	}
}				

static inline uint8_t read_b1(void){
	return (PINB & (1<<PINB1)) ? 1 : 0;
}

void b1_debounce_1ms(void){
	static uint8_t candidate_1 = 0;
	static uint8_t reported_1  = 0;
	static uint8_t cnt_1 = 0;

	uint8_t now = read_b1();

	if(now == candidate_1){ if(cnt_1 < 255) cnt_1++; }
	else { cnt_1 = 0; candidate_1 = now; }

	if(cnt_1 >= 25){
		if(reported_1 == 1 && candidate_1 == 0){
			if(b1_released < 255) b1_released++;
		}
		reported_1 = candidate_1;
	}
}

static uint8_t last_s0_s1;
uint8_t now_s0_s1;

void run_microwave() {
	if (!is_running && !is_paused) {
		/*now_s0_s1 = ((PINC>>5)&0x03);
		if(last_s0_s1 != now_s0_s1){
			index_mi = now_s0_s1;
			time_remaining = init_time[now_s0_s1];
			last_s0_s1 = now_s0_s1;
		}
		else{;} */
		show_power();
		mode_selection();
		last_s0_s1 = ((PINC>>5)&0x03);
		display_digit(time_remaining,index_mi,cc);					//显示初始化数码管的内容（如果前面没有进入if）
		while (b0_released){
			b0_released = 0;
			cli();
			reach_1s   = 0;										// 清掉已排队的 1秒事件  行之有效
			howmany_ms = 0;									    // 从整秒重新起步
			sei();
			is_running = true;									//从running开始计数，所以在这之前加一些东西限制1s
			is_paused = false;
			}
		}
		else if (is_running) {
			show_power();
			display_digit(time_remaining,index_mi,cc);
			if (reach_1s) {
				reach_1s = 0;  
				if (time_remaining > 0) {
					time_remaining--;		
				}else {
					now_s0_s1 = ((PINC>>5)&0x03);
						if (now_s0_s1 != last_s0_s1){
							last_s0_s1 = now_s0_s1;
							is_running = false;
						}
				}
			}
			while(b0_released){
				show_power();
				b0_released = 0;
				howmany_ms = 0;
				if (time_remaining < 9)
				{
					time_remaining = time_remaining + 1;
				}else {
					time_remaining = 9;
				}
			}  
			
			}
		 
		else if (is_paused) {
		// PAUSED STATE - timer stopped, awaiting resume or reset.
	} 
}

int main(void)
{
	initialise_hardware();			//必要的硬件初始设置
	is_running = false;				//初始的状态机状态
	is_paused = false;
		
	while (true) {
		run_microwave();       //只用一个状态驱动机来实现所有功能
	}
}