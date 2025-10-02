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

volatile uint16_t howmany_ms_forbuzz = 0;
volatile uint8_t  reach_250ms = 0;

ISR(INT2_vect){								//�����ж�B2�½��ص�isr��������B2�ĵ�ƽ�����仯��ʱ�򣬼���Ƿ�Ϊ�½��أ�����ǣ��Ǿ�ʵ��power_cont++
	power_cont++;
	TCCR1A |= (1<<COM1B1);
	OCR1A = 195;
	OCR1B = 124;
	reach_250ms = 0;
	howmany_ms_forbuzz = 0;
	if(reach_250ms){
		reach_250ms = 0;
		howmany_ms_forbuzz =0;
		TCCR1A = TCCR1A&0b11011111;
	}
	
}

static volatile uint8_t cc;    //��ʼ��cc������ֻ��ʼ��һ�β��ҿ��Դ��θ�isr
volatile uint16_t howmany_ms = 0;   // �ۼƹ��˶��ٺ���   ע�����uint16_t�����п��ܱ�interrupt��ϵ� �ǵ�Ҫ�ùر�ȫ���ж�
volatile uint8_t  reach_1s = 0;   // ���� 1 ���ˡ���С�¼�����


ISR(TIMER2_COMPA_vect){
	cc = !cc;
	b0_debounce_1ms();				//���B0��B1���½����źţ���������
	b1_debounce_1ms();
	howmany_ms++;
	howmany_ms_forbuzz++;
	//howmany_ms_forjingle++;
	while (howmany_ms>=1000){
		reach_1s++;							//��һ����¼���1�Σ�����֪������¼��Ĵ����᲻��Ӱ�������߼���ϵ
		howmany_ms = 0;
	}										// ��1000ms��1��											// �۵�1�룬������Ƭ
	while (howmany_ms_forbuzz>=250){
		reach_250ms++;							//��һ����¼���1�Σ�����֪������¼��Ĵ����᲻��Ӱ�������߼���ϵ
		howmany_ms_forbuzz = 0;
	}
	
}

void initialise_hardware(void) {
	DDRA = 0xFF;			
	DDRC |= (1<<DDRC7)|(1<<DDRC3)|(1<<DDRC2)|(1<<DDRC1)|(1<<DDRC0);	
	DDRD |= (1<<DDRD7)|(1<<DDRD6)|(1<<DDRD5)|(1<<DDRD4);
	_delay_ms(100);					//�˴���delay�����������ϵ�Ķ����ģ�����Ӱ��ssd��led������
	TCCR2A = (1<<WGM21);
	TCCR2B = (1<<CS22);
	TIMSK2 = (1<<OCIE2A);
	OCR2A = 124;
	sei();
	EICRA |= (1<<ISC21);
	EIFR |= (1<<INTF2);
	EIMSK |= (1<<INT2);
	TCCR1A = (1<<WGM11)|(1<<WGM10);											//����timer1��ʱ���������Ȳ�����OC1B��������
	TCCR1B = (1<<CS11)|(1<<CS10)|(1<<WGM12)|(1<<WGM13);
	OCR1A = 255;
	OCR1B = 124;
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
	s0_s1 = ((PINC>>5)&0x03);											//ģʽѡ����Ҫ��ȡpin�ŵ�ֵ
	uint8_t mask_code = 1<<(s0_s1);
	PORTC = (PORTC&0xF0)|mask_code;										//�õ�ĩ��λ��s0��s1�����ĩ��λ��Ӧ��һ��ֵ������������������
	index_mi = s0_s1;
	time_remaining = init_time[s0_s1];
}

void display_digit(uint8_t time_remaining, uint8_t index_mi, uint8_t cc)
{
	PORTC = (0x7F&PINC)|((0x01&cc)<<7);         //��λѡcc�����c�˿���
	if (cc==0){									//��cc=0��ʱ����ʾ�ұߣ�cc=1��ʱ����ʾ��ߣ���������޸�index�����֣��ĳ�time_remaining
		PORTA = cooking_time[time_remaining];	
	} 
	else{
		PORTA = mode_identify[index_mi];
	}
}


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
		show_power();
		mode_selection();
		last_s0_s1 = ((PINC>>5)&0x03);
		display_digit(time_remaining,index_mi,cc);					//��ʾ��ʼ������ܵ����ݣ����ǰ��û�н���if��
		if(reach_250ms){
			reach_250ms = 0;
			howmany_ms_forbuzz =0;
			TCCR1A = TCCR1A&0b11011111;
		}
		while (b0_released){
			b0_released = 0;
			TCCR1A |= (1<<COM1B1);
			OCR1A = 255;
			OCR1B = 124;
			cli();												//��Ϊ��uint16_t���п��ܻᱻinterrupt���
			reach_250ms = 0;
			howmany_ms_forbuzz = 0;								//���е���˼�ǣ����ڲſ�ʼ�����200ms��ʱ��
			reach_1s   = 0;										// ������Ŷӵ� 1���¼�  ��֮��Ч
			howmany_ms = 0;									    // ���е���˼�ǣ����ڲſ�ʼ�����1000ms��ʱ��
			sei();
			is_running = true;									//��running��ʼ��������������֮ǰ��һЩ��������1s
			is_paused = false;
			}
		}
		else if (is_running) {
			show_power();
			display_digit(time_remaining,index_mi,cc);
			if(reach_250ms){
				reach_250ms = 0;
				howmany_ms_forbuzz =0;
				TCCR1A = TCCR1A&0b11011111;
			}
			if (reach_1s) {
				reach_1s = 0;  
				if (time_remaining > 0) {
					time_remaining--;		
				}else {															//��ʱ����ʱ�Ѿ�Ϊ0
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
				TCCR1A |= (1<<COM1B1);
				OCR1A = 255;
				OCR1B = 124;
				howmany_ms_forbuzz = 0;
				reach_250ms = 0;
				if (time_remaining < 9)
				{
					time_remaining = time_remaining + 1;
				}else {
					time_remaining = 9;
				}
			}  
			while (b1_released)
			{
				show_power();
				b1_released = 0;
				howmany_ms = 0;
				TCCR1A |= (1<<COM1B1);
				OCR1A = 155;
				OCR1B = 124;
				howmany_ms_forbuzz = 0;
				reach_250ms = 0;
				is_paused = true;
				is_running = false;
				
			}
		}
		 
		else if (is_paused) {
			show_power();
			display_digit(time_remaining,index_mi,cc);
			if(reach_250ms){
				reach_250ms = 0;
				howmany_ms_forbuzz =0;
				TCCR1A = TCCR1A&0b11011111;
			}
			while(b1_released){
				show_power();
				b1_released = 0;
				howmany_ms = 0;
				TCCR1A |= (1<<COM1B1);
				OCR1A = 155;
				OCR1B = 124;
				howmany_ms_forbuzz = 0;
				reach_250ms = 0;
				is_paused = false;
				is_running = false;
			}
			while(b0_released){
				show_power();
				b0_released = 0;
				howmany_ms = 0;
				TCCR1A |= (1<<COM1B1);
				OCR1A = 255;
				OCR1B = 124;
				howmany_ms_forbuzz = 0;
				reach_250ms = 0;
				is_paused = false;
				is_running = true;
			}
		
	} 
}

int main(void)
{
	initialise_hardware();			//��Ҫ��Ӳ����ʼ����
	is_running = false;				//��ʼ��״̬��״̬
	is_paused = false;
		
	while (true) {
		run_microwave();       //ֻ��һ��״̬��������ʵ�����й���
	}
}