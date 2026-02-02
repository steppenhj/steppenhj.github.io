/* USER CODE BEGIN Header */
/**
  * @brief          : Neuro-Driver Firmware (Simple Polling Version)
  * @note           : [Study Note] 이 코드는 CPU가 모든 일을 순서대로 처리하는 'Super Loop' 방식입니다.
  * RTOS 도입 시 하드웨어 설정(TIM, GPIO)은 그대로 가져가고, while(1) 내부 로직만 태스크로 쪼개집니다.
  */
/* USER CODE END Header */

#include "main.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* =========================
 * [1] 튜닝 파라미터 (Tunables)
 * ========================= */
// PWM 해상도 (0 ~ 999). 값이 클수록 모터 전압이 높아짐.
#define MOTOR_PWM_MAX             999

// 서보모터 제어용 펄스 폭 (단위: us, 마이크로초)
// 보통 서보는 50Hz(20ms) 주기에서 1000~2000us 펄스로 각도를 제어함.
#define SERVO_MIN_US              650   // 오른쪽 끝
#define SERVO_MAX_US              2350  // 왼쪽 끝
#define SERVO_CENTER_US           1500  // 중앙
#define TELEMETRY_PERIOD_MS       50    // 엔코더 정보를 보낼 주기 (50ms = 초당 20회)

/* 핸들러 선언 (STM32 하드웨어 자원 관리자) */
TIM_HandleTypeDef htim1; // 엔코더 (타이머가 펄스 수를 자동으로 셈)
TIM_HandleTypeDef htim2; // DC 모터 PWM (속도 조절)
TIM_HandleTypeDef htim3; // 서보 PWM (조향 조절)
UART_HandleTypeDef huart2; // PC/노트북 통신

/* 함수 원형 */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM1_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM3_Init(void);
static void Force_Hardware_Config(void);

int main(void)
{
  /* 1. 초기화 (Initialization) */
  HAL_Init(); // HAL 라이브러리 초기화
  SystemClock_Config(); // 시스템 클럭 설정 (MCU 심장박동 속도 설정)

  // 주변장치(Peripherals) 초기화
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  MX_TIM1_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();

  /* 2. 핀 강제 설정 (중요!) */
  // CubeMX 자동 생성 코드가 꼬였을 때를 대비해, 핀 맵핑을 코드 레벨에서 확정 짓는 부분입니다.
  Force_Hardware_Config();

  /* 3. 장치 시작 (Start Peripherals) */
  // [RTOS 이식 포인트] 이 부분은 RTOS 태스크 시작 전에 하드웨어를 깨우는 용도로 그대로 사용됩니다.
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);       // DC Motor PWM 시작
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);       // Servo PWM 시작
  HAL_TIM_Encoder_Start(&htim1, TIM_CHANNEL_ALL); // Encoder 카운팅 시작 (하드웨어가 알아서 셈)
  __HAL_TIM_SET_COUNTER(&htim1, 0);               // 카운터 0으로 리셋

  /* 4. 변수 초기화 */
  uint8_t rx_data;        // UART로 1바이트씩 받을 변수
  uint8_t buffer[64];     // 받은 문자를 모아서 문장으로 만들 버퍼
  uint8_t buf_index = 0;  // 버퍼의 현재 위치

  uint32_t last_telemetry_time = 0;           // 마지막으로 보고한 시간
  uint32_t last_command_time = HAL_GetTick(); // 마지막으로 명령 받은 시간 (Failsafe용)
  uint16_t prev_cnt = (uint16_t)__HAL_TIM_GET_COUNTER(&htim1); // 이전 엔코더 값

  /* 5. 메인 루프 (Super Loop) */
  // [RTOS 이식 포인트] 이 while(1) 안에 있는 내용들이 쪼개져서 각기 다른 Task로 들어갑니다.
    while (1)
    {
        /* ====================================================
         * [1] UART 수신 (Polling 방식 -> DMA로 변경 예정)
         * ==================================================== */
        // [Blocking] CPU가 1ms 동안 여기서 멈춰서 데이터가 오나 감시합니다.
        // 데이터가 안 오면 1ms를 그냥 날리는 셈입니다. (RTOS에서는 최악의 방식)
        if (HAL_UART_Receive(&huart2, &rx_data, 1, 1) == HAL_OK)
        {
            /* 1. 불필요한 문자 무시 */
            if (rx_data == '\r') { continue; }

            /* 2. 패킷 완성 검사 (개행 문자 '\n'이 오면 문장 끝) */
            if (rx_data == '\n')
            {
                buffer[buf_index] = 0; // 문자열 끝(Null) 표시

                int speed_cmd = 0;
                int angle_us  = SERVO_CENTER_US;

                /* 3. 파싱 (Parsing) - "속도,각도" 형태 분리 */
                // 예: "100,1500" -> speed_cmd=100, angle_us=1500
                if (sscanf((char*)buffer, "%d,%d", &speed_cmd, &angle_us) == 2)
                {
                    last_command_time = HAL_GetTick(); // 명령 시간 갱신 (워치독 밥 주기)

                    // [디버그] 명령 잘 받았다고 LED 깜빡임
                    HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);

                    /* --- DC 모터 제어 (H-Bridge) --- */
                    // [Safety] PWM 최대치 제한 (하드웨어 보호)
                    if (speed_cmd > MOTOR_PWM_MAX) speed_cmd = MOTOR_PWM_MAX;
                    if (speed_cmd < -MOTOR_PWM_MAX) speed_cmd = -MOTOR_PWM_MAX;

                    // 방향 결정 및 PWM 설정
                    // L298N 드라이버: IN1, IN2 상태에 따라 정/역회전 결정
                    if (speed_cmd > 0) {
                        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_SET);   // IN1 = High
                        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET); // IN2 = Low (전진)
                        __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, (uint32_t)speed_cmd);
                    } else if (speed_cmd < 0) {
                        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET); // IN1 = Low
                        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);   // IN2 = High (후진)
                        __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, (uint32_t)abs(speed_cmd));
                    } else {
                        // 정지 (Free Wheel)
                        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET);
                        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
                        __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 0);
                    }

                    /* --- 서보 모터 제어 --- */
                    // [Safety] 서보 기계적 한계 보호
                    if (angle_us < SERVO_MIN_US) angle_us = SERVO_MIN_US;
                    if (angle_us > SERVO_MAX_US) angle_us = SERVO_MAX_US;
                    
                    // 서보 PWM 펄스 폭 변경 -> 각도 변경
                    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, (uint32_t)angle_us);
                }
                
                // 버퍼 초기화 (다음 명령 받을 준비)
                buf_index = 0;
                memset(buffer, 0, sizeof(buffer));
            }
            else
            {
                /* 문자가 계속 들어오는 중이면 버퍼에 저장 */
                if (buf_index < 60) {
                    buffer[buf_index++] = rx_data;
                } else {
                    // 버퍼 넘침 방지 (Safety)
                    buf_index = 0;
                }
            }
        }

        /* =========================
         * [2] 안전장치 (Fail-safe)
         * ========================= */
        // 마지막 명령 수신 후 500ms(0.5초)가 지났으면 통신 끊김으로 간주
        if (HAL_GetTick() - last_command_time > 500)
        {
            // 모터 즉시 정지 (급발진 방지)
            __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 0);
            HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET); // LED 끔
        }

        /* =========================
         * [3] Telemetry (상태 보고)
         * ========================= */
        // 50ms마다 한 번씩 실행
        if (HAL_GetTick() - last_telemetry_time >= TELEMETRY_PERIOD_MS)
        {
            // 엔코더 카운터 값 읽기 (0 ~ 65535)
            uint16_t current_cnt = (uint16_t)__HAL_TIM_GET_COUNTER(&htim1);
            
            // 변화량 계산 (이번 값 - 지난 값)
            // int16_t로 캐스팅하는 이유: 65535에서 0으로 넘어가는(Overflow) 상황을 음수로 처리하기 위함
            int16_t delta = (int16_t)(current_cnt - prev_cnt);

            prev_cnt = current_cnt; // 현재 값을 과거 값으로 저장

            // 속도(PPS) 계산: 변화량 * (1초 / 주기)
            int32_t speed_pps = (int32_t)delta * (1000 / TELEMETRY_PERIOD_MS);

            char tx_buffer[32];
            snprintf(tx_buffer, sizeof(tx_buffer), "ENC:%ld\n", (long)speed_pps);
            
            // [Blocking] 데이터 전송 완료될 때까지 대기 (RTOS에서는 이것도 DMA로 바꿔야 함)
            HAL_UART_Transmit(&huart2, (uint8_t*)tx_buffer, (uint16_t)strlen(tx_buffer), 10);

            last_telemetry_time = HAL_GetTick();
        }
    }
}

/* --- 하단은 하드웨어 초기화 함수들 (CubeMX 자동 생성 + 수정) --- */
// [Study Note] 아래 설정들은 RTOS 프로젝트에서도 99% 동일하게 사용됩니다.

void SystemClock_Config(void)
{
  // MCU 클럭(심장박동) 설정. 84MHz 등으로 설정됨.
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
  // [엔코더 설정]
  // TIM_ENCODERMODE_TI12: A상, B상 펄스를 모두 세어서 해상도를 4배로 높임.
  // 하드웨어가 자동으로 카운터(CNT) 레지스터를 증가/감소시킴. CPU가 할 일 없음 (개이득).
  __HAL_RCC_TIM1_CLK_ENABLE();
  TIM_Encoder_InitTypeDef sConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 0;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 65535; // 16비트 최대값
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  sConfig.EncoderMode = TIM_ENCODERMODE_TI12; // A, B상 모두 사용
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
  // [DC 모터 PWM 설정]
  // Prescaler 1399 -> 주파수를 60Hz 근처로 낮춰서 모터 토크(힘)를 확보함.
  // Period 999 -> PWM 해상도를 1000단계(0~999)로 설정.
  TIM_OC_InitTypeDef sConfigOC = {0};
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 1399; // 중요: 주파수 낮춤
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
  // [서보 모터 PWM 설정]
  // 서보 모터는 보통 50Hz(20ms) 주기를 사용해야 함.
  // Prescaler와 Period 조합으로 50Hz를 만듦.
  TIM_OC_InitTypeDef sConfigOC = {0};
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 83;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 19999; // 20000 카운트 = 20ms (1카운트 = 1us)
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  HAL_TIM_PWM_Init(&htim3);
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = SERVO_CENTER_US; // 초기 위치: 중앙 (1500)
  HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_3);
}

/* ★ 핀 강제 설정 유지 ★ */
// [RTOS 이식 포인트] UART 설정 부분에서 나중에 'DMA' 설정을 추가해야 합니다.
static void MX_USART2_UART_Init(void)
{
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200; // 통신 속도
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;

  /* 핀 맵핑 (Alternate Function) */
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_USART2_CLK_ENABLE();
  GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_3; // PA2(TX), PA3(RX)
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF7_USART2; // AF7번 기능 사용
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_GPIO_Init(void)
{
  // 일반 GPIO(방향 제어용) 설정
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  
  // 초기 상태: 모두 LOW (정지)
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1|GPIO_PIN_4|LD2_Pin, GPIO_PIN_RESET);
  
  // PA1, PA4 (모터 방향), LD2 (상태 LED) -> 출력 모드로 설정
  GPIO_InitStruct.Pin = GPIO_PIN_1|GPIO_PIN_4|LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

static void Force_Hardware_Config(void)
{
  // [Study Note] 자동 생성 코드를 믿지 못하거나, 특정 핀을 확실히 지정하고 싶을 때 사용.
  // RTOS 프로젝트에서도 이 함수는 그대로 복사해서 가져가면 안전합니다.
  
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* TIM2_CH1 -> PA0 (DC 모터 PWM) */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF1_TIM2;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
  
  /* TIM3_CH3 -> PB0 (서보 모터 PWM) */
  __HAL_RCC_GPIOB_CLK_ENABLE();
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF2_TIM3;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
  
  /* TIM1 Encoder -> PA8/PA9 (엔코더 입력) */
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