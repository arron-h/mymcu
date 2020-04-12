#include <Keypad.h>
#include <LiquidCrystal.h>
#include <usb_desc.h>

#define COM_FREQUENCY_LENGTH 7U
#define NAV_FREQUENCY_LENGTH 6U
#define LCD_OFF_TIMEOUT 30000U
#define LCD_SHUTDOWN_TEXT 20000U

/* Remove until buttons are wired 
const int BUTTON_ROWS = 4;
const int BUTTON_COLS = 5;
char buttonValues[BUTTON_ROWS][BUTTON_COLS] = {
  { 0x01,0x02,0x03,0x04,0x05 },
  { 0x06,0x07,0x08,0x09,0x0A },
  { 0x0B,0x0C,0x0D,0x0E,0x0F },
  { 0x10,0x11,0x12,0x13,0x14 }
};
byte buttonRowPins[BUTTON_ROWS] = {9, 10, 11, 12};
byte buttonColPins[BUTTON_COLS] = {0, 1, 6, 7, 8};
*/

/* Button definitions */
const int BUTTON_ROWS = 1;
const int BUTTON_COLS = 3;
char buttonValues[BUTTON_ROWS][BUTTON_COLS] = {
  { 0x01,0x02,0x03 }
};

/* Pin definitions */
const int cautionLedPin = 16;
const int warningLedPin = 17;
const int lcdEnablePin  = 18;
const int lcdRSPin      = 19;
const int lcdRWPin      = 20;
const int lcdD4Pin      = 9;
const int lcdD5Pin      = 10;
const int lcdD6Pin      = 11;
const int lcdD7Pin      = 12;
const int lcdBacklightPin = 5;
byte buttonRowPins[BUTTON_ROWS] = {0};
byte buttonColPins[BUTTON_COLS] = {1,2,3};

/* HID parameters */
byte hidDataBuffer[RAWHID_RX_SIZE];

/* Button setup */
Keypad keypad = Keypad(makeKeymap(buttonValues), buttonRowPins, buttonColPins, BUTTON_ROWS, BUTTON_COLS);

/* LCD setup */
LiquidCrystal lcd = LiquidCrystal(lcdRSPin, lcdRWPin, lcdEnablePin, lcdD4Pin, lcdD5Pin, lcdD6Pin, lcdD7Pin);
char lcdBuffer[2][16];
struct LcdOffset
{
  int row;
  int col;
}
const LCD_OFFSETS[4] = 
{
  { 0, 0 }, // MYMCU_CMDS_COM_ACT_CHANGED
  { 0, 9 }, // MYMCU_CMDS_COM_SBY_CHANGED
  { 1, 0 }, // MYMCU_CMDS_NAV_ACT_CHANGED
  { 1, 9 }, // MYMCU_CMDS_NAV_SBY_CHANGED
};
const char LCD_FLIPFLOPS[2] = {127,126};
enum LcdState
{
  LCDSTATE_OFF,
  LCDSTATE_ON,

  LCDSTATE_INVALID = 0xFF
};
LcdState lcdState = LCDSTATE_INVALID;

/* MyMCU Message Protol */
const byte MYMCU_HEADER[2] = {0xFC,0xAB};
enum MyMcuMessageCommands
{
  MYMCU_CMDS_ANNUC_CHANGED = 0x1,
  MYMCU_CMDS_COM_ACT_CHANGED,
  MYMCU_CMDS_COM_SBY_CHANGED,
  MYMCU_CMDS_NAV_ACT_CHANGED,
  MYMCU_CMDS_NAV_SBY_CHANGED,
  MYMCU_CMDS_LCD_ON,
  MYMCU_CMDS_LCD_OFF,
  MYMCU_CMDS_LCD_HEARTBEAT,
  MYMCU_CMDS_LCD_STRING,

  MYMCU_CMDS_MAX,
  MYMCU_CMDS_LCD_OFFSET = MYMCU_CMDS_COM_ACT_CHANGED
};

enum MyMcuAnnuciatorValues
{
  MYMCU_ANNUC_CAUT_ON  = 0x1,
  MYMUC_ANNUC_CAUT_OFF = 0x2,
  MYMCU_ANNUC_WARN_ON  = 0x4,
  MYMUC_ANNUC_WARN_OFF = 0x8,
};

unsigned long startMs; 
unsigned long lastHeartbeat;
boolean startupRan = false;

void processAnnunciator(byte value)
{
  switch(value)
  {
    case MYMCU_ANNUC_CAUT_ON:
      digitalWrite(cautionLedPin, HIGH);
      break;
    case MYMUC_ANNUC_CAUT_OFF:
      digitalWrite(cautionLedPin, LOW);
      break;
    case MYMCU_ANNUC_WARN_ON:
      digitalWrite(warningLedPin, HIGH);
      break;
    case MYMUC_ANNUC_WARN_OFF:
      digitalWrite(warningLedPin, LOW);
      break;
  }
}

void lcdPrintString(byte* string, byte stringLength)
{
  lcd.setCursor(0, 0);
  lcd.write((const char*)string, (size_t)stringLength);
}

void setLCDState(LcdState newState)
{
  if (newState != lcdState)
  {
    if (newState == LCDSTATE_OFF)
    {
      lcd.noDisplay();
      digitalWrite(lcdBacklightPin, LOW);
    }
    else if (newState == LCDSTATE_ON)
    {
      lcd.display();
      lcd.clear();
      digitalWrite(lcdBacklightPin, HIGH);
    }
    lcdState = newState;
  }
}

void processLCDRadio(byte freqChanged, byte* value, size_t valueLength)
{
  if (valueLength < NAV_FREQUENCY_LENGTH)
    return;

  int idx = freqChanged - MYMCU_CMDS_LCD_OFFSET;
  if (idx >= 4)
    return;
  
  const LcdOffset& lcdOffset = LCD_OFFSETS[idx];
  lcd.setCursor(lcdOffset.col, lcdOffset.row);
  
  size_t freqLength;
  if (freqChanged == MYMCU_CMDS_COM_ACT_CHANGED || freqChanged == MYMCU_CMDS_COM_SBY_CHANGED)
  {
    freqLength = COM_FREQUENCY_LENGTH;
  }
  else
  {
    freqLength = NAV_FREQUENCY_LENGTH;
  }

  // Write new contents
  lcd.write((const char*)value, min(freqLength, valueLength));

  // Add flip-flops
  lcd.setCursor(7, 0);
  lcd.write(LCD_FLIPFLOPS, 2);
  lcd.setCursor(7, 1);
  lcd.write(LCD_FLIPFLOPS, 2);
}

void setup()
{
  Serial.begin(9600);

  // Set LED pins
  pinMode(cautionLedPin, OUTPUT);
  pinMode(warningLedPin, OUTPUT);
  pinMode(13, OUTPUT); // Board LED pin

   // Put ALL LEDs ON initially
  digitalWrite(cautionLedPin, HIGH);
  digitalWrite(warningLedPin, HIGH);
  digitalWrite(13, HIGH);

  // Clear LCD contents
  pinMode(lcdBacklightPin, OUTPUT);
  digitalWrite(lcdBacklightPin, HIGH);
  lcd.begin(16, 2);
  lcd.clear();
  
  // Calculate start time
  startMs = millis();
  lastHeartbeat = startMs;
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
      digitalWrite(cautionLedPin, LOW);
      digitalWrite(warningLedPin, LOW);
      digitalWrite(13, LOW);
      setLCDState(LCDSTATE_OFF);
      startupRan = true;
    }
  }

  if (msNow - lastHeartbeat > LCD_OFF_TIMEOUT)
  {
    setLCDState(LCDSTATE_OFF);
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

  /*
   * Read HID data
   */
  int bytesRead = RawHID.recv(hidDataBuffer, 0);
  if (bytesRead >= 4)
  {
    if (hidDataBuffer[0] == MYMCU_HEADER[0] && hidDataBuffer[1] == MYMCU_HEADER[1])
    {
      byte cmd = hidDataBuffer[2];
      switch(cmd)
      {
        case MYMCU_CMDS_ANNUC_CHANGED:
          processAnnunciator(hidDataBuffer[3]);
          break;
        case MYMCU_CMDS_COM_ACT_CHANGED:
        case MYMCU_CMDS_COM_SBY_CHANGED:
        case MYMCU_CMDS_NAV_ACT_CHANGED:
        case MYMCU_CMDS_NAV_SBY_CHANGED:
          processLCDRadio(cmd, hidDataBuffer + 3, (size_t)(bytesRead - 3));
          break;
        case MYMCU_CMDS_LCD_ON:
          setLCDState(LCDSTATE_ON);
          break;
        case MYMCU_CMDS_LCD_OFF:
          setLCDState(LCDSTATE_OFF);
          break;
        case MYMCU_CMDS_LCD_HEARTBEAT:
          lastHeartbeat = millis();
          break;
        case MYMCU_CMDS_LCD_STRING:
          lcdPrintString(hidDataBuffer + 4, hidDataBuffer[3]);
        default:
          break;
      }
    }
  }
}
