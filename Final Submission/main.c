//
//                MSP430F552x
//             -----------------
//         /|\|                 |
//          | |                 |
//          --|RST              |
//            |                 |
//     Vin -->|P6.0/CB0/A0  P2.0|--> PWM
//
//   Bhargavi Nisarga
//   Texas Instruments Inc.
//   April 2009
//   Built with CCSv4 and IAR Embedded Workbench Version: 4.21
//******************************************************************************

//Milestone 2
//Alex Marino and Cameron Bendzynski

#include <msp430.h>
#include <math.h>
volatile float Res; //resistance of thermistor
volatile double R; //ADC input
volatile float temp; //current temperature calculated by the equations
volatile float lastTemp; //previous temperature
volatile float tempInt; //current temperature stored as an integer for UART transmission
volatile int targetTemp; //target temperature as defined via UART
volatile float deltaTemp; //difference from target temperature to current temperature reading
volatile float timeTemp; //difference from current temperature to last temperature


int main(void)
{
  lastTemp = 0;                               //initialize previous temperature to 0 degrees
  WDTCTL = WDTPW + WDTHOLD;                 // Stop WDT
  ADC12CTL0 = ADC12SHT02 + ADC12ON;         // Sampling time, ADC12 on
  ADC12CTL1 = ADC12SHP;                     // Use sampling timer
  ADC12IE = 0x01;                           // Enable interrupt
  ADC12CTL0 |= ADC12ENC;                       //enable conversion
  P6SEL |= 0x01;                            // P6.0 ADC option select

  //PWM
  P2DIR |= BIT0;                                //set out
  P2SEL |= BIT0;                               //Set pin 2.0 as PWM OUT

  P4SEL |= BIT4 + BIT5;                       // P3.3,4 = USCI_A0 TXD/RXD

  UCA1CTL1 |= UCSWRST;                        // **Put state machine in reset**
  UCA1CTL1 |= UCSSEL_2;                       // SMCLK
  UCA1BR0 = 6;                                // 1MHz 9600 (see User's Guide)
  UCA1BR1 = 0;                                // 1MHz 9600
  UCA1MCTL |= UCBRS_0 + UCBRF_13 + UCOS16;    // Modulation UCBRSx=1, UCBRFx=0
  UCA1CTL1 &= ~UCSWRST;                       // **Initialize USCI state machine**
  UCA1IE |= UCRXIE;                           // Enable USCI_A0 RX interrupt

  ADC12CTL0 |= ADC12SC;                     // Start sampling/conversion

  //sampling timer - ADC
  TA0CTL = TASSEL_2 + MC_1 + ID_2;            //SMCLK, UP mode, divide by 4
  TA0CCTL0 = CCIE;                          //enable capture compare interrupt
  TA0CCR0 = 10000;                          //set capture compare value

  //PWM
  TA1CCR0 = 1000;                           //total period
  TA1CCTL1 = OUTMOD_7;                      //PWM mode reset/set
  TA1CCR1 = 100;                        //initial ON period
  TA1CTL = TASSEL_2 + MC_1 + ID_2;      //SMCLK, UP mode, divide by 4


  __bis_SR_register(GIE);     //enable global interrupt
  while(1) //infinite while loop
  {
 //liner thermistor equations
  Res = 10000*((4095/R)-1); //find resistance of thermistor
  deltaTemp = (temp - targetTemp); //find change in temperature (current reading - target temperature)
  if ((Res >= 0) && (Res <= 1918 )) //100 to 70 degrees
  {
      //linear equation 1
      temp =-0.0275*Res + 116.24;
  }
  else if ((Res > 1918) && (Res <= 5932))//70 to 40 degrees
  {
      temp = -0.0076*Res + 78.917; //linear equation 2
  }
  else if (Res > 5932)//40 to 0 degrees
  {
      temp = -0.0022*Res + 47.71; //linear equation 3
  }
  else
  {
      temp =-0.0275*Res + 116.24;  //default to equation 3
  }
  tempInt = (int)temp; //store temperature as an integer
  timeTemp = 9*(temp - lastTemp);//weight the time-temp calculation higher by multiplying by a constant
  lastTemp = temp; //store the current temperature as last temp
  }
}

// A = 3.354x10-3
// B = 2.569x10-4
// C = 2.62x10-6
// D = 6.383x10-8

#pragma vector = ADC12_VECTOR //ADC interrupt
__interrupt void ADC12_ISR(void)
{
  switch(__even_in_range(ADC12IV,34))
  {
  case  0: break;                           // Vector  0:  No interrupt
  case  2: break;                           // Vector  2:  ADC overflow
  case  4: break;                           // Vector  4:  ADC timing overflow
  case  6:                                  // Vector  6:  ADC12IFG0
    R = ADC12MEM0; //R value from the thermistor
    ADC12CTL0 |= ADC12SC;                   // Start sampling/conversion
    break;
  default: break;
  }
}

#pragma vector=USCI_A1_VECTOR                   // Detects interrupt for UART
__interrupt void USCI_A1_ISR(void)
{
    switch (__even_in_range(UCA1IV, 4))
    {
    case 0:                                     // Vector 0 - no interrupt
        break;
    case 2:                                     // Vector 2 - RXIFG
        while (!(UCA1IFG & UCTXIFG));           // USCI_A1 TX buffer ready?
        targetTemp = UCA1RXBUF;                 //receives target temperature over UART
        UCA1TXBUF = tempInt;                    //sends current temperature over UART
        break;
    default:
        break;
    }
}


#pragma vector=TIMER0_A0_VECTOR //timer interrupt vector
__interrupt void TIMER_A0_INT(void)
{
    //make sure TA1CCR1 is within the correct range, or will be moved into the correct range, before modifying it
    if((TA1CCR1 < 950 & TA1CCR1 > 50) | ((TA1CCR1 <= 50) & (deltaTemp + timeTemp > 0)) | ((TA1CCR1 >= 950) & (deltaTemp + timeTemp < 0)))
        TA1CCR1 = TA1CCR1 + 0.5*(deltaTemp + timeTemp); //change PWM value based on temperature calculations
    else
        TA1CCR1 = TA1CCR1; //hold PWM
}


