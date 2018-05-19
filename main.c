//a frequency meter based on PIC12F675, with analog / PWM output
//
//2 ranges:
// - high (<20Mhz)
// - low (<2Mhz)
//
// connection:
//
//
//
//                               |---------------------|
//                               |                     |
//                               |                     |
//  Fin------------------------->| T1CKI/GP5           |
//                               |                     |
//                               |                     |
//                               |                     |
//                               |     PIC12F675       |                   -----[Radj]----[ua]--->GND
//                               |                     |                   |
//                               |                     |                   |
//                               |             PWM_PIN |>--------[ R1 ]-----------|22uf|--------->GND
//                               |                     |
//                               |                     |
//                               |             LED_PIN |>--------[ 1k ]-------------|>|---------->GND
//                               |                     |
//                               |---------------------|
//
//
//LED_PIN: goes high to indicate frequency range of 0..20Mhz, low to indicate frequency range of 0..2Mhz
//
//PWM_PIN: output a PWm pulse train
//
//Radj choosen to match reading of ua / ammeter.
//for example: for 200ua full-range ammeter / multimeter, pick Radj so that Vcc/(R1 + Radj) = 200ua. For Vcc=5v, R1 + Radj ~=25K
//
//
//Calibration:
//1. apply a known frequency source to T1CKI;
//2. adjust Radj to achieve the reading on ua/ammeter
//

#include "config.h"						//configuration words
#include "gpio.h"                           //we use gpio functions
#include "delay.h"                          //we use software delays

//hardware configuration
//#define FREQ_PORT			GPIO
//#define FREQ_DDR			TRISIO
//#define FREQ_PIN			(1<<5)			//T1CKI pin / GP5

#define PWM_PORT			GPIO
#define PWM_DDR				TRISIO
#define PWM_PIN				(1<<0)			//PWM pin
#define PWM_CNT				TMR0			//pwm counter

#define LED_PORT			GPIO
#define LED_DDR				TRISIO
#define LED_PIN				(1<<1)			//led indicator pin for high (set) / low range (clear)
//end hardware configuration

//global defines
#define FREQ_CNT			200				//***FIXED***, do not change. timer0 interrupts counter -> controls the length of gating
//gate time = 256 * 8 * 200 = 409,600us
#define FREQ_HIGH			20ul			//max frequency, in Mhz, above 10% of which LED_PIN is lit
//must be in whole Mhz range

#define FREQ2CNT(freq)		(freq) 
//global variables
volatile uint32_t freq;						//32-bit counter
volatile  uint8_t pwm_dc;					//pwm output compare register / duty cycle
volatile  uint8_t freq_cnt;					//current count of tmr0 interrupt invocation

//isr
void interrupt isr(void) {
	uint32_t freq10;						//=freq, used as temporary variable
	
	//tmr0 isr
	if (T0IF) {
		T0IF = 0;							//reset the flag
		freq_cnt -= 1;						//decrement isr counter downcounter
		if (freq_cnt == 0) {				//enough time has passed
			freq |= TMR1;					//update freq
			TMR1 = 0;						//reset tmr1
			//freq10 = X1024to1000(freq);	//freq10 is the right number now
			freq10 = freq;
			freq = 0;						//reset freq
			freq_cnt = FREQ_CNT;			//reset the downcounter
			//convert freq10 to pwm duty cycle
			//if (freq10 >= 256 * 8 * FREQ_CNT / 8 * FREQ_HIGH) {	//freq >= 20Mhz
			//if (freq10 >= 256ul * FREQ_CNT * FREQ_HIGH) {		//freq >= 20Mhz
			//	pwm_dc = 255;				//max duty cycle
			//	IO_SET(LED_PORT, LED_PIN);	//set LED
			//} else 
			if (freq10 >= 256ul * FREQ_CNT * FREQ_HIGH / 10) {	//freq >= 2Mhz, < 20Mhz
				//pwm_dc = freq10 * 8 / (256 * 8 * FREQ_CNT * FREQ_HIGH) * 256	//top at 20Mhz = 256	
				pwm_dc = freq10 / (FREQ_CNT * (FREQ_HIGH / 1));
				IO_SET(LED_PORT, LED_PIN);	//indicating high range
			} else {						//freq < 2Mhz
				//pwm_dc = freq10 * 8 / (256 * 8 * FREQ_CNT * FREQ_HIGH / 10) * 256ul 	//top at 2Mhz = 256
				pwm_dc = freq10 / (FREQ_CNT * (FREQ_HIGH / 10));
				IO_CLR(LED_PORT, LED_PIN);
			}		
		}	
	}		

	//tmr1 isr
	if (TMR1IF) {
		TMR1IF = 0;							//reset the flag
		freq += 0x10000ul;					//tmr1 is 16bit
	}
	
}
	
//initialize the meter
void freq_init(void) {
	//initialize the variables
	pwm_dc = 0;								//initialize pwm
	freq_cnt = FREQ_CNT;					//initialize freq_cnt. a down counter
	
	//initialize the led indicator
	IO_OUT(LED_DDR, LED_PIN);				//default as output
	
	//initialize the pwm pin: low and as output
	IO_CLR(PWM_PORT, PWM_PIN); IO_OUT(PWM_DDR, PWM_PIN);
	
	//initialize the pulse input pin: input
	//IO_IN(FREQ_DDR, FREQ_PIN);
	
	//initialize tmr0 as time base
	//8:1 prescaler
	T0CS = 0;								//0->count on Fcy=F_CPU
	PSA = 0;								//0->prescaler assigned to tmr0
	PS2=0, PS1=1, PS0=0;					//0b010->8x prescaler
	TMR0 = 0;								//reset tmr0
	
	T0IF = 0;								//0->reset the flag
	T0IE = 1;								//enable interrupt
	
	//initialize tmr1
	//8:1 prescaler
	TMR1ON = 0;								//0->disable the timer
	TMR1GE = 0;								//0->tmr1 not gated
	T1CKPS1= 1, T1CKPS0=1;					//0b11->8:1 prescaler
	T1OSCEN = 0;							//0->disable t1 low power oscillator
	nT1SYNC= 1;							//1->TMR1 not synchronized
	TMR1CS = 1;								//1->count on T1CKI
	TMR1 = 0;								//reset tmr1
	
	TMR1IF = 0;								//0->reset the flag
	TMR1IE = 1;								//1->enable interrupt
	PEIE = 1;								//1->enable peripheral interrupt
	
	TMR1ON = 1;								//1->turn on tmr1
}

	
int main(void) {
	
	mcu_init();							    //initialize the mcu
	freq_init();							//reset the frequency meter
	ei();									//enable the global interrupt
	while (1) {
		//generate the pwm output on PWM_PIN
		if (PWM_CNT > pwm_dc) IO_CLR(PWM_PORT, PWM_PIN);
		else IO_SET(PWM_PORT, PWM_PIN);
	}
}

