/*
 Name:		ArduinoController.ino
 Created:	9/14/2020 11:10:14 AM
 Author:	Ideonics
*/

// the setup function runs once when you press reset or power the board

#include <TimerOne.h>
#include <Pushbutton.h>
#include <ArduinoModbus.h>
#include <ArduinoRS485.h>
#include "Process.h"

#define TIMER_TICK	10	// Timer tick
//DELAY
#define CLAMP_FAILURE_INTERVAL	(5000 / TIMER_TICK )
#define EXHAUST_INTERVAL		(2000  / TIMER_TICK )
#define FILL_INTERVAL			(500 / TIMER_TICK )
#define STABILIZATION_INTERVAL	(14000  / TIMER_TICK )
#define DECLAMP_INTERVAL		(3500  / TIMER_TICK )
#define TEST_INTERVAL			(3500  / TIMER_TICK )
#define COM_INTERVAL			(2000  / TIMER_TICK)
//PB Debounce Count
#define PB_DEBOUNCE			40

//COM
#define RS485_ENABLE		20


//Pressure Range
#define MIN_PRESSURE_1		270
#define MAX_PRESSURE_1		330
#define MIN_PRESSURE_1_5	270
#define MAX_PRESSURE_1_5	330
#define MIN_PRESSURE_3		270
#define MAX_PRESSURE_3		330

//Digital Inputs
#define EMER_PB		31			//Emergency Pushbutton
#define START_PB	25			//Start Pushbutton
#define REED_1		33			//Reed Switch 1
#define REED_2		27			//Reed Switch 2
#define Px_SW		35			//Pressure Switch
#define	SELECT_1_5	29			//Select 1.5 feet 
#define	SELECT_3	23			//Select 3 feet


//Digital Output
#define STN_1_INLET_SV			52			//Station 1 Inlet
#define STN_1_EXHAUST_SV		36			//Station 1 Exhaust
#define STN_2_INLET_SV			50			//Station 2 Inlet
#define STN_2_EXHAUST_SV		34			//Station 3 Exhaust
#define CLAMP_SV				48			//Clamp SV
#define DECLAMP_SV				32			//Declamp SV



#define STN_1_GRN_LMP	22			//Station 1 Green Lamp
#define STN_1_RED_LMP	38			//Station 1 Red Lamp
#define STN_2_GRN_LMP	24			//Station 2 Green Lamp
#define STN_2_RED_LMP	40			//Station 2 Red Lamp




Pushbutton Emer_PB(EMER_PB, PULL_UP_ENABLED, DEFAULT_STATE_LOW);
Pushbutton Start_PB(START_PB, PULL_UP_ENABLED, DEFAULT_STATE_HIGH);
Pushbutton Reed_1(REED_1, PULL_UP_ENABLED, DEFAULT_STATE_HIGH);
Pushbutton Reed_2(REED_2, PULL_UP_ENABLED, DEFAULT_STATE_HIGH);
Pushbutton Px_Sw(Px_SW, PULL_UP_ENABLED, DEFAULT_STATE_HIGH);
Pushbutton Select_1_5(SELECT_1_5, PULL_UP_ENABLED, DEFAULT_STATE_HIGH);
Pushbutton Select_3(SELECT_3, PULL_UP_ENABLED, DEFAULT_STATE_HIGH);

typedef enum  { IDLE = 0, START, CLAMP , FILL , STABILIZE, 
				READ_FILL, WAIT_FILL, 
				TEST, READ_TEST, WAIT_TEST, 
				EXHAUST, DECLAMP, 
				WAIT_RESULT, READ_RESULT,
				ERROR, EMERGENCY } STATE;

typedef enum { NO_PRESSURE = 1, CLAMP_FAILURE, FILL_FAILURE, TEST_FAILURE } FAILURE_REASON;

STATE ComState = IDLE;

STATE State = IDLE;
int TimerDelay = -1;	//Delay counter for timer operations ; -1 => infinite

unsigned int Fill_Pressure_1 = 0;
unsigned int Test_Pressure_1 = 0;
unsigned int PressureDifference_1 = 0;

unsigned int Fill_Pressure_2 = 0;
unsigned int Test_Pressure_2 = 0;
unsigned int PressureDifference_2 = 0;

const char ModBusFrame_1[] = { 0x01, 0x03, 0x00, 0x11, 0x00, 0x01, 0xD4, 0x0F };
const char ModBusFrame_2[] = { 0x02, 0x03, 0x00, 0x11, 0x00, 0x01, 0xD4, 0x3C };

unsigned char Rx_Buf[100], RxBufIndex = 0;

void DO_ON(unsigned pin)
{
	digitalWrite(pin, LOW);
		delay(5000);
		digitalWrite(pin, HIGH);

}

void setup() {

	Serial.begin(9600);
	
	
	pinMode(RS485_ENABLE, OUTPUT);
	digitalWrite(RS485_ENABLE, HIGH);

	pinMode(STN_1_INLET_SV, OUTPUT);
	pinMode(STN_1_EXHAUST_SV, OUTPUT);
	pinMode(STN_2_INLET_SV, OUTPUT);
	pinMode(STN_2_EXHAUST_SV, OUTPUT);
	pinMode(CLAMP_SV, OUTPUT);
	pinMode(DECLAMP_SV, OUTPUT);
	pinMode(STN_1_GRN_LMP, OUTPUT);
	pinMode(STN_1_RED_LMP, OUTPUT);
	pinMode(STN_2_GRN_LMP, OUTPUT);
	pinMode(STN_2_RED_LMP, OUTPUT);

	digitalWrite(STN_1_INLET_SV, HIGH);
	digitalWrite(STN_1_EXHAUST_SV, HIGH);
	digitalWrite(STN_2_INLET_SV, HIGH);
	digitalWrite(STN_2_EXHAUST_SV, HIGH);
	digitalWrite(CLAMP_SV, HIGH);
	digitalWrite(DECLAMP_SV, HIGH);
	digitalWrite(STN_1_GRN_LMP, HIGH);
	digitalWrite(STN_1_RED_LMP, HIGH);
	digitalWrite(STN_2_GRN_LMP, HIGH);
	digitalWrite(STN_2_RED_LMP, HIGH);

	

	
	Timer1.initialize(TIMER_TICK * 1000);
	Timer1.attachInterrupt(Process);
	

	Serial.println("AOSmith-HoseleakTestBench\n");
}


bool RDF, RDT, RDS;

// the loop function runs over and over again until power down or reset
void loop() {
	
	if (State != EMERGENCY && Emer_PB.isPressed())
	{
		digitalWrite(STN_1_EXHAUST_SV, LOW);		//switch on exhaust
		digitalWrite(STN_2_EXHAUST_SV, LOW);

		digitalWrite(CLAMP_SV, HIGH);		//switch off clamp ...just in case
		digitalWrite(STN_1_INLET_SV, HIGH);		//switch off inlet
		digitalWrite(STN_2_INLET_SV, HIGH);


		State = EMERGENCY;
		TimerDelay = 0;
		Serial.println("Emergency Pressed\n");

		delay(2000);
		digitalWrite(STN_1_EXHAUST_SV, HIGH);		//switch off exhaust
		digitalWrite(STN_2_EXHAUST_SV, HIGH);

		DO_ON(DECLAMP_SV);

		while (Emer_PB.isPressed());
		delay(1000);
		Reset();
		return;
	}
	switch (State)
	{
	case READ_FILL:
		if (RDF == 1)
		{
			Serial.println("FLRD");
			delay(5);
			RDF = 0;
		}
		
		break;
	case READ_TEST:
		if (RDT == 1)
		{
			Serial.println("TSRD");
			delay(5);
			RDT = 0;
		}
		
		break;

	case DECLAMP:
		if (RDS == 1)
		{
			Serial.println("RSRD");
			delay(5);
			RDS = 0;
		}
		break;
			
	}
	if (ComState == WAIT_FILL)
	{
		if (Serial.available() > 1)
		{
			String cmd = Serial.readString();

			if (cmd == "FLRD")
			{
				Serial.println("RDFL");
				
				ComState = IDLE;
				
			}
		}
	}

	

	else if (ComState == WAIT_TEST)
	{
		if (Serial.available() > 1)
		{
			String cmd = Serial.readString();

			if (cmd == "TSRD")
			{
				Serial.println("RDTS");
				ComState = IDLE;
			}
		}

	}

	
	else if (ComState == WAIT_RESULT)
	{
		if (Serial.available() > 1)
		{
			String cmd = Serial.readString();

			if (cmd == "Pass")
			{
				digitalWrite(STN_1_GRN_LMP, LOW);
				//digitalWrite(STN_2_GRN_LMP, LOW);
			}
			else if (cmd == "Fail")
			{
				digitalWrite(STN_1_RED_LMP, LOW);
				//digitalWrite(STN_2_RED_LMP, LOW);
			}
			else if (cmd == "PF")
			{
				digitalWrite(STN_1_GRN_LMP, LOW);
				digitalWrite(STN_2_RED_LMP, LOW);
			}
			else if (cmd == "FP")
			{
				digitalWrite(STN_1_RED_LMP, LOW);
				digitalWrite(STN_2_GRN_LMP, LOW);
			}
			ComState = IDLE;
		}

	}
	
}


void Process()
{
	switch(State)
	{
		case IDLE:
			
			if (Start_PB.isPressed())
			{
				Serial.println("Cycle Start\n");
				if (!Px_Sw.isPressed())  
				{
					Error(NO_PRESSURE);
					
					return;
				}
				if (Reed_1.isPressed() && !Reed_2.isPressed())	//if reed 1 is on and reed 2 is off
				{
					Serial.println("CLAMP");
					
					digitalWrite(CLAMP_SV, LOW);		//Clamp ON
					State = CLAMP;
					TimerDelay = 0;
					
				}
				else
				{
				
					Error(CLAMP_FAILURE);
					
				}

			}
			break;

		case CLAMP:
			if (TimerDelay < CLAMP_FAILURE_INTERVAL) { 
				TimerDelay++; 
				if (!Reed_1.isPressed() && Reed_2.isPressed())	//clamping completed
				{

					State = FILL;
					TimerDelay = 0;
					Serial.println("FILL");
				}
			}
			else { //Clamp Failure
				
				Error(CLAMP_FAILURE);
				
				
			}
		
			break;
		case FILL:
			
			if (TimerDelay < FILL_INTERVAL) { TimerDelay++; return; }
			
			digitalWrite(STN_1_INLET_SV, LOW);
			digitalWrite(STN_2_INLET_SV, LOW);
			
			TimerDelay = 0;
			
			State = STABILIZE;
			
			Serial.println("STABILIZE");
			
			break;

		case STABILIZE:
			
			if (TimerDelay < STABILIZATION_INTERVAL) { TimerDelay++; return; }
			
			RDF = 1;
			
			
			TimerDelay = 0;
			
			
			State = READ_FILL;
			ComState = WAIT_FILL;
		
		break;

		case READ_FILL:
			if (TimerDelay < COM_INTERVAL) { TimerDelay++; return; }
			digitalWrite(STN_1_INLET_SV, HIGH);		//switch off inlets

			digitalWrite(STN_2_INLET_SV, HIGH);
			State = TEST;
			ComState = IDLE;
			TimerDelay;
			break;

		case TEST:
			if (TimerDelay < TEST_INTERVAL) { TimerDelay++; return; }

			RDT = 1;
			
			State = READ_TEST;
			ComState = WAIT_TEST;
			TimerDelay = 0;
			break;

		case READ_TEST:
			if (TimerDelay < COM_INTERVAL) { TimerDelay++; return; }
			Serial.println("Exhaust");
			
			digitalWrite(CLAMP_SV, HIGH);			//switch off clamp
			digitalWrite(STN_1_EXHAUST_SV, LOW);		//switch on exhaust
			digitalWrite(STN_2_EXHAUST_SV, LOW);
			
			
			TimerDelay = 0;
			State = EXHAUST;
			ComState = IDLE;
			
			break;

		case EXHAUST : 
			
			if (TimerDelay < EXHAUST_INTERVAL) { TimerDelay++; return; }

			Serial.println("Declamp");
			
			
			digitalWrite(STN_1_EXHAUST_SV, HIGH);		//switch off exhaust
			digitalWrite(STN_2_EXHAUST_SV, HIGH);

			digitalWrite(DECLAMP_SV, LOW);				//Start Declamp
			
			State = DECLAMP;
			ComState = WAIT_RESULT;
			TimerDelay = 0;
			break;

		case DECLAMP:
			if(TimerDelay == 0 ) RDS = 1;
			if (TimerDelay < DECLAMP_INTERVAL) { TimerDelay++; return; }
			
			

			digitalWrite(DECLAMP_SV, HIGH);
			
			
			State = READ_RESULT;
			
			TimerDelay = 0;
			

			break;

		case READ_RESULT:
			if (TimerDelay < COM_INTERVAL) { TimerDelay++; return; }

			Reset();
			break;

		case ERROR:
			if (TimerDelay < DECLAMP_INTERVAL) { TimerDelay++; return; }
			Reset();
			break;

	}


	
}

void Reset()
{
	
	TimerDelay = 0;
	Fill_Pressure_1 = Fill_Pressure_2 = Test_Pressure_1 = Test_Pressure_2 = PressureDifference_1 = PressureDifference_2 = 0;
	digitalWrite(STN_1_INLET_SV, HIGH);
	digitalWrite(STN_1_EXHAUST_SV, HIGH);
	digitalWrite(STN_2_INLET_SV, HIGH);
	digitalWrite(STN_2_EXHAUST_SV, HIGH);
	digitalWrite(CLAMP_SV, HIGH);
	digitalWrite(DECLAMP_SV, HIGH);
	digitalWrite(STN_1_GRN_LMP, HIGH);
	digitalWrite(STN_1_RED_LMP, HIGH);
	digitalWrite(STN_2_GRN_LMP, HIGH);
	digitalWrite(STN_2_RED_LMP, HIGH);

	State = IDLE;
	ComState = IDLE;
	Serial.println("IDLE");
}

void Error(FAILURE_REASON reason)
{
	switch (reason)
	{
	case NO_PRESSURE:
		Serial.println("INPUT Px FAIL");
		digitalWrite(STN_1_RED_LMP, LOW);
		digitalWrite(STN_2_RED_LMP, LOW);
		break;

	case CLAMP_FAILURE:
		Serial.println("CLAM FAILURE");
		digitalWrite(CLAMP_SV, HIGH);		//clamp off
		digitalWrite(DECLAMP_SV, LOW);		//declamp on
		digitalWrite(STN_1_RED_LMP, LOW);
		digitalWrite(STN_2_RED_LMP, LOW);
		break;

	case FILL_FAILURE:
		
		break;

	default:
		break;
	}
	State = ERROR;
	TimerDelay = 0;
	
}

void RS485_Send(const char*data, unsigned int size)
{
	digitalWrite(RS485_ENABLE, HIGH);
	Serial1.write(data, size);
	digitalWrite(RS485_ENABLE, LOW);
}