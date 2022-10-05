/*
 * MAIN Generated Driver File
 * 
 * @file main.c
 * 
 * @defgroup main MAIN
 * 
 * @brief This is the generated driver implementation file for the MAIN driver.
 *
 * @version MAIN Driver Version 1.0.0
 */

/*
© [2022] Microchip Technology Inc. and its subsidiaries.

    Subject to your compliance with these terms, you may use Microchip 
    software and any derivatives exclusively with Microchip products. 
    You are responsible for complying with 3rd party license terms  
    applicable to your use of 3rd party software (including open source  
    software) that may accompany Microchip software. SOFTWARE IS ?AS IS.? 
    NO WARRANTIES, WHETHER EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS 
    SOFTWARE, INCLUDING ANY IMPLIED WARRANTIES OF NON-INFRINGEMENT,  
    MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT 
    WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE, 
    INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY 
    KIND WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF 
    MICROCHIP HAS BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE 
    FORESEEABLE. TO THE FULLEST EXTENT ALLOWED BY LAW, MICROCHIP?S 
    TOTAL LIABILITY ON ALL CLAIMS RELATED TO THE SOFTWARE WILL NOT 
    EXCEED AMOUNT OF FEES, IF ANY, YOU PAID DIRECTLY TO MICROCHIP FOR 
    THIS SOFTWARE.
 */
#include <avr/sleep.h>

#include "mcc_generated_files/system/system.h"
#include "mcc_generated_files/timer/delay.h"

/* Macro definitions */
#define ADC_MUX_VBAT_gc     ADC_MUXPOS_AIN8_gc
#define ADC_MUX_VLOAD_gc    ADC_MUXPOS_AIN6_gc
#define ADC_MUX_VDD_gc      ADC_MUXPOS_AIN7_gc
#define SAMPLES 256
#define REFERENCE 1.024

/* Offset calibration */
#define OFFSET 0

/* Resistor values for calculations */
#define R1 1.0
#define R2 1.5

/* Variable definitions */
volatile float vr1r2 = 0.0;
volatile float vr1 = 0.0;
volatile float vr2 = 0.0;

volatile float vdd = 0.0;
volatile float vbat = 0.0;

volatile float ibat = 0.0;
volatile float iload = 0.0;
volatile float idd = 0.0;

volatile bool btn_flag = false;

/* Function prototypes */
float adc_diff_16x(ADC_MUXPOS_t muxpos_config, ADC_MUXNEG_t muxneg_config);
float adc_single_2x(ADC_MUXPOS_t muxpos_config);
void usart_send_all(void);
void usart_send_float(volatile float *data);
void LEDs_change_state(void);

/*
    Main application
 */

int main(void) {
    SYSTEM_Initialize();
    
    while (1) {
        /* Measure voltage at relevant nodes */
        vr1r2 = adc_diff_16x(ADC_MUX_VBAT_gc, ADC_MUX_VDD_gc);
        vr1 = adc_diff_16x(ADC_MUX_VBAT_gc, ADC_MUX_VLOAD_gc);
        vr2 = adc_diff_16x(ADC_MUX_VLOAD_gc, ADC_MUX_VDD_gc);
        vdd = adc_single_2x(ADC_MUXPOS_VDDDIV10_gc);

        /* Calculate current and voltage in the circuit */
        ibat = vr1 / R1;
        idd = vr2 / R2;
        iload = (ibat - idd);
        vbat = vdd + vr1r2;

        /* Send result over USART */
        usart_send_all();

        /* If the PORTC interrupt has been triggered by PC4*/
        if (btn_flag == true) {
            /* Simple software debounce*/
            DELAY_milliseconds(10);
            if (!(SW0_GetValue())) {
                LEDs_change_state();
            }
            btn_flag = false;
        }

        /* Go to sleep */
        sleep_mode();
    }
}

void LEDs_change_state(void) {
    /* If LED1 is off */
    if (LED1_GetValue()) {
        /* Turn LED0 off and LED1 on */
        LED0_SetHigh();
        LED1_SetLow();

        /* If LED2 is off */
    } else if (LED2_GetValue()) {
        /* Turn LED2 on */
        LED2_SetLow();

        /* If LED3 is off */
    } else if (LED3_GetValue()) {
        /* Turn LED3 on */
        LED3_SetLow();

        /* If LEDs 1-3 are on */
    } else {
        /* Turn LED0 on and all other LEDs off */
        LED0_SetLow();
        LED1_SetHigh();
        LED2_SetHigh();
        LED2_SetHigh();
        LED3_SetHigh();
    }
}

void usart_send_all(void) {
    /* Data Visualizer packet start bit */
    while(!USART0_IsTxReady());
    USART0_Write(0x3c);

    /* Send data */
    usart_send_float(&vbat);
    usart_send_float(&ibat);
    usart_send_float(&vdd);
    usart_send_float(&idd);
    usart_send_float(&iload);
    usart_send_float(&vr1r2);
    usart_send_float(&vr1);
    usart_send_float(&vr2);

    /* Data Visualizer packet end bit */
    while(!USART0_IsTxReady());
    USART0_Write(~0x3c);

    /* Clear interrupt flag and wait for TX complete */
    USART0.STATUS = USART_TXCIF_bm;
    while (!(USART0.STATUS & USART_TXCIF_bm));
}

void usart_send_float(volatile float *data) {
    for (uint8_t i = 0; i < 4; i++) {
        while(!USART0_IsTxReady());
        USART0_Write(((uint8_t *) data)[i]);
    }
}

/* Differential measurement, 16x gain, burst accumulation mode */
float adc_diff_16x(ADC_MUXPOS_t muxpos_config, ADC_MUXNEG_t muxneg_config) {

    /* 16x gain */
    ADC0.PGACTRL = ADC_GAIN_16X_gc | ADC_PGAEN_bm;

    /* Set positive pin */
    ADC0.MUXPOS = ADC_VIA_PGA_gc | muxpos_config;

    /* Enable differential mode and set trigger to MUXNEG write */
    ADC0.COMMAND |= ADC_DIFF_bm | ADC_START_MUXNEG_WRITE_gc;

    /* Set negative pin. This write also starts the burst conversions. */
    ADC0.MUXNEG = ADC_VIA_PGA_gc | muxneg_config;

    /* Wait for result to be ready */
    while (!(ADC0.INTFLAGS & ADC_RESRDY_bm));

    /* Return averaged ADC result */
    /*Amplified and accumulated result divided by the gain value (16), resolution (+/- 2048) and number of samples to reflect measured voltage drop*/
    return (((int32_t) ADC0.RESULT) + OFFSET) * (((REFERENCE / 16.0) / 2048.0) / SAMPLES);
}

/* Single Ended measurement, 2x gain, burst accumulation mode */
float adc_single_2x(ADC_MUXPOS_t muxpos_config) {

    /* 2x gain */
    ADC0.PGACTRL = ADC_GAIN_2X_gc | ADC_PGAEN_bm;

    /* Set ADC channel */
    ADC0.MUXPOS = ADC_VIA_PGA_gc | muxpos_config;

    /* Enable single ended mode and set trigger to start immediate */
    ADC0.COMMAND = ADC_MODE_BURST_gc | ADC_START_IMMEDIATE_gc;

    /* Wait for result to be ready */
    while (!(ADC0.INTFLAGS & ADC_RESRDY_bm));

    /* Return averaged ADC result */
    /* Amplified and accumulated result divided by the gain value (2), resolution (4096) and number of samples to reflect measured voltage drop */
    /* Since the measured voltage is VDD/10, the result is also multiplied by 10 */
    return (((int32_t) ADC0.RESULT) + OFFSET) * 10 * (((REFERENCE / 2.0) / 4096.0) / SAMPLES);
}