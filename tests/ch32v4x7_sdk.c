// SPDX-FileCopyrightText: Copyright (c) 2026 Adafruit Industries
// SPDX-License-Identifier: BSD-3-Clause
// Linux host regression tests: exercise the real drivers against mapped RAM.
// These check register selection and values, not peripheral electrical behavior.
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include "ch32v4x7.h"

extern ETH_DMADESCTypeDef *DMATxDescToSet;
extern ETH_DMADESCTypeDef *DMAPTPTxDescToSet;

static unsigned checks;
static unsigned failures;

#define CHECK(condition) do { \
    ++checks; \
    if (!(condition)) { \
        ++failures; \
        printf("FAIL line %d: %s\n\r", __LINE__, #condition); \
    } \
} while (0)

static void test_dma(void)
{
    DMA_Channel_TypeDef *channels[] = {
        DMA1_Channel1, DMA1_Channel2, DMA1_Channel3, DMA1_Channel4,
        DMA1_Channel5, DMA1_Channel6, DMA1_Channel7, DMA1_Channel8,
        DMA1_Channel9, DMA1_Channel10, DMA1_Channel11, DMA1_Channel12,
        DMA1_Channel13, DMA2_Channel1, DMA2_Channel2, DMA2_Channel3,
        DMA2_Channel4, DMA2_Channel5, DMA2_Channel6, DMA2_Channel7,
        DMA2_Channel8, DMA2_Channel9, DMA2_Channel10, DMA2_Channel11
    };
    DMA_TypeDef *banks[] = {DMA1, DMA1_EXTEN, DMA2, DMA2_EXTEN};
    const uint32_t flags[] = {
        DMA1_FLAG_TC1, DMA1_FLAG_TC2, DMA1_FLAG_TC3, DMA1_FLAG_TC4,
        DMA1_FLAG_TC5, DMA1_FLAG_TC6, DMA1_FLAG_TC7, DMA1_FLAG_TC8,
        DMA1_FLAG_TC9, DMA1_FLAG_TC10, DMA1_FLAG_TC11, DMA1_FLAG_TC12,
        DMA1_FLAG_TC13, DMA2_FLAG_TC1, DMA2_FLAG_TC2, DMA2_FLAG_TC3,
        DMA2_FLAG_TC4, DMA2_FLAG_TC5, DMA2_FLAG_TC6, DMA2_FLAG_TC7,
        DMA2_FLAG_TC8, DMA2_FLAG_TC9, DMA2_FLAG_TC10, DMA2_FLAG_TC11
    };
    const uint32_t interrupts[] = {
        DMA1_IT_TC1, DMA1_IT_TC2, DMA1_IT_TC3, DMA1_IT_TC4,
        DMA1_IT_TC5, DMA1_IT_TC6, DMA1_IT_TC7, DMA1_IT_TC8,
        DMA1_IT_TC9, DMA1_IT_TC10, DMA1_IT_TC11, DMA1_IT_TC12,
        DMA1_IT_TC13, DMA2_IT_TC1, DMA2_IT_TC2, DMA2_IT_TC3,
        DMA2_IT_TC4, DMA2_IT_TC5, DMA2_IT_TC6, DMA2_IT_TC7,
        DMA2_IT_TC8, DMA2_IT_TC9, DMA2_IT_TC10, DMA2_IT_TC11
    };
    const uint32_t raw_flags[] = {
        DMA_GIF8, DMA_GIF9, DMA_GIF10, DMA_GIF11, DMA_GIF12, DMA_GIF13
    };
    const uint32_t raw_clear[] = {
        DMA_CGIF8, DMA_CGIF9, DMA_CGIF10, DMA_CGIF11, DMA_CGIF12, DMA_CGIF13
    };

    // RM v1.2 sections 11.3.1/2 and 11.3.7/8: seven normal channels,
    // then extended channels 8-13 at four bits per channel, starting at bit 0.
    for (unsigned i = 0; i < 6; ++i) {
        CHECK(raw_flags[i] == (1U << (4 * i)));
        CHECK(raw_clear[i] == (1U << (4 * i)));
    }
    for (unsigned i = 0; i < 24; ++i) {
        unsigned channel = i < 13 ? i : i - 13;
        unsigned bank = (i < 13 ? 0 : 2) + (channel >= 7);
        unsigned shift = 4 * (channel < 7 ? channel : channel - 7);
        uint32_t complete = 2U << shift;
        for (unsigned b = 0; b < 4; ++b) {
            banks[b]->INTFCR = 0;
            banks[b]->INTFR = 0;
        }
        channels[i]->CFGR = 0x7FFF;
        channels[i]->CNTR = 123;
        channels[i]->PADDR = 0x20001000;
        channels[i]->MADDR = 0x20002000;
        DMA_DeInit(channels[i]);
        CHECK(channels[i]->CFGR == 0 && channels[i]->CNTR == 0);
        CHECK(channels[i]->PADDR == 0 && channels[i]->MADDR == 0);
        for (unsigned b = 0; b < 4; ++b) {
            CHECK(banks[b]->INTFCR == (b == bank ? 15U << shift : 0));
        }
        CHECK(DMA_GetFlagStatus(flags[i]) == RESET);
        CHECK(DMA_GetITStatus(interrupts[i]) == RESET);
        banks[bank]->INTFR = complete;
        CHECK(DMA_GetFlagStatus(flags[i]) == SET);
        CHECK(DMA_GetITStatus(interrupts[i]) == SET);
        banks[bank]->INTFCR = 0;
        DMA_ClearFlag(flags[i]);
        CHECK(banks[bank]->INTFCR == complete);
        banks[bank]->INTFCR = 0;
        DMA_ClearITPendingBit(interrupts[i]);
        CHECK(banks[bank]->INTFCR == complete);
    }
}

static void test_spi_usart(void)
{
    SPI_InitTypeDef spi;
    memset(&spi, 0xA5, sizeof(spi));
    SPI_StructInit(&spi);
    CHECK(spi.SPI_NSS == SPI_NSS_Hard);
    SPI1->CTLR1 = 0;
    SPI_Init(SPI1, &spi);
    CHECK(SPI1->CTLR1 == 0);

    USART_InitTypeDef uart;
    USART_StructInit(&uart);
    const uint16_t parity[] = {USART_Parity_Mark, USART_Parity_No,
        USART_Parity_Space, USART_Parity_Even, USART_Parity_Mark, USART_Parity_Odd};
    for (unsigned i = 0; i < sizeof(parity) / sizeof(parity[0]); ++i) {
        uart.USART_Parity = parity[i];
        USART_InitTypeDef before = uart;
        USART1->CTLR4 |= 0x100;
        USART_Init(USART1, &uart);
        CHECK(memcmp(&before, &uart, sizeof(uart)) == 0);
        CHECK((USART1->CTLR4 & 0x100) != 0);
        uint16_t expected = 0;
        if (parity[i] == USART_Parity_Mark) expected = 8;
        if (parity[i] == USART_Parity_Space) expected = 12;
        CHECK((USART1->CTLR4 & USART_CTLR4_CHECK_SEL) == expected);
        expected = 0;
        if (parity[i] == USART_Parity_Even || parity[i] == USART_Parity_Odd)
            expected = parity[i];
        CHECK((USART1->CTLR1 & 0x600) == expected);
    }
    USART_MARKorSPACE_Config(USART1, USART_VerifyMode_SPACE);
    CHECK((USART1->CTLR4 & USART_CTLR4_CHECK_SEL) == 12);
}

static void test_flags(void)
{
    DVP->STATUS = 0;
    CHECK(DVP_GetFlagStatus(DVP_FLAG_FIFO_RDY) == RESET);
    DVP->STATUS = 1;
    CHECK(DVP_GetFlagStatus(DVP_FLAG_FIFO_RDY) == SET);
    DVP->STATUS = 2;
    CHECK(DVP_GetFlagStatus(DVP_FLAG_FIFO_RDY) == RESET);
    CHECK(DVP_GetFlagStatus(DVP_FLAG_FIFO_FULL) == SET);

    for (unsigned event = 0; event < 2; ++event) {
        for (unsigned enabled = 0; enabled < 2; ++enabled) {
            I3C->EVR = event ? I3C_IT_FCIE : 0;
            I3C->INTENR = I3C_IT_ERRIE | (enabled ? I3C_IT_FCIE : 0);
            CHECK(I3C_GetITStatus(I3C_IT_FCIE) == (event && enabled ? SET : RESET));
        }
    }
    EXTI->INTENR = EXTI->EVENR = EXTI->RTENR = EXTI->FTENR = 0x003FFFFF;
    EXTI_DeInit();
    CHECK(EXTI->INTENR == 0 && EXTI->EVENR == 0);
    CHECK(EXTI->RTENR == 0 && EXTI->FTENR == 0);
    // RAM records the write-one-to-clear mask; it does not emulate W1C hardware.
    CHECK(EXTI->INTFR == 0x003FFFFF);
}

static void test_psram_clock(void)
{
    PSRAMTimingInitTypeDef timing = {20, 12, 3};
    PSRAMInitTypeDef psram = {PSRAM_CFIFO_WRADD, PSRAM_CAPCFG_32M, ENABLE, &timing};
    PSRAMInit(&psram);
    CHECK(PSRAM->CTLR == (PSRAM_CFIFO_WRADD | PSRAM_CAPCFG_32M | 0x10000));
    CHECK(PSRAM->TIMING == (20U | (12U << 16) | (3U << 24)));
    psram.PSRAM_exti_lpmd = DISABLE;
    PSRAMInit(&psram);
    CHECK(PSRAM->CTLR == (PSRAM_CFIFO_WRADD | PSRAM_CAPCFG_32M));
    PSRAM_Exti_LP(ENABLE);
    CHECK((PSRAM->CTLR & 0x10000) != 0);
    PSRAM_Exti_LP(DISABLE);
    CHECK((PSRAM->CTLR & 0x10000) == 0);

    const uint32_t divisors[] = {RCC_CLKDIV11, RCC_CLKDIV12, RCC_CLKDIV13,
        RCC_CLKDIV14, RCC_CLKDIV15, RCC_CLKDIV16, RCC_CLKDIV17, RCC_CLKDIV18};
    RCC_ADCCLKSourceConfig(RCC_ADCCLKSource_USBHSPLL);
    RCC_ADCCLKConfig(RCC_PCLK2_Div2);
    for (unsigned i = 0; i < sizeof(divisors) / sizeof(divisors[0]); ++i) {
        RCC_ClocksTypeDef clocks;
        RCC_ADCClockSourceUSBHSConfig(divisors[i]);
        RCC_GetClocksFreq(&clocks);
        CHECK(clocks.ADCCLK_Frequency == 480000000U / (i + 11) / 2);
    }
    RCC->CFGR2 = 0x12345;
    RCC_UTMI2cmd(ENABLE);
    CHECK(RCC->CFGR2 == 0x80012345);
    RCC_UTMI2cmd(DISABLE);
    CHECK(RCC->CFGR2 == 0x12345);
    RCC->RSTSCKR = RCC_LSIRDY;
    IWDG_Enable();
    CHECK(IWDG->CTLR == 0xCCCC);

    // HSE never becomes ready in mapped RAM: exercise the real timeout path.
    memset((void *)RCC, 0, sizeof(*RCC));
    SystemCoreClock = 200000000;
    SystemInit();
    CHECK(SystemClock == 20000000);
    CHECK(SystemCoreClock == 20000000);
}

static void test_ethernet(void)
{
    ETH->DMASR = ETH_DMA_TransmitProcess_Suspended |
        ETH_DMA_ReceiveProcess_Suspended | ETH_DMASR_TS | ETH_DMASR_RS;
    CHECK(ETH_GetTransmitProcessState() == ETH_DMA_TransmitProcess_Suspended);
    CHECK(ETH_GetReceiveProcessState() == ETH_DMA_ReceiveProcess_Suspended);
    ETH->DMASR = 0;
    CHECK(ETH_GetTransmitProcessState() == 0 && ETH_GetReceiveProcessState() == 0);
    for (unsigned status = 0; status < 2; ++status) {
        for (unsigned masked = 0; masked < 2; ++masked) {
            ETH->MMCRIR = status ? 0x20 : 0;
            ETH->MMCRIMR = masked ? 0x20 : 0;
            ETH->MMCTIR = status ? 0x00200000 : 0;
            ETH->MMCTIMR = masked ? 0x00200000 : 0;
            CHECK(ETH_GetMMCITStatus(ETH_MMC_IT_RFCE) ==
                  (status && !masked ? SET : RESET));
            CHECK(ETH_GetMMCITStatus(ETH_MMC_IT_TGF) ==
                  (status && !masked ? SET : RESET));
        }
    }
    ETH_DMADESCTypeDef *desc = (ETH_DMADESCTypeDef *)0x20001000;
    uint32_t timestamp[] = {0x12345678, 0x87654321};
    uint8_t packet = 0x5A;
    memset(desc, 0, sizeof(*desc));
    desc->Buffer1Addr = 0x20002000;
    DMATxDescToSet = DMAPTPTxDescToSet = desc;
    CHECK(ETH_HandlePTPTxPkt(&packet, 1, timestamp) == ETH_ERROR);
    CHECK(timestamp[0] == 0x12345678 && timestamp[1] == 0x87654321);
    CHECK(DMATxDescToSet == desc);
    CHECK(*(uint8_t *)0x20002000 == packet);

    // A ready timestamp follows the existing success path.
    DMATxDescToSet = DMAPTPTxDescToSet = desc;
    desc->Status = ETH_DMATxDesc_TTSS;
    desc->Buffer2NextDescAddr = 0x1234;
    CHECK(ETH_HandlePTPTxPkt(&packet, 1, timestamp) == ETH_SUCCESS);
    CHECK(timestamp[0] == 0x20002000 && timestamp[1] == 0x1234);
    CHECK((desc->Status & ETH_DMATxDesc_TTSS) == 0);
}

int main(void)
{
    const uintptr_t addresses[] = {0x40000000, 0x50050000, 0x20000000, 0x1FF00000};
    for (unsigned i = 0; i < sizeof(addresses) / sizeof(addresses[0]); ++i) {
        void *address = (void *)addresses[i];
        if (mmap(address, 0x100000, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0) != address) {
            perror("Map emulated peripheral registers");
            return 2;
        }
    }
    test_dma();
    test_spi_usart();
    test_flags();
    test_psram_clock();
    test_ethernet();
    printf("CH32V4x7 SDK: %u checks, %u failures\n\r", checks, failures);
    return failures != 0;
}
