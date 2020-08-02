#include <LPC214x.H>  
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#define PLOCK 0x00000400 
#define LED_OFF (IO0SET = 1U << 31) 
#define LED_ON  (IO0CLR = 1U << 31) 
#define COL0 (IO1PIN & 1 <<19) 
#define COL1 (IO1PIN & 1 <<18) 
#define COL2 (IO1PIN & 1 <<17) 
#define COL3 (IO1PIN & 1 <<16)

//function_list

 //interrupts
void init_sensor_keyboard_interrupts(void);
__irq void Keyboard_ISR(void);
__irq void Sensor_ISR(void);
 
void delay(int count);
void callDoc_msgDoc();
void keyboard_check();
void GSM_init();
void UART0_init(void);
void UART0_TxChar(char ch);
void UART0_SendString(char* str);
void GSM_ReceiveMsg(void);
void GSM_Response(void);
void GSM_Calling(char *Mob_no);
void GSM_HangCall(void);
void GSM_Response_Display(void);
void callFire_msgFire();
void ring_bell(int dutycycle);
	
//Global_variables
char buff[160];		/* buffer to store responses and messages */
bool status_flag = false;	/* for checking any new message */
volatile int buffer_pointer;
char Mobile_no[14];		/* store mobile no. of received message */
char message_received[60];		/* save received message */
int position = 0;	/* save location of current message */
char lookup_table[4][4][10]={  {{"9029292201\0"}, {"9029292102\0"}, {"9023892201\0"},{"9039292201\0"}}, 
                                     {{"9029292201\0"}, {"9029292201"}, {"9029292251"},{"9029292201"}},  
                                     {{"9029292101"}, {"9029292201"}, {"9029293201"},{"9029292201"}},   
                                     {{"9029292401"}, {"9339292201"}, {"9999592201"},{"9929292201"}}}; 


int main (void) 
{
  init_sensor_keyboard_interrupts() ;  	// initialize the external interrupts
	GSM_init();
	PINSEL2 |= 1 << 24;  // P0.28 AS AD0.1(01) for sensor
	AD0CR= (1 << 1 | 1 << 21 | 1 << 24) ; //Sensor_init
	PINSEL0 |= 2 << 14;   //select P0.7 as PWM2 (option 2) 
  while (1)  
  {        
    //AD0GDR|=(1<<31)|(0x1180); //0001 0001 1000 0000 = 70
		while ( (AD0GDR & (unsigned long) 1 << 31) == 0){}; 
    int i = (AD0GDR >> 6 ) & 0xFF ; 
    if (i > 60){
		  int duty=i/10;
      ring_bell(duty);
		}
		IO0CLR|=0x8000;	
  }
}

__irq void Keyboard_ISR(void) // Interrupt Service Routine-ISR 
{
 //keyboard
	keyboard_check();
  callDoc_msgDoc(Mobile_no);	
  EXTINT |= 0x4;          //clear interrupt
  VICVectAddr = 0;      // End of interrupt execution
}

__irq void Sensor_ISR(void) // Interrupt Service Routine-ISR 
{
 
 callFire_msgFire();
 ring_bell(100);
	
 EXTINT |= 0x2;          //clear interrupt
 VICVectAddr = 0;      // End of interrupt execution
}

void init_sensor_keyboard_interrupts()  //Initialize Interrupt
{
  EXTMODE = 0x6;           //Edge sensitive mode on EINT1 and EINT2
  
  EXTPOLAR = 0;        //Falling Edge Sensitive
  PINSEL0 = 0xA0000000;    //Select Pin function P0.14 and P0.15 as EINT1 and EINT2
  
  /* initialize the interrupt vector */
  VICIntSelect &= ~ (0x3<<15);            // EINT1 selected as IRQ 15&16
  VICVectAddr0 = (unsigned int)Sensor_ISR; // address of the ISR
  VICVectCntl0 = (1<<5) | 15;            // 
  VICVectAddr5 = (unsigned int)Keyboard_ISR; // address of the ISR
  VICVectCntl5 = (1<<5) | 16;            // 
  VICIntEnable = (0x3<<15);               // EINT1&2 interrupt enabled
  EXTINT = 0x6;    
}

void GSM_init(){
	
	buffer_pointer = 0;
	bool is_msg_arrived;
	memset(message_received, 0, 60);
	UART0_init();
	UART0_SendString("GSM Initializing...");
	delay(300);
	while(1)
	{
		UART0_SendString("ATE0\r\n");		/* send ATE0 to check module is ready or not */
		delay(5);
		GSM_ReceiveMsg();
		strcpy(buff,"OK\r\n");
		if(strstr(buff,"OK"))
		{
			GSM_Response();		/* get Response */
			memset(buff,0,160);
			break;
		}
		else
		{
			//UART0_SendString("Error");
		}
	}
	delay(1000);

	//UART0_SendString("Text Mode");
	UART0_SendString("AT+CMGF=1\r\n");	/* select message format as text */
	GSM_ReceiveMsg();
	strcpy(buff,"OK\r\n");
	GSM_Response();
	delay(10);	
}


void UART0_init(void)
{
	PINSEL0 = PINSEL0 | 0x00000005;	/* Enable UART0 Rx0 and Tx0 pins of UART0 */
	U0LCR = 0x83;	/* DLAB = 1, 1 stop bit, 8-bit character length */
	U0DLM = 0x00;	/* For baud rate of 9600 with Pclk = 15MHz */
	U0DLL = 0x61;	/* We get these values of U0DLL and U0DLM from formula */
	U0LCR = 0x03; /* DLAB = 0 */
	U0IER = 0x00000001; /* Enable RDA interrupts */
	IO0DIR|=0xFF<<16;
}

void UART0_TxChar(char ch) /* A function to send a byte on UART0 */
{
	U0IER = 0x00000000; /* Disable RDA interrupts */
	U0THR = ch;
	while( (U0LSR & 0x40) == 0 );	/* Wait till THRE bit becomes 1 which tells that transmission is completed */
	U0IER = 0x00000001; /* Enable RDA interrupts */
}

void UART0_SendString(char* str) /* A function to send string on UART0 */
{
	U0IER = 0x00000000; /* Disable RDA interrupts */
	uint8_t i = 0;
	while( str[i] != '\0' )
	{
		UART0_TxChar(str[i]);
		i++;
	}
	U0IER = 0x00000001; /* Enable RDA interrupts */
}

void GSM_ReceiveMsg(void){
	int i=0;
	for(i=0; i<2; i++){
	while((U0LSR & (0x01<<0))==0x00){};
					IO0CLR |= 0xFF << 16;
					IO0SET |= U0RBR << 16;
	}
}

void GSM_Response(void)
{
	unsigned int timeout=0;
	int CRLF_Found=0;
	char CRLF_buff[2];
	int Response_Length=0;
	while(1)
	{
		if(timeout>=60000)		/*if timeout occur then return */
		return;
		Response_Length = strlen(buff);
		if(Response_Length)
		{
			delay(1);
			timeout++;
			if(Response_Length==strlen(buff))
			{
				for(int i=0;i<Response_Length;i++)
				{
					memmove(CRLF_buff,CRLF_buff+1,1);
					CRLF_buff[1]=buff[i];
					if(strncmp(CRLF_buff,"\r\n",2))
					{
						if(CRLF_Found++==2)		/* search for \r\n in string */
						{
							GSM_Response_Display();		/* display response */
							return;
						}
					}

				}
				CRLF_Found = 0;

			}
			
		}
		delay(1);
		timeout++;
	}
	//status_flag = false;
}

void GSM_Response_Display(void)
{
	buffer_pointer = 0;
	while(1)
	{
		if(buff[buffer_pointer]== '\r' || buff[buffer_pointer]== '\n')		/* search for \r\n in string */
		{
			buffer_pointer++;
		}
		else
			break;
	}
	

	while(buff[buffer_pointer]!='\r')		/* display response till "\r" */
	{
		UART0_TxChar(buff[buffer_pointer]);								
		buffer_pointer++;
	}
	buffer_pointer=0;
	memset(buff,0,strlen(buff));
}

void GSM_Calling(char *Mob_no)
{
	char call[20];
	sprintf(call,"ATD%s;\r\n",Mob_no);		
	UART0_SendString(call);		/* send command ATD<Mobile_No>; for calling*/
}

void GSM_HangCall(void)
{
	UART0_SendString("ATH\r\n");		/*send command ATH\r to hang call*/
	
}

void GSM_Send_Msg(char *num,char *sms)
{
	char sms_buffer[35];
	buffer_pointer=0;
	sprintf(sms_buffer,"AT+CMGS=\"%s\"\r\n",num);
	UART0_SendString(sms_buffer);		/*send command AT+CMGS="Mobile No."\r */
	delay(200);
	while(1)
	{
		if(buff[buffer_pointer]==0x3e)		/* wait for '>' character*/
		{
			buffer_pointer = 0;
			memset(buff,0,strlen(buff));
			UART0_SendString(sms);		/* send msg to given no. */
			UART0_TxChar(0x1a);				/* send Ctrl+Z then only message will transmit*/
			break;
		}
		buffer_pointer++;
		buff[buffer_pointer]='>';
	}
	delay(300);
	buffer_pointer = 0;
	memset(buff,0,strlen(buff));
	memset(sms_buffer,0,strlen(sms_buffer));
}

void keyboard_check(){
	
	int rowsel,colsel;
	IO0DIR |= 1U << 31 | 0x00FF0000; // to set P0.16 to P0.23 as o/ps   
	
 while(1) 
   { 
//check for keypress in row0,make row0 '0',row1=row2=row3='1' 
	rowsel=0;IO0SET |= 0X000F0000;IO0CLR |= 1 << 16; 
	if(COL0==0){colsel=0;break;};if(COL1==0){colsel=1;break;}; 
	if(COL2==0){colsel=2;break;};if(COL3==0){colsel=3;break;}; 
//check for keypress in row1,make row1 '0' 
	rowsel=1;IO0SET |= 0X000F0000;IO0CLR |= 1 << 17; 
	if(COL0==0){colsel=0;break;};if(COL1==0){colsel=1;break;}; 
	if(COL2==0){colsel=2;break;};if(COL3==0){colsel=3;break;}; 
//check for keypress in row2,make row2 '0' 
	rowsel=2;IO0SET |= 0X000F0000;IO0CLR |= 1 << 18;//make row2 '0' 
	if(COL0==0){colsel=0;break;};if(COL1==0){colsel=1;break;}; 
	if(COL2==0){colsel=2;break;};if(COL3==0){colsel=3;break;}; 
//check for keypress in row3,make row3 '0' 
	rowsel=3;IO0SET |= 0X000F0000;IO0CLR |= 1 << 19;//make row3 '0' 
	if(COL0==0){colsel=0;break;};if(COL1==0){colsel=1;break;}; 
	if(COL2==0){colsel=2;break;};if(COL3==0){colsel=3;break;}; 
 } 
	delay(50);  //allow for key debouncing 
	while(COL0==0 || COL1==0 || COL2==0 || COL3==0);//wait for key release 
	delay(50);   //allow for key debouncing 
	IO0SET |= 0X000F0000; //disable all the rows 
	memset(Mobile_no,0,strlen(Mobile_no));
	strcpy(Mobile_no,lookup_table[rowsel][colsel]);
}

void delay(int count)
{
  int j=0,i=0;
  for(j=0;j<count;j++)
  {
    /* At 60Mhz, the below loop introduces
    delay of 10 us */
    for(i=0;i<35;i++);
  }
}

void callDoc_msgDoc(){
	GSM_Calling(Mobile_no);										/* call sender of message */
	UART0_SendString("Calling...");
	UART0_SendString(Mobile_no);
	delay(1500);
	GSM_HangCall();											/* hang call */
	//UART0_SendString("Hang Call");
	delay(1500);
	GSM_Send_Msg("Come To ICU, Immediately",Mobile_no); //send message
  delay(100);
}

void callFire_msgFire(){
	GSM_Calling("101");										/* call sender of message */
	UART0_SendString("Calling Fire Station ");
	UART0_SendString("101");
	delay(1500);
	GSM_HangCall();											/* hang call */
	//UART0_SendString("Hang Call");
	delay(1500);
	GSM_Send_Msg("Fire Alert at XYZ Hospital, Start Immediatly ","101"); //send message
  delay(100);
}

void ring_bell(int dutycycle){
	 PWMPCR = (1 << 10);  // enable PWM2 channel 
   PWMMR0 = 1000;   // set PULSE rate to value suitable for Bell operation 
   PWMMR2 = (1000U*dutycycle)/100;  // set PULSE period 
   PWMTCR = 0x00000009;  // bit D3 = 1 (enable  PWM), bit D0=1 (start the timer) 
	 PWMLER = 0X04;  // load the new values to PWMMR0 and PWMMR2 registers
    
}