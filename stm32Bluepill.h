#ifndef STM32BLUEPILL_H_
#define STM32BLUEPILL_H_
#include<stdint.h>
#include<stdbool.h>
#include<stm32f103x6.h>

void setupTimer(TIM_TypeDef *TIMx,uint32_t prescalar, uint32_t arr, uint32_t dutyCyclePercent);
void setupClock(uint32_t frequency);
void delay(uint32_t milliseconds);
void toggleLED();
void setupLED();
void initializeUART(uint32_t baudRate);
void uartSendChar(char character);
void uartWrite(const char *message);
void ledWrite(char state);
void Error_Handler(void);
#endif /* STM32BLUEPILL_H_ */
