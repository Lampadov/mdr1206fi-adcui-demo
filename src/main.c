/**
 * @file    main.c
 * @brief   Описание: MDR1206FI обрабатывает данные с MDR5103FI через блок ADCUI.
 *
 *          Подключение (модуль MDR5103FI -> плата MDR1206FI):
 *              XP1.4 (VDD)  -> +3,3 В
 *              XP1.2 (GND)  -> GND
 *              XP1.3 (CLK)  -> PD14 (CLK_SD_EXT)
 *              XP1.1 (DOUT) -> PD10 (SD_I0_EXT)
 *
 *          HSE = 8 МГц -> тактовая частота АЦП 1 МГц, OSR = 256,
 *          частота отсчётов 3906,25 Гц.
 *
 *          Что смотреть при отладке: массив Samples (знаковые коды
 *          24-разрядного АЦП) на 128 отсчётов.
 *          Для сигнала 50 Гц, 10 В п-п на рассматриваемой плате MDR5103FI
 *          с шунтом 10 мОм ожидаемый размах кода около +-262000.
 */

#include "MDR32VF0xI_config.h"
#include "MDR32VF0xI_rst_clk.h"
#include "MDR32VF0xI_port.h"
#include "MDR32VF0xI_adcui.h"

#define WINDOW_SIZE 128

//Массив отсчётов
volatile int32_t Samples[WINDOW_SIZE];

// Чтение одного отсчёта: ждём данные, расширяем 24 бита до 32 со знаком
static int32_t ReadSample(void)
{
    while (ADCUI_GetFlagStatus(ADCUI_F0_UNIT, ADCUI_Fx_IF_EMP) == SET) {
    }
    return ((int32_t)(ADCUI_GetResult(ADCUI_CH_F0_I0) << 8)) >> 8;
}

int main(void)
{
    PORT_InitTypeDef         PORT;
    ADCUI_InitTypeDef        ADCUI;
    ADCUI_FxUnit_InitTypeDef ADCUI_UNIT;
    uint32_t                 i;

    // Тактирование: HSE 8 МГц -> блок ADCUI без делителей.
    RST_CLK_HSE_Config(RST_CLK_HSE_ON);
    while (RST_CLK_HSE_GetStatus() != SUCCESS) {
    }
    RST_CLK_PCLKCmd(RST_CLK_PCLK_PORTD | RST_CLK_PCLK_ADCUI, ENABLE);
    RST_CLK_ADCUI_ClkDeInit();
    RST_CLK_HSE_C1_SetPrescaler(RST_CLK_HSE_PRESCALER_DIV_1);
    RST_CLK_ADCUI_ClkSelection(RST_CLK_ADCUI_CLK_SRC_HSE_C1_CLK);
    RST_CLK_ADCUI_SetPrescaler(RST_CLK_ADCUI_PRESCALER_DIV_1);
    RST_CLK_ADCUI_ClkCmd(ENABLE);

    // Порт D: включаем внешний модулятор альтернативной функцией выводов
    PORT_StructInit(&PORT);
    PORT.PORT_Mode     = PORT_MODE_DIGITAL;
    PORT.PORT_Function = PORT_FUNCTION_ALTERNATIVE;

    PORT.PORT_Pin       = PORT_PIN_14;                    // CLK_SD_EXT, выход
    PORT.PORT_Direction = PORT_DIRECTION_OUTPUT;
    PORT.PORT_Power     = PORT_POWER_INCREASED_UPTO_4mA;
    PORT_Init(MDR_PORTD, &PORT);

    PORT.PORT_Pin       = PORT_PIN_10;                    // SD_I0_EXT, вход
    PORT.PORT_Direction = PORT_DIRECTION_INPUT;
    PORT.PORT_Power     = PORT_POWER_NOMINAL_UPTO_2mA;
    PORT_Init(MDR_PORTD, &PORT);

    // ADCUI: 24 бита, 1 МГц, OSR 256
    ADCUI_DeInit();
    ADCUI_StructInit(&ADCUI);
    ADCUI.ADCUI_DataResol      = ADCUI_DATA_RESOL_24BIT;
    ADCUI.ADCUI_VRefConfig     = ADCUI_REFERENCE_VOLTAGE_INTERNAL;
    ADCUI.ADCUI_ClockFrequency = ADCUI_CLK_FREQUENCY_1MHz;
    ADCUI.ADCUI_Divider        = ADCUI_OVERSAMPLING_DIV_256;
    ADCUI_Init(&ADCUI);

    ADCUI_Fx_StructInit(&ADCUI_UNIT);
    ADCUI_UNIT.ADCUI_Fx_IntegratorEnable   = ADCUI_INT_DISABLE;        // используется шунт, интегратор не нужен
    ADCUI_UNIT.ADCUI_Fx_SourceIDAT         = ADCUI_Fx_IDAT_I;          // в FIFO отсчёты тока
    ADCUI_UNIT.ADCUI_Fx_ISelectDataFilter  = ADCUI_FxISEL_AFTER_HPF;   // для сохранения постоянной составляющей
    ADCUI_Fx_Init(ADCUI_F0_UNIT, &ADCUI_UNIT);

    ADCUI_ChopperConfig(ADCUI_CH_F0_I0, DISABLE);  // с внешним модулятором чоппер выключен

    ADCUI_ResetDigital(ENABLE);
    ADCUI_ResetDigital(DISABLE);
    ADCUI_ChannelConfig(ADCUI_CH_F0_I0, ENABLE);

    // Первые отсчёты - переходный процесс фильтра SINC3, поэтому отбрасываем
    for (i = 0; i < 64; i++) {
        (void)ReadSample();
    }

    // Заполняем массив подряд идущими отсчетами
    while(1) {
        for (i = 0; i < WINDOW_SIZE; i++) {
            Samples[i] = ReadSample();
        }
    }
}
