#include "main.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define MOTOR_PWM_MAX 999
#define SERVO_MIN_US 650
#define SERVO_MAX_US 2350
#define SERVO_CENTER_US 1500
#define TELEMETRY_PERIDO_MS 50

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;
UART_HandleTypeDef huart2;

void SystemClock_config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM1_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM3_Init(void);
static void Force_Hardware_Config(void);

int main(void){
    // 1. 초기화
    HAL_Init();
    SystemClock_config();

    MX_GPIO_Init();
    MX_USART2_UART_Init();
    MX_TIM1_Init();
    MX_TIM2_Init();
    MX_TIM3_Init();

    // 2. 핀 강제 설정
    Force_Hardware_Config();

    // 3. 장치 시작
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1); //DC
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3); //Servo
    HAL_TIM_Encoder_Start(&htim1, TIM_CHANNEL_ALL); //Encoder
    __HAL_TIM_SET_COUNTER(&htim1, 0);

    // 4. 변수 초기화
    uint8_t rx_data; //1바이트 수신용
    uint8_t buffer[64]; //문자열 조립용 버퍼
    uint8_t buf_index = 0; //buffer index

    uint32_t last_telemetry_time = 0;
    uint32_t last_command_time = HAL_GetTick();
    uint16_t prev_cnt = (uint16_t)__HAL_TIM_GET_COUNTER(&htim1);

    // 5. 메인 루프
    while(1)
    {
        // UART 수신 (Polling)
        if (HAL_UART_Receive(&huart2, &rx_data, 1, 1) == HAL_OK)
        {
            if(rx_data == '\r') continue;

            if(rx_data == '\n')
            {
                buffer[buf_index] = 0; //문자열 끝 맺음

                int speed_cmd = 0;
                int angle_us = SERVO_CENTER_US;

                //파싱 시도
                if(sscanf((char*)buffer, "%d,%d", &speed_cmd, &angle_us)==2)
                {
                    last_command_time = HAL_GetTick();

                    // 디버그 - 명령 수신 확인용 LD2 토글
                    HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);

                    // DC
                    if(speed_cmd > MOTOR_PWM_MAX) speed_cmd = MOTOR_PWM_MAX;
                    if(speed_cmd < MOTOR_PWM_MAX) speed_cmd = -MOTOR_PWM_MAX;

                    if(speed_cmd > 0){
                        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_SET); //전진
                        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
                        __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, (uint32_t)speed_cmd);
                    }
                    else if(speed_cmd < 0){
                        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET); //후진
                        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RET); 
                        __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, (uint32_t)abs(speed_cmd));
                    }
                    else{
                        //정지 시 브레이크 (둘 다 HIGH or LOW) -> 여기선 Free Wheel (LOW/LOW)
                        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET);
                        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
                        __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 0);
                    }

                    //Servo
                    if(angle_us < SERVO_MIN_US) angle_us = SERVO_MIN_US;
                    if(angle_us > SERVO_MAX_US) angle_us = SERVO_MAX_US;
                    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, (uint32_t)angle_us);
                }
                else
                {
                    // 파싱 실패 시 디버깅
                }
                
                // buffer 및 인덱스 초기화
                buf_index = 0;
                memset(buffer, 0, sizeof(buffer));
            }
            else
            {
                //일반 문자 저장
                if(buf_index < 60) buffer[buf_index++] = rx_data;
                else buf_index = 0; //버퍼 꽉 차면 강제 초기화 (overflow 방지)
            }
        }
    }

    // 2. FailSafe
    if(HAL_GetTick() - last_command_time > 500)
    {
        __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 0);
        // LED끄기 (연결 끊김 표시)
        HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
    }

    // 3. Telemetry 전송
    if(HAL_GetTick() - last_telemtry_time >= TELEMETRY_PERIDO_MS)
    {
        // 엔코더 값 읽기
        uint16_t current_cnt = (uint16_t)__HAL_TIM_GET_COUNTER(&htim1);
        int16_t delta = (int16_t)(current_cnt - prev_cnt);

        prev_cnt = current_cnt;

        int32_t speed_pps = (int32_t)delta * (1000 / TELEMETRY_PERIDO_MS);

        char tx_buffer[32];

        snprintf(tx_buffer, sizeof(tx_buffer), "ENC:%ld\n", (long)speed_pps);
        HAL_UART_Transmit(&huart2, (uint8_t)tx_buffer, (uint16_t)strlen(tx_buffer), 10);

        last_telemetry_time = HAL_GetTick();
    }

    // 위까지 다시 복습함.
    //========================================

    /* --- 초기화 및 설정 함수들 --- */

    void SystemClock_Config(void)
    {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM = 16;
    RCC_OscInitStruct.PLL.PLLN = 336;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
    RCC_OscInitStruct.PLL.PLLQ = 4;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2);
    }

    static void MX_TIM1_Init(void)
    {
    __HAL_RCC_TIM1_CLK_ENABLE();
    TIM_Encoder_InitTypeDef sConfig = {0};
    TIM_MasterConfigTypeDef sMasterConfig = {0};
    htim1.Instance = TIM1;
    htim1.Init.Prescaler = 0;
    htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim1.Init.Period = 65535;
    htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim1.Init.RepetitionCounter = 0;
    htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    sConfig.EncoderMode = TIM_ENCODERMODE_TI12;
    sConfig.IC1Polarity = TIM_ICPOLARITY_RISING;
    sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
    sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
    sConfig.IC1Filter = 0;
    sConfig.IC2Polarity = TIM_ICPOLARITY_RISING;
    sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
    sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
    sConfig.IC2Filter = 0;
    HAL_TIM_Encoder_Init(&htim1, &sConfig);
    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig);
    }

    static void MX_TIM2_Init(void)
    {
    TIM_OC_InitTypeDef sConfigOC = {0};
    htim2.Instance = TIM2;
    /* [수정 포인트] Prescaler 변경 */
        // 기존: 99 (840Hz) -> 토크 부족, 코너링 시 멈춤
        // 변경: 1399 (60Hz) -> 토크 14배 상승 효과 (Phase 1과 동일한 "Kick") 중요함.
    htim2.Init.Prescaler = 1399;
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = 999;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    HAL_TIM_PWM_Init(&htim2);
    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = 0;
    HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1);
    }

    static void MX_TIM3_Init(void)
    {
    TIM_OC_InitTypeDef sConfigOC = {0};
    htim3.Instance = TIM3;
    htim3.Init.Prescaler = 83;
    htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim3.Init.Period = 19999;
    htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    HAL_TIM_PWM_Init(&htim3);
    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = SERVO_CENTER_US;
    HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_3);
    }

    /* ★ 핀 강제 설정 유지 ★ */
    static void MX_USART2_UART_Init(void)
    {
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 115200;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;

    /* PA2, PA3 강제 활성화 */
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USART2_CLK_ENABLE();
    GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    if (HAL_UART_Init(&huart2) != HAL_OK)
    {
        Error_Handler();
    }
    }

    static void MX_GPIO_Init(void)
    {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1|GPIO_PIN_4|LD2_Pin, GPIO_PIN_RESET);
    GPIO_InitStruct.Pin = GPIO_PIN_1|GPIO_PIN_4|LD2_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    }

    static void Force_Hardware_Config(void)
    {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    /* TIM2_CH1 -> PA0 */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitStruct.Pin = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF1_TIM2;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    /* TIM3_CH3 -> PB0 */
    __HAL_RCC_GPIOB_CLK_ENABLE();
    GPIO_InitStruct.Pin = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF2_TIM3;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    /* TIM1 Encoder -> PA8/PA9 */
    GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF1_TIM1;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    }

    void Error_Handler(void)
    {
    __disable_irq();
    while (1) {}
    }



// 1. static을 함수에 붙이는 의미
// 2. uint8_t buffer[64]; 이면 buffer를 64바이트 할당하는 것인가, 
// 그리고 그게 메모리 저장인가
// 3. uint8_t, uint16_t, uint32_t에서 숫자의 의미는 무엇인가,
// 그리고 어떻게 그 숫자를 정해야 하는가


코드 따라 작성하면서 몇 가지 질문 생겼습니다. 아래에 적겠습니다. 면접에서 나온다고 상상하면서 답변 못 할 것 같은 것들 중심으로 가지고 왔습니다.


1. static을 함수에 붙이는 의미

2. uint8_t buffer[64]; 이면 buffer를 64바이트 할당하는 것인가, 그리고 그게 메모리 저장인가

3. uint8_t, uint16_t, uint32_t에서 숫자의 의미는 무엇인가, 그리고 어떻게 그 숫자를 정해야 하는가