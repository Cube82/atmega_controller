#include <PS2X_lib.h>
#include <VirtualWire.h>
 
// pseudo-channel to distinguish transmiters
#define channel			"1"
// ping message
#define ping			"ping"

// pins definition
#define led_pin			A5
#define transmit_pin	8

/******************************************************************
* set pins connected to PS2 controller
******************************************************************/
#define PS2_CLK			PD4 
#define PS2_ATT			PD3 
#define PS2_CMD			PD2 
#define PS2_DAT			PD1  

/******************************************************************
* select modes of PS2 controller:
*   - pressures = analog reading of push-butttons
*   - rumble    = motor rumbling
******************************************************************/
#define pressures		false
#define rumble			false

// interval at which to send ping (milliseconds)
const long interval = 900;
// will store last time ping was sent
unsigned long previousMillis = 0;

int ps2Error = 0;

/******************************************************************
* Controller class based on PS2X
******************************************************************/
class Controller : public PS2X
{
	private:

		// sticks values for comparison
		byte _lastRx = 128;
		byte _lastRy = 128;
		byte _lastLx = 128;
		byte _lastLy = 128;

	public:

		static const byte buttons_count = 16;

		unsigned int buttons_keys[buttons_count] =
		{
			PSB_SELECT,
			PSB_L3,
			PSB_R3,
			PSB_START,
			PSB_PAD_UP,
			PSB_PAD_RIGHT,
			PSB_PAD_DOWN,
			PSB_PAD_LEFT,
			PSB_L2,
			PSB_R2,
			PSB_L1,
			PSB_R1,
			PSB_TRIANGLE,
			PSB_CIRCLE,
			PSB_CROSS,
			PSB_SQUARE
		};

		const char buttons_values[buttons_count][5] =
		{
			"|se:",	// PSB_SELECT,
			"|l3:",	// PSB_L3,
			"|r3:",	// PSB_R3,
			"|st:",	// PSB_START,
			"|up:",	// PSB_PAD_UP,
			"|rt:",	// PSB_PAD_RIGHT,
			"|dn:",	// PSB_PAD_DOWN,
			"|lt:",	// PSB_PAD_LEFT,
			"|l2:",	// PSB_L2,
			"|r2:",	// PSB_R2,
			"|l1:",	// PSB_L1,
			"|r1:",	// PSB_R1,
			"|tr:",	// PSB_TRIANGLE,
			"|ci:",	// PSB_CIRCLE,
			"|cr:",	// PSB_CROSS,
			"|sq:"	// PSB_SQUARE
		};

		unsigned int sticks_keys[4] =
		{
			PSS_RX,
			PSS_RY,
			PSS_LX,
			PSS_LY
		};

		const char sticks_values[4][5] =
		{
			"|rx:",	// PSS_RX,
			"|ry:",	// PSS_RY,
			"|lx:",	// PSS_LX,
			"|ly:"	// PSS_LY
		};

		// save sticks values at the end of loop for comparison
		void SaveSticksValues()
		{
			_lastRx = Analog(PSS_RX);
			_lastRy = Analog(PSS_RY);
			_lastLx = Analog(PSS_LX);
			_lastLy = Analog(PSS_LY);
		}

		// returns true if any value of right or left analog changed since previous loop
		boolean StickValueChanged()
		{
			if (Analog(PSS_RX) != _lastRx || Analog(PSS_RY) != _lastRy || Analog(PSS_LX) != _lastLx || Analog(PSS_LY) != _lastLy)
				return true;
			else
				return false;
		}

		// returns true if given value of right or left analog changed since previous loop
		boolean StickValueChanged(byte stick)
		{
			switch (stick)
			{
			case PSS_RX:
				return Analog(PSS_RX) != _lastRx;
				break;
			case PSS_RY:
				return Analog(PSS_RY) != _lastRy;
				break;
			case PSS_LX:
				return Analog(PSS_LX) != _lastLx;
				break;
			case PSS_LY:
				return Analog(PSS_LY) != _lastLy;
				break;
			default:
				return false;
			}
		}
};

// Controller Class instance
Controller ctrl;

void setup()
{
	// settings for radio
    vw_set_tx_pin(transmit_pin);
    vw_setup(2000);

    pinMode(led_pin, OUTPUT);
	
	// setup pins and settings: GamePad(clock, command, attention, data, Pressures?, Rumble?) check for error
	ps2Error = ctrl.config_gamepad(PS2_CLK, PS2_CMD, PS2_ATT, PS2_DAT, pressures, rumble);

}
 
void loop()
{
	// skip loop if no controller found
	if (ps2Error != 0)
	{
		digitalWrite(led_pin, HIGH);
		delay(100);
		return;	
	}

	// read controller
	ctrl.read_gamepad();

	if (ctrl.NewButtonState() || ctrl.StickValueChanged())	// will be TRUE if any button changes state (on to off, or off to on) OR any stick changes value
		sendValues();	// read, format and send values from controller

	// sending ping every interval
	if (millis() - previousMillis >= interval)
	{
		sendPing();	// send ping
	}

	ctrl.SaveSticksValues();

	delay(1);
}

void sendPing()
{
	char msg[10];
	msg[0] = '\0';
	strcat(msg, "C:");
	strcat(msg, channel);
	strcat(msg, "|");
	strcat(msg, ping);

	send(msg);
}

void sendValues()
{
	// message to send
	char msg[VW_MAX_MESSAGE_LEN];
	msg[0] = '\0';

	char value[5];

	// channel
	strcat(msg, "C:");
	strcat(msg, channel);

	// stick values
	for (int i = 0; i < 4; i++)
	{
		if (ctrl.StickValueChanged(ctrl.sticks_keys[i]))
		{
			strcat(msg, ctrl.sticks_values[i]);
			itoa(ctrl.Analog(ctrl.sticks_keys[i]), value, 10);
			strcat(msg, value);
		}
	}

	// buttons values
	for (int i = 0; i < ctrl.buttons_count; i++)
	{
		if (strlen(msg) > (VW_MAX_MESSAGE_LEN - 7)) // break if next part of message won't fix in msg
			break;

		if (ctrl.NewButtonState(ctrl.buttons_keys[i]))
		{
			strcat(msg, ctrl.buttons_values[i]);
			itoa(ctrl.Button(ctrl.buttons_keys[i]), value, 10);
			strcat(msg, value);
		}
	}

	// ending pipe
	strcat(msg, "|");

	// send message
	send(msg);
}

// sending radio message
void send(char message[VW_MAX_MESSAGE_LEN])
{
	digitalWrite(led_pin, HIGH);

	vw_send((uint8_t *)message, strlen(message));
	vw_wait_tx();

	// save last send time for ping timer
	previousMillis = millis();

	digitalWrite(led_pin, LOW);
}