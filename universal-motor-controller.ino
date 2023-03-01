

#include <AccelStepper.h>
#include <Servo.h>
#include "EncoderTool.h"


#define POSITION_MAX 1024
#define SERVO_MAX 180
#define POSITION_DELTA 50
#define LRPWM_STEP_DURATION 10
#define START_POSITION 512;
#define MIN_STEPPER_SPEED_REQUIREMENT 50
#define LRPWM_MAX_MOD 10


enum RUN_MODE
{
	CONTINUOUS,
	POSITIONAL
};


int runMode = RUN_MODE::CONTINUOUS;


AccelStepper *stepper,
			 *twoPinStepper;

Servo *servo;

EncoderTool::PolledEncoder encoderTool;



#define PIN_ENCODER_VIN 2
#define PIN_ENCODER_A 4
#define PIN_ENCODER_B 5
#define PIN_ENCODER_BUTTON 3

#define SERVO_SIGNAL_PIN 6

#define STEPPER_SIGNAL_PIN_1 A0
#define STEPPER_SIGNAL_PIN_2 A1
#define STEPPER_SIGNAL_PIN_3 A2
#define STEPPER_SIGNAL_PIN_4 A3

#define PIN_RPWM 9
#define PIN_LPWM 10

#define TWO_PIN_STEPPER_DIR_PIN 12
#define TWO_PIN_STEPPER_STEP_PIN 11



unsigned long lrpwmStepTimestamp = 0;

int position = START_POSITION;

int lastButtonState = 0;




void onButtonChanged(int buttonState);




void setup()
{
	Serial.begin(9600);

	pinMode(PIN_LPWM, OUTPUT);
	pinMode(PIN_RPWM, OUTPUT);

	pinMode(PIN_ENCODER_VIN, OUTPUT);

	encoderTool.begin(PIN_ENCODER_A, PIN_ENCODER_B, PIN_ENCODER_BUTTON);
	encoderTool.attachButtonCallback(onButtonChanged);

	pinMode(SERVO_SIGNAL_PIN, OUTPUT);
  	pinMode(LED_BUILTIN, OUTPUT);

	twoPinStepper = new AccelStepper(AccelStepper::DRIVER, TWO_PIN_STEPPER_STEP_PIN, TWO_PIN_STEPPER_DIR_PIN);
	twoPinStepper->setAcceleration(100000);
	twoPinStepper->setMaxSpeed(100000);

	stepper = new AccelStepper(AccelStepper::FULL4WIRE, STEPPER_SIGNAL_PIN_1, STEPPER_SIGNAL_PIN_3, STEPPER_SIGNAL_PIN_2, STEPPER_SIGNAL_PIN_4);
	stepper->setAcceleration(1000);
	stepper->setMaxSpeed(1000);

	servo = new Servo();
	servo->attach(SERVO_SIGNAL_PIN);	
}


auto lastEncoderValue = 0;

unsigned long loopCount = 0;

void loop()
{
	loopCount++;

	// Power the encoder
	digitalWrite(PIN_ENCODER_VIN, HIGH);


	auto encoderDelta = 0;

	encoderTool.tick();

	if(encoderTool.valueChanged() == true)
	{
		auto encoderValue = encoderTool.getValue();

		if(lastEncoderValue != encoderValue)
		{
			encoderDelta = encoderValue - lastEncoderValue;

			lastEncoderValue = encoderValue;
		}

		Serial.println("Delta is: " + String(encoderDelta));
	}
	

	position += encoderDelta * POSITION_DELTA;
	position = min(POSITION_MAX - 1, position);
	position = max(0, position);

	auto servoPWM = map(position, 0, POSITION_MAX, 0, SERVO_MAX);


	if(runMode == RUN_MODE::CONTINUOUS)
	{
		if((millis() / 100) % 2 == 0)
			digitalWrite(LED_BUILTIN, HIGH);
		else
			digitalWrite(LED_BUILTIN, LOW);   

		auto leftRightPWM = map(position, 0, 1024, -LRPWM_MAX_MOD, LRPWM_MAX_MOD + 1);
		
		auto modVal = millis()%(LRPWM_MAX_MOD - (abs(leftRightPWM) - 1));

		if(leftRightPWM != 0 && modVal == 0)
		{
			if(leftRightPWM > 0)
			{
				digitalWrite(PIN_LPWM, HIGH);
				digitalWrite(PIN_RPWM, LOW);
			}
			else
			{
				digitalWrite(PIN_LPWM, LOW);
				digitalWrite(PIN_RPWM, HIGH);
			}
		}
		else
		{
			digitalWrite(PIN_LPWM, LOW);
			digitalWrite(PIN_RPWM, LOW);
		}


		auto stepperSpeed = map(position, 0, 1024, -800, 800);

		if(abs(stepperSpeed) >= MIN_STEPPER_SPEED_REQUIREMENT)
		{
			stepper->setSpeed(stepperSpeed);
			stepper->runSpeed();

			twoPinStepper->setSpeed(stepperSpeed);
			twoPinStepper->runSpeed();
		}
	}
	else if(runMode == RUN_MODE::POSITIONAL)
	{
		digitalWrite(LED_BUILTIN, LOW);   
		
		if(encoderDelta != 0)
		{
			stepper->move(encoderDelta);
			twoPinStepper->move(encoderDelta);
		}

		if(stepper->distanceToGo() != 0)
			stepper->run();

		if(twoPinStepper->distanceToGo() != 0)
			twoPinStepper->run();

		if(encoderDelta == 1)
		{
			digitalWrite(PIN_LPWM, HIGH);
			digitalWrite(PIN_RPWM, LOW);
			lrpwmStepTimestamp = millis();
		}
		else if(encoderDelta == -1)
		{
			digitalWrite(PIN_LPWM, LOW);
			digitalWrite(PIN_RPWM, HIGH);
			lrpwmStepTimestamp = millis();
		}
	}
	
	servo->write(servoPWM);

	if(lrpwmStepTimestamp != 0 && (millis() - lrpwmStepTimestamp) > LRPWM_STEP_DURATION)
	{
		digitalWrite(PIN_LPWM, LOW);
		digitalWrite(PIN_RPWM, LOW);

		lrpwmStepTimestamp = 0;
	}
}



void onButtonChanged(int buttonState)
{
    Serial.print("Current button state: " + String(buttonState == LOW ? "pressed" : "released"));

	if(buttonState != lastButtonState)
	{
		if(buttonState == HIGH)
		{
			Serial.println("Button was pressed");

			if(runMode == RUN_MODE::CONTINUOUS)
			{
				stepper->setCurrentPosition(0);
				twoPinStepper->setCurrentPosition(0);

				runMode = RUN_MODE::POSITIONAL;
				Serial.println("Mode is now POSITIONAL");
			}
			else
			{
				position = START_POSITION;
				runMode = RUN_MODE::CONTINUOUS;
				Serial.println("Mode is now CONTINUOUS");
			}

			// reset the LR PWMs
			lrpwmStepTimestamp = millis();
		}

		lastButtonState = buttonState;
	}
}