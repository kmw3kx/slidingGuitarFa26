#include "daisy_seed.h"

using namespace daisy;

DaisySeed hw;
// for serial read:
/** Create a FIFO for receiving messages to echo out. */
FIFO<FixedCapStr<128>, 16> msg_fifo;

/** Callback that fires whenever new data is sent from the serial port */
void UsbCallback(uint8_t *buff, uint32_t *length)
{
    if(buff && length) /**< Check that our inputs are not null */
    {
        // Create a new string, and push it to the FIFO
        FixedCapStr<128> rx((const char *)buff, *length);
        msg_fifo.PushBack(rx);

        /** Something like this below _can_ work. (where outbuff is a global char array)
         *  However, it is ideal to leave this callback as quickly as possible.
         *  So it is recommended to handle any responses to the input elsewhere.
         */
        // strncpy(outbuff, (const char*)buff, *length);
        // hw.PrintLine(outbuff);
    }
}
// end serial read

#include "stm32h7xx_hal.h"

TIM_HandleTypeDef htim2;

// --- Physical Conversion Constants ---
// CQRobot 25D motor: 48 CPR on motor shaft * 4.4 gear ratio ≈ 211.2 counts/rev on output shaft
// 20T GT2 timing pulley (2 mm pitch) = 40 mm (0.040 m) linear travel per output revolution
const float METERS_PER_TICK = 0.040f / (48.0f * 4.4f); // ~0.0001894 m/tick (0.1894 mm/tick)

// =================================================================
// Hardware Timer 2 Initialization (Quadrature Decoder Mode)
// =================================================================
void Init_Hardware_Encoder_TIM2(void)
{
    // 1. Enable peripheral clocks for TIM2 and GPIOA
    __HAL_RCC_TIM2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    // 2. Configure Pin D24 (PA0) and Pin D25 (PA1) as Alternate Function TIM2
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin       = GPIO_PIN_0 | GPIO_PIN_1;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_PULLUP;             // Enable internal pull-ups
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF1_TIM2;           // Maps PA0/PA1 to TIM2
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // 3. Configure TIM2 in 4x Hardware Quadrature Decoder Mode
    TIM_Encoder_InitTypeDef sConfig = {0};
    htim2.Instance               = TIM2;
    htim2.Init.Prescaler         = 0;
    htim2.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim2.Init.Period            = 0xFFFFFFFF;          // Full 32-bit counter range
    htim2.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    sConfig.EncoderMode  = TIM_ENCODERMODE_TI12;        // 4x counting on both edges
    sConfig.IC1Polarity  = TIM_ICPOLARITY_RISING;
    sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
    sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
    sConfig.IC1Filter    = 4;                           // Hardware digital noise filter

    sConfig.IC2Polarity  = TIM_ICPOLARITY_RISING;
    sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
    sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
    sConfig.IC2Filter    = 4;

    HAL_TIM_Encoder_Init(&htim2, &sConfig);
    HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_ALL);
}

int_32_t GetEncoderTicks(void)
{
    return (int32_t)__HAL_TIM_GET_COUNTER(&htim2);
}

int_32_t current_ticks = 0;

int_32_t goal_ticks = 0;

int main(void){

    // Initialize the Daisy Seed
    hw.Init();
    // Start the log, and wait for connection
    hw.StartLog(true); // set to false if running without a USB connection (e.g., standalone)

    // Print "Hello World" to the Serial Monitor
    hw.PrintLine("Hello World!");

	// Set USB callback
    hw.usb_handle.SetReceiveCallback(UsbCallback,
                                     UsbHandle::UsbPeriph::FS_INTERNAL);

    /** Print an initial message once the connection occurs */
    hw.PrintLine("Send messages in your Serial Monitor!");

	// //Blinking
	// uint32_t blink_rate = 50; /**< Change me to test tweaking the program! */

	// bool blinkState = false;
	// /** Timing variables */
    // uint32_t now, blink_time;
    // now = blink_time = System::GetNow();
	// //end Blinking

    while(1) {
    // Read the 32-bit hardware counter directly from STM32 silicon registers
    int32_t current_ticks = (int32_t)__HAL_TIM_GET_COUNTER(&htim2);
	/* Whenever the FIFO has contents,
    go through and print each message that has been received */
        while(!msg_fifo.IsEmpty())
        {
            auto msg = msg_fifo.PopFront();
            hw.Print("Received Message: ");
            hw.PrintLine(msg.Cstr());
        }

        /** Blink LED to show program is running (this indicates to
         * the user that the Serial Monitor Connection has been established) */
        hw.SetLed((System::GetNow() & 1023) > 511);
	}
}
