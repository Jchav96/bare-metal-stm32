#include"stm32Bluepill.h"

uint32_t clockFreq; // Global variable to hold clock frequency

void setupTimer(TIM_TypeDef *TIMx,uint32_t prescalar, uint32_t arr, uint32_t dutyCyclePercent){
	if(TIMx == TIM1){
		RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;
	}else if(TIMx == TIM2){
		RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
	}else if(TIMx == TIM3){
		RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
	}
	TIMx->PSC = prescalar-1; // The prescalar starts at zero, hence the subtracting one
	TIMx->ARR = arr; // The arr determines at which value does the timer reset when counting
	// Calculating the duty cycle percent and sending it to the capture compare register 1
	TIMx->CCR1 = (arr * dutyCyclePercent)/100;
	// Enable PWM Mode (This is where the fun begins...)
	// This involves writing to Capture Compare Mode Register 1
	TIMx->CCMR1 = 104; // See register mapping...
	// Enable the output on Capture Compare enable Register
	TIMx->CCER |= TIM_CCER_CC1E;
	// Enable the Timer counter on the control register
	TIMx->CR1 |= TIM_CR1_CEN;
}

void setupClock(uint32_t frequency){
	clockFreq = frequency; // pass inputted frequency to global variable for delay function use...
	RCC->CR |= RCC_CR_HSEON; // Enable the high speed external clock
	while(!(RCC->CR & RCC_CR_HSERDY)); // Do nothing until the External High speed clock is ready
	// Set the flash latency based on the inputted frequency
	if(frequency <= 24000000){
		FLASH->ACR = FLASH_ACR_LATENCY_0;
	}else if(frequency <= 48000000){
		FLASH->ACR = FLASH_ACR_LATENCY_1;
	}else{
		FLASH->ACR = FLASH_ACR_LATENCY_2;
	}
	/*
	 * SYS clock has a max frequency of 72MHz resulting in the 3 conditionals between
	 * Flash latencies 0,1, and 2
	 */
	// Configure the PLL to reach the target frequency from the HSE
	RCC->CR &= ~RCC_CR_PLLON; // Turn the PLL off prior to config
	RCC->CFGR |= RCC_CFGR_PLLSRC; // Use HSE and set PLL as the source
	// Enable the PLL and wait for it to stabilize...
	// HSI RC (High Speed internal RC Oscillator) has a frequency of 8MHz
	uint32_t hsiFreq = 8000000;
	uint32_t pllMultiplier = frequency / hsiFreq; // This is needed to set the clock up
												  // at higher frequencies
	if(pllMultiplier < 2 || pllMultiplier > 16){
		pllMultiplier = 9; // Default to 9 for out of range values
	}
	// Clearing the PLL Multiplier bits
	RCC->CFGR &= ~RCC_CFGR_PLLMULL;
	// Set the pll multiplier bit's accordingly
	// The pll bits are 18, 19,20, and 21
	RCC->CFGR |= (pllMultiplier - 2) << 18;
	/* Small snippet from official documentation...
	 *
	    Bits 21:18 PLLMUL[3:0]: PLL multiplication factor
		These bits are written by software to define the PLL multiplication factor. They can be written only
		when PLL is disabled.
		000x: Reserved
		0010: PLL input clock x 4
		0011: PLL input clock x 5
		0100: PLL input clock x 6
		0101: PLL input clock x 7
		0110: PLL input clock x 8
		0111: PLL input clock x 9
		10xx: Reserved
		1100: Reserved
		1101: PLL input clock x 6.5
		111x: Reserved
		Caution: The PLL output frequency must not exceed 72 MHz.

	   This is where the pllMultiplier -2 part comes from as the binary value's (pll input's) and
	   the corresponding decimal output's are 2 apart (binary being 2 higher hence the subtracting...)
	 */
	// Enabling the PLL & waiting for it to stabilize
	RCC->CR |= RCC_CR_PLLON;
	while(!(RCC->CR & RCC_CR_PLLRDY)){}
	// Set the PLL as the system clock source
	RCC->CFGR &= ~RCC_CFGR_SW;
	RCC->CFGR |= RCC_CFGR_SW_PLL;
	while((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL){}
}

void delay(uint32_t milliseconds){
	// Set up TIM2 with the correct configuration
	//setupTimer(TIM2,(clockFreq / 1000) - 1, milliseconds, 100);
	setupTimer(TIM2,(1000 / clockFreq) - 1,milliseconds,100);
	// Reset the counter register to start the count from zero
	TIM2->CNT = 0;
	//clear the update interrupt flag
	TIM2->SR &= ~TIM_SR_UIF;
	// Wait until the Timer x status register's interrupt flag bit is cleared
	while(!(TIM2->SR & TIM_SR_UIF)){}
	// Turn off TIM2 after the delay to save power
	TIM2->CR1 &= ~TIM_CR1_CEN;
}

void initializeUART(uint32_t baudRate){
	/*
	  Using USART1 as a means for UART communication...
	  The difference between UART and USART is that USART is Synchronous/ Asynchronous while UART
	  is not (hence the extra s, UART only operates in asynchronous mode). As a result, UART 1 which
	  is connected to APB2 can be used (in asynchronous mode). Pins A9 (TX1) and A10 (RX1) will be used
	 */
	// Enable the clock for USART 1 and GPIOA
	RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
	RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
	// Set PA9 & PA10 for alternate function push pull and input floating respectively
	GPIOA->CRH &= ~(GPIO_CRH_MODE9 | GPIO_CRH_CNF9); // clear the mode and configuration bits
	GPIOA->CRH |= (GPIO_CRH_MODE9_1 | GPIO_CRH_CNF9_1); // set PA9 to output mode push pull (1 for output)
	GPIOA->CRH &= ~(GPIO_CRH_MODE10 | GPIO_CRH_CNF10);
	GPIOA->CRH |= GPIO_CRH_MODE10_0; // Set PA10 as an input (RX)
	USART1->BRR = 72000000 / baudRate;
    USART1->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
}

void uartSendChar(char character){
	// Wait until the transmit buffer is empty
	while(!(USART1->SR & USART_SR_TXE)){}
	// Send the character into the data register
	USART1->DR = character;
	//wait until the character has been sent
	while(!(USART1->SR & USART_SR_TC)){}
}

void uartWrite(const char *message){
	while(*message){
		if(*message == '\n'){
			uartSendChar('\r');
		}
		uartSendChar(*message++);
	}
}

// A push button has been connected to B12, as such, a function should be made to establish
// the push button as an input
void pushbuttoninitialize(){
	// enable the clock for the port
	RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;
	// clear the mode and configuration bits
	GPIOB->CRH &= ~(GPIO_CRH_MODE13 | GPIO_CRH_CNF13);
	// Set PB12 as an input
	GPIOB->CRH |= GPIO_CRH_MODE13_1;
	// Setting the pin as a pull down
	GPIOB->ODR |= GPIO_IDR_IDR13;
}

bool isTheButtonPressed(){
	while(GPIOB->IDR & GPIO_IDR_IDR13){
		delay(250);
		if((GPIOB->IDR & GPIO_IDR_IDR13)){
			return false;
		}
		return (!(GPIOB->IDR & GPIO_IDR_IDR13));
	}
	return false;
}

void toggleLED(){
	GPIOC->ODR ^= GPIO_ODR_ODR13;
}

void ledWrite(char state){
	if(state == 'low' | state == 'LOW' | state == 'Low'){
		// Set the LED LOW
		GPIOC->IDR &= ~GPIO_IDR_IDR13;
	}
	else if(state == 'high' | state == 'HIGH' | state == 'High'){
		// Set the LED HIGH
		GPIOC->IDR |= GPIO_IDR_IDR13;
	}
}

void setupLED(){
	// The onboard LED is connected to Pin C13 and will be set as an output
	// ALL GPIO Pins are connected to APB2...
	RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;
	// Enable the pin as an output...
	GPIOC->CRH &= ~(GPIO_CRH_MODE13 | GPIO_CRH_CNF13);
	GPIOC->CRH |= (1 << GPIO_CRH_MODE13_Pos);          // Set mode to output 2 MHz
}

void Error_Handler(void){
	while(1);
}
