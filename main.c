/* 
 * File:   main.c
 * Author: Santiago
 *
 * Created on November 29, 2018, 4:57 PM
 * Updated on February 18, 2019, 12:13 PM
 * Updated on September 4, 2019, 1:50 PM
 */

#include <pconfig.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <usart.h>
#include <spi.h>
#include <timers.h>
#include "config.c" // Configuration bits

void selectMic(int);
void testUSART(void);
void testSPI(void);
uint8_t rx_command=0x70; // 's' = 0x73, 'p' = 0x70
uint8_t spi_in0 = 0x40; // MSB
uint8_t spi_in1 = 0x41; // LSB
uint8_t mask = 0x7F;
uint8_t n_mic=0;

void IOpins(void){
// Configuracion de los pines como I/O
    TRISAbits.TRISA0 = 0;   // Multiplexor A0 output
    TRISAbits.TRISA1 = 0;   // Multiplexor B0 output
    TRISAbits.TRISA2 = 0;   // Multiplexor A1 output
    TRISAbits.TRISA3 = 0;   // Multiplexor B1 output
    TRISAbits.TRISA4 = 0;   // NC
    TRISAbits.TRISA5 = 0;   // SPI CS - Marca inicio y final de TX
    TRISBbits.TRISB0 = 1;   // SPI Data Input
    TRISBbits.TRISB1 = 0;   // SPI SCLK Output pag 199
    TRISBbits.TRISB2 = 0;   // Output de prueba - salida alta
    TRISBbits.TRISB3 = 0;   // NC
    TRISBbits.TRISB4 = 0;   // NC
    TRISBbits.TRISB5 = 1;   // PGM
    TRISBbits.TRISB6 = 1;   // PGC
    TRISBbits.TRISB7 = 1;   // PGD
    TRISCbits.TRISC0 = 0;   // NC
    TRISCbits.TRISC1 = 0;   // NC
    TRISCbits.TRISC2 = 0;   // NC
    TRISCbits.TRISC6 = 0;   // UART Tx output
    TRISCbits.TRISC7 = 1;   // UART Rx input = SPI Data Output Disabled
    __delay_ms(1);
}

void configUSART(void){
    CloseUSART(); //turn off USART if was previously on    

    int spbrg;
	unsigned char USARTconfig;
    
    USARTconfig = USART_TX_INT_OFF &    // Interruption TX off
            USART_RX_INT_ON &          // Interruption RX off
            USART_BRGH_HIGH &           // High speed
            USART_CONT_RX &             // Continuous TX
            USART_EIGHT_BIT &           // 8-bit or 9-bit
            USART_ASYNCH_MODE &         // ASYNCH TX
            USART_ADDEN_OFF;            // related to NINE_BIT transmission
    
    spbrg = 3; // 3.000.000 baud rate - 0% error
    
    /*
    ---- SPBRG needs to be changed depending upon oscillator frequency----
	spbrg = FOSC/(7 * baud) - 1; // Proteus
    spbrg = _XTAL_FREQ/(4 * 3000000) - 1;
    */
        
    OpenUSART(USARTconfig, spbrg); //API configures USART for desired parameters
    //PEIE = 1;  // Peripherals Interrupt Enable Bit
    //GIE = 1;   // Global Interrupt Enable Bit
    //CREN = 1;  // Enable Data Reception
    PIE1bits.RCIE = 1;
    BRG16 = 1; // 16-bit baud rate - SPBRGH and SPBRG
    //WUE = 0; // RX pin not monitored or rising edge detected
    __delay_ms(1);
}

void configSPI(void){
    CloseSPI();
    //mode 00: CKP=0 CKE=0 -- idle state to active & idle state = low
    //(sync_mode,bus_mode,smp_phase)
    OpenSPI(SPI_FOSC_TMR2,MODE_01,SMPEND);
    __delay_ms(1);
}

void configTimer2(void){
    CloseTimer2();
    // Fosc/4 -> F/1:4:16 -> F/2 => Fosc/8:32:128 = TMR2_OUTPUT
    // TMR2 cuenta hasta PR2 y luego envia señal de clock
    // ==>  PR2 = 29  => 48.000.000/240 = 200.000 Hz - 4 muestras por microfono
    //      PR2 = 9   => 48.000.000/320 = 150.000 Hz - 3 muestras por microfono
    //      PR2 = 14  => 48.000.000/480 = 100.000 Hz - 2 muestras por microfono
    //      PR2 = 29  => 48.000.000/960 = 50.000  Hz - 1 muestra  por microfono
    
    PR2 = 1;
    //PR2 = 1; // SCLK = 3 MHz
    OpenTimer2(TIMER_INT_OFF & T2_PS_1_1 & T2_POST_1_1);
    __delay_ms(1);
}


void main(void)
{
    ADCON1 = 0x0F ; // Configure All Analog Channel to Digital
    TRISA = 1;
    TRISB = 1; // Configurar todos los pines como entrada para no mandar datos
    TRISC = 1; // al circuito que este conectado cuando se programe
    
           
    configUSART();  // Configuracion USART
    configTimer2(); // Configuracion de Timer 2
    configSPI();    // Configuracion SPI
    
    // Sobreescribo configuracion de pines segun lo que necesito
    IOpins();  // Configuracion pines entrada/salida
    
    uint8_t pack_num = 0;

    /*
     Send char to say you are ready to transmit
     while(BusyUSART()); 
     TXREG = 0x72; // 'r' = 0x72
     
     */
    while(1){
        LATCbits.LATC0 = 0;
        if (RCIF == 1)
        {
          rx_command = RCREG;  // Read The Received Data Buffer
          RCIF = 0;             // Clear The Flag
        }        
        //for (int i=0; i<=10; i++)  __delay_ms(10);
        if (rx_command == 0x73){ //wait 's' start
            n_mic = 0;
            selectMic(n_mic);
            __delay_ms(1);
            while(1){
                
                if(n_mic == 0) {
                    while(BusyUSART());
                    TXREG = 65;
                }
                LATCbits.LATC0 = !LATCbits.LATC0;
                // Start acquisition
                LATAbits.LATA5 = 1;             // CS Pulse High
                __delay_us(2);
                unsigned char dummy;
                dummy = SSPBUF;                 // Clear BF
                LATAbits.LATA5 = 0;             // CS Pulse Low                
                // First byte
                SSPBUF = 0x00;                  // initiate bus cycle
                while ( !SSPSTATbits.BF );      // wait until cycle complete
                spi_in0 = SSPBUF;               // Clear BF
                // Second byte
                SSPBUF = 0x00;                  // initiate bus cycle
                while ( !SSPSTATbits.BF );      // wait until cycle complete
                spi_in1 = SSPBUF;               // Clear BF
                // Finish acquisition
                //LATAbits.LATA5 = 1;             // CS Pulse High
                
                // Set first 2 bits to 0
                spi_in0 = spi_in0 & mask;
                // Select Mic Multiplexer before sending data to let
                // ADC input stabilize while it waits next sample
                n_mic = n_mic+1;
                selectMic(n_mic);
                
                
                // Transmit to PC
                // 4 us per byte approx
                while(BusyUSART());
                TXREG = spi_in0;
                while(BusyUSART());
                TXREG = spi_in1;
                
                
                LATCbits.LATC0 = !LATCbits.LATC0;
                if(n_mic==4) {
                    n_mic=0;
                    selectMic(n_mic);
//                    while(BusyUSART());
//                    TXREG = pack_num++;
                    while(BusyUSART());
                    TXREG = 90;
                    __delay_us(11);
                    //while(BusyUSART()); 
                    //TXREG = 0x7F;
                    //while(BusyUSART());
                    //TXREG = 0xFF;
                    //__delay_us(10); // final
                    //__delay_us(2); // 5 mics final 11kHz
                    //__delay_us(58); // 2 mics
                    //__delay_us(72); // 1 mic 11kHz
                    //__delay_us(30); // 1 mic 22kHz
                    if (RCIF == 1)
                    {
                      rx_command = RCREG;  // Read The Received Data Buffer
                      RCIF = 0;             // Clear The Flag
                    }
                    if(rx_command==0x70) break; // 0x70 = 'p' in ASCII
                }
            }
        }
    }
}

void selectMic(int n_mic){
    /*
     A0 & A1: Multiplexer 1
     A2 & A3: Multiplexer 2
     Table
     A0/A1/A2/A3 / MIC OUT
     0/ 0/ 0/ 1  /  0 (HEART)(DIRECT)
     0/ 1/ 0/ 1  /  1 (INVERTED)
     1/ 0/ 0/ 1  /  2 (INVERTED)
     X/ X/ 0/ 0  /  3 (INVERTED)
     X/ X/ 1/ 1  /  4 (DIRECT)
     */
    switch(n_mic){
        
                    
        case 1:     LATA = 0b00001010;
                    break;
          
        case 2:     LATA = 0b00001001;
                    break;
                    
        case 3:     LATA = 0b00000000;
                    break;
        
        case 4:     LATA = 0b00001100;
                    break;
                    
        case 5:     LATA = 0b00001111;
                    break;
                
        case 0:     LATA = 0b00001000;
                    break;
    }
}
 
//for (int i=0; i<=10; i++)  __delay_ms(10);
