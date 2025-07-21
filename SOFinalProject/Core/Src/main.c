#include "main.h"
#include "usb_device.h"
#include "usbd_cdc_if.h"
#include <string.h>
#include <stdio.h>

#define BUFFER_SIZE 10

ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;
UART_HandleTypeDef huart2;

uint16_t buffer1[BUFFER_SIZE] = {0};
uint16_t buffer2[BUFFER_SIZE] = {0};
uint16_t buffer3[BUFFER_SIZE] = {0};
uint16_t buffer4[BUFFER_SIZE] = {0};

uint16_t raw[4] = {0};
uint16_t bufferIndex = 0;
uint8_t convCompleted = 0;

extern USBD_HandleTypeDef hUsbDeviceFS;
extern uint8_t UserTxBufferFS[];

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_ADC1_Init(void);
static void MX_USART2_UART_Init(void);
uint16_t media_movel(uint16_t *buffer);

/* Média móvel */
uint16_t media_movel(uint16_t *buffer) {
    uint32_t soma = 0;
    for (int i = 0; i < BUFFER_SIZE; i++) {
        soma += buffer[i];
    }
    return soma / BUFFER_SIZE;
}

/* Callback do ADC DMA */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc) {
    if (hadc->Instance == ADC1) {
        convCompleted = 1;
    }
}

/* MAIN */
int main(void) {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_ADC1_Init();
    MX_USART2_UART_Init();
    MX_USB_DEVICE_Init();

    char Buffer[64];

    HAL_ADC_Start_DMA(&hadc1, (uint32_t*)raw, 4);

    while (1) {
        if (convCompleted) {
            convCompleted = 0;

            buffer1[bufferIndex] = raw[0];
            buffer2[bufferIndex] = raw[1];
            buffer3[bufferIndex] = raw[2];
            buffer4[bufferIndex] = raw[3];

            bufferIndex = (bufferIndex + 1) % BUFFER_SIZE;

            uint8_t btn1 = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_1);
            uint8_t btn2 = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_0);

            //aqui invertemos a logica para os botoes
            btn1 = !btn1;
            btn2 = !btn2;

            //gambiarra, nao reproduzir em casa
            if (btn1 == 1) {
                btn2 = 0;
            }

            uint16_t pot1 = media_movel(buffer1);
            uint16_t pot2 = media_movel(buffer2);
            uint16_t pot3 = media_movel(buffer3);
            uint16_t pot4 = media_movel(buffer4);

            // Envio via UART
            sprintf(Buffer, "aX:%4d aY:%4d bX:%4d bY:%4d | BTN1:%d BTN2:%d\r\n", pot1, pot2, pot3, pot4, btn1, btn2);
            HAL_UART_Transmit(&huart2, (uint8_t*)Buffer, strlen(Buffer), 100);

            // Envio via USB
            CDC_Transmit_FS((uint8_t*) Buffer, strlen(Buffer));

            HAL_ADC_Start_DMA(&hadc1, (uint32_t*)raw, 4);
        }


    }
}

/* CLOCK CONFIG */
void SystemClock_Config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 4;
    RCC_OscInitStruct.PLL.PLLN = 96;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 4;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                                |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV2;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1);
}

/* ADC CONFIG - 12 BITS */
static void MX_ADC1_Init(void) {
    ADC_ChannelConfTypeDef sConfig = {0};
    hadc1.Instance = ADC1;
    hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
    hadc1.Init.Resolution = ADC_RESOLUTION_12B; // <--- RESOLUÇÃO 12 BITS
    hadc1.Init.ScanConvMode = ENABLE;
    hadc1.Init.ContinuousConvMode = DISABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion = 4;
    hadc1.Init.DMAContinuousRequests = ENABLE;
    hadc1.Init.EOCSelection = ADC_EOC_SEQ_CONV;
    HAL_ADC_Init(&hadc1);

    sConfig.SamplingTime = ADC_SAMPLETIME_480CYCLES;

    sConfig.Channel = ADC_CHANNEL_0; sConfig.Rank = 1;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);

    sConfig.Channel = ADC_CHANNEL_1; sConfig.Rank = 2;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);

    sConfig.Channel = ADC_CHANNEL_4; sConfig.Rank = 3;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);

    sConfig.Channel = ADC_CHANNEL_8; sConfig.Rank = 4;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);
}

/* UART2 CONFIG - PA2 (TX), PA3 (RX) */
static void MX_USART2_UART_Init(void) {
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 115200;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart2);
}

/* DMA CONFIG */
static void MX_DMA_Init(void) {
    __HAL_RCC_DMA2_CLK_ENABLE();
    HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);
}

/* GPIO CONFIG */
static void MX_GPIO_Init(void) {
	 GPIO_InitTypeDef GPIO_InitStruct = {0};

	 __HAL_RCC_GPIOC_CLK_ENABLE();
	 __HAL_RCC_GPIOH_CLK_ENABLE();
	 __HAL_RCC_GPIOA_CLK_ENABLE();
	 __HAL_RCC_GPIOB_CLK_ENABLE();
	 // Configura PA4 e PA5 como entrada com pull-up (botões)
	 GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1;
	 GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	 GPIO_InitStruct.Pull = GPIO_PULLUP;
	 HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

/* ERRO */
void Error_Handler(void) {
    __disable_irq();
    while (1) {}
}
