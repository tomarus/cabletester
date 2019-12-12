#include <Adafruit_SSD1306.h>

#define OLED_ADDR 0x3C
Adafruit_SSD1306 display(128, 64);

#define PIN_4051_S0 5
#define PIN_4051_S1 4
#define PIN_4051_S2 3
#define PIN_4051_A A0

// set DEBUG to true to display raw ADC values instead of just "OK"
const bool DEBUG = false;

const char *labels[] = {
	"-12v", "GND1", "GND2", "GND3", "+12v", "+5v", "GATE", "CV"};

// map 4051 input pins to display order
// must match label order
// see read_values()
// this is really hacky and ugly and depends on your wiring
const int order[] = {
	6, // cv
	5, // +5v
	2, // gnd2
	7, // gate
	0, // -12v
	4, // +12v
	1, // gnd1
	3  // gnd3
};

// allow some fluctuation on our ADC
const int margin = 20;

// expected values (+/- margin)
// might need some tweaking
// this is really hacky too
const int expect[] = {
	746, 544, 395, 286, 204, 130, 80, 35};

int cable_status = 0;
int prev_status = -1;
const char *statuses[] = {
	"OK, Boomer",
	"NO CABLE",
	"SHORTED",
	"BAD CABLE"};

int values[] = {1, 1, 1, 1, 1, 1, 1, 1};
int newvalues[] = {0, 0, 0, 0, 0, 0, 0, 0};

void setup()
{
	pinMode(PIN_4051_S0, OUTPUT);
	pinMode(PIN_4051_S1, OUTPUT);
	pinMode(PIN_4051_S2, OUTPUT);

	display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
	display.clearDisplay();

	draw_status();

	display.setTextColor(WHITE, BLACK);
	display.setTextSize(1);
	for (int i = 0; i < 8; i++)
	{
		int x = (i & 3) * 32;
		int y = i < 4 ? 18 : 42;
		display.setCursor(x + 4, y);
		display.print(labels[i]);
	}

	hdotline(39);
	vdotline(31);
	vdotline(63);
	vdotline(95);
	display.display();
}

void loop()
{
	read_values();
	check_status();

	display.setTextSize(1);
	for (int i = 0; i < 8; i++)
	{
		draw_value(i);
	}
	draw_status();
	display.display();
}

void hdotline(int y)
{
	for (int i = 1; i < display.width(); i += 2)
	{
		display.drawPixel(i, y, SSD1306_WHITE);
	}
}

void vdotline(int x)
{
	for (int i = 17; i < display.height(); i += 2)
	{
		display.drawPixel(x, i, SSD1306_WHITE);
	}
}

// inmargin checks if value is within the expected range
bool inmargin(const int value, const int expect)
{
	return value > expect - margin && value < expect + margin;
}

// check_status checks for shorts, nr of unconnected wires, etc
void check_status()
{
	int allok = 0;
	int nc = 0;
	int shrt = 0;

	for (int i = 0; i < 8; i++)
	{
		int prev = i == 0 ? 7 : i - 1;
		int next = i == 7 ? 0 : i + 1;

		if (values[i] < 5)
		{
			nc++;
		}
		else if (inmargin(values[i], values[prev]) || inmargin(values[i], values[next]))
		{
			shrt++;
		}
		else if (inmargin(values[i], expect[i]))
		{
			allok++;
		}
	}

	// advanced AI to decide on the cable status
	if (allok == 8 || (allok == 5 && nc == 3))
	{
		// everything should be ok
		cable_status = 0;
	}
	else if (nc == 8)
	{
		// no cable
		cable_status = 1;
	}
	else if (shrt > 0)
	{
		// shorted somewhere
		cable_status = 2;
	}
	else
	{
		// otherwise a bad cable
		cable_status = 3;
	}
}

// draw_status renders the global status on top of the screen
void draw_status()
{
	if (cable_status != prev_status)
	{
		prev_status = cable_status;
		display.setTextSize(2);
		display.setCursor(0, 0);
		display.print("          ");
		display.setCursor(0, 0);
		display.print(statuses[cable_status]);
	}
}

// align_value right aligns a 4-digit number for a 6px font width.
void align_value(int x, int y, int v)
{
	int off = v < 10 ? 18 : v < 100 ? 12 : v < 1000 ? 6 : 0;
	display.setCursor(x + off, y);
	display.print(v);
}

// draw_value renders one of the eight display boxes.
void draw_value(const int n)
{
	if (newvalues[n] == values[n])
		// return early, do nothing if the value isn't changed
		return;

	values[n] = newvalues[n];
	int v = values[n];

	int x = (n & 3) * 32;
	int y = n < 4 ? 28 : 52;
	display.setCursor(x, y);
	display.print("     ");

	if (v < 5)
	{
		// return early, 0 volt, unconnected wire,
		// display nothing for unconnected wires
		return;
	}

	int prev = n == 0 ? 7 : n - 1;
	int next = n == 7 ? 0 : n + 1;
	if (inmargin(values[n], values[prev]) || inmargin(values[n], values[next]))
	{
		// If our next or previous value is the same (+/- margin)
		// there is probably a short circuit.
		display.setCursor(x + 1, y);
		display.print("SHORT");
	}
	else if (inmargin(values[n], expect[n]))
	{
		// Measurements are within limits
		// Display OK or it's raw value
		if (DEBUG)
		{
			align_value(x + 4, y, v);
		}
		else
		{
			display.setCursor(x + 16, y);
			display.print("OK");
		}
	}
	else
	{
		// Display raw ADC value if out of limits.
		align_value(x + 4, y, v);
	}
}

// myAnalogRead reads the analog input twice
// to stabilize the ADC input before returning
// a value.
int myAnalogRead(int pin)
{
	analogRead(pin);
	return analogRead(pin);
}

// read_values updates all values from the 4051 mux.
void read_values()
{
	for (int i = 0; i < 8; i++)
	{
		digitalWrite(PIN_4051_S0, i & 1);
		digitalWrite(PIN_4051_S1, i & 2);
		digitalWrite(PIN_4051_S2, i & 4);
		newvalues[order[i]] = myAnalogRead(PIN_4051_A);
	}
}
