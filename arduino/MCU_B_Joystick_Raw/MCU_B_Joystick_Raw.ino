#include <Keypad.h>

/* Button definitions */
const int BUTTON_ROWS = 5;
const int BUTTON_COLS = 6;
char buttonValues[BUTTON_ROWS][BUTTON_COLS] = {
  { 0x01,0x02,0x03,0x04,0x05,0x06 },
  { 0x07,0x08,0x09,0x0A,0x0B,0x0C },
  { 0x0D,0x0E,0x0F,0x10,0x11,0x12 },
  { 0x13,0x14,0x15,0x16,0x17,0x18 },
  { 0x19,0x1A,0x1B,0x1C,0x1D,0x1E },
};
/* Pin definitions */
byte buttonRowPins[BUTTON_ROWS] = {6, 7, 8, 9, 10};
byte buttonColPins[BUTTON_COLS] = {0, 1, 2, 3, 4, 5};

/* Button setup */
Keypad keypad = Keypad(makeKeymap(buttonValues), buttonRowPins, buttonColPins, BUTTON_ROWS, BUTTON_COLS);
unsigned long startMs;
bool startupRan = false;

void setup()
{
  Serial.begin(9600);

  // Set LED pins
  pinMode(13, OUTPUT); // Board LED pin

   // Put ALL LEDs ON initially
  digitalWrite(13, HIGH);

  // Calculate start time
  startMs = millis();
}

void loop()
{
  unsigned long msNow = millis();
  
  /*
   * Illuminate all LEDs when we power up for 0.5s
   */
  if (!startupRan)
  {
    if (msNow - startMs > 500)
    {
      digitalWrite(13, LOW);
      startupRan = true;
    }
  }

  /*
   * Process buttons
   */
  bool buttonChanged = keypad.getKeys();
  if (buttonChanged)
  {
    // Expect we only press 1 button at a time
    Key& button = keypad.key[0];
    if (button.kchar != NO_KEY)
    {
      bool isPressed = button.kstate == PRESSED || button.kstate == HOLD;
      Joystick.button((byte)button.kchar, isPressed);
    }
  }
}
