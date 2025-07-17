//#define DEBUG_PULSES

// HexOutput defines if the serial input/ouput are Hex bytes (e.g. to communicate with ESP8266) or a Hex string (e.g.to communicate with the Arduino IDE serail monitor).
// Leave the line uncommented to communicate using Hex bytes, or comment it to communicate using Hex strings.
//#define HexOutput

const byte OUTPUT_PIN = 11;
const byte INPUT_PIN = 3; //Only pin 2 or 3 can be used, interput is needed
const byte PULSE_BUFFER_SIZE = 200;

volatile unsigned long timeLowTransition = 0;
volatile byte bufferReadPosition = 0;
volatile byte bufferWritePosition = 0;
volatile byte pulseBuffer[PULSE_BUFFER_SIZE];

#ifdef DEBUG_PULSES
String pulseLengths;
#endif

#ifdef HexOutput
const byte numByteBuffer = 16; //UART RX Buffer size
const char endMarker = 0x0A; //UART command delimiter (RX)
#endif

void setup()
{
  pinMode(OUTPUT_PIN, OUTPUT);
  digitalWrite(OUTPUT_PIN, LOW);
  pinMode(INPUT_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(INPUT_PIN), busChange, CHANGE);
  delay(5000);
  Serial.begin(9600);
}

// This interrupt handler receives data from a remote slink device
void busChange()
{
  static unsigned long timeOfPreviousInterrupt = 0;
  unsigned long timeNow = micros();

  if (timeNow - timeOfPreviousInterrupt < 100) {
    return;
  }
  timeOfPreviousInterrupt = timeNow;

 int busState = digitalRead(INPUT_PIN);
  if (busState == LOW) {
    timeLowTransition = timeNow;
    return;
  }

  // Bus is high. The time that the bus has been low determines what
  // has happened. Let's store this information for analysis outside
  // of the interrupt handler.
  int timeLow = timeNow - timeLowTransition;

  if ((bufferWritePosition + 1) % PULSE_BUFFER_SIZE == bufferReadPosition) {
#if !defined(HexOutput)
    Serial.println(F("Pulse buffer overflow when receiving data"));
#endif
    return;
  }

  // Divide by 10 to make the pulse length fit in 8 bits
  pulseBuffer[bufferWritePosition] = min(255, timeLow / 10);
  bufferWritePosition = (bufferWritePosition + 1) % PULSE_BUFFER_SIZE;
}

void processSlinkInput()
{
  static byte currentByte = 0;
  static byte currentBit = 0;
  static bool partialOutput = false;

  while (bufferReadPosition != bufferWritePosition) {
    int timeLow = pulseBuffer[bufferReadPosition] * 10;

    bufferReadPosition = (bufferReadPosition + 1) % PULSE_BUFFER_SIZE;

#ifdef DEBUG_PULSES
    if (timeLow > 2000) {
      pulseLengths = String();
    }
    else {
      pulseLengths += " ";
    }
    pulseLengths += String(timeLow, DEC);
#endif

    if (timeLow > 2000) {
      // 2400 us -> new data sequence               
      if (partialOutput) {
#ifdef HexOutput 
        Serial.write(0x0A);
#else
        if (currentBit != 0) {
          Serial.print(F("!Discarding "));
          Serial.print(currentBit);
          Serial.print(F(" stray bits"));
        }
        Serial.print('\n');
#endif
        partialOutput = false;
      }

      currentBit = 0;
      continue;
    }

    partialOutput = true;
    currentBit += 1;
    if (timeLow > 900) {
      // 1200 us -> bit == 1
      bitSet(currentByte, 8 - currentBit);
    }
    else {
      // 600 us -> bit == 0
      bitClear(currentByte, 8 - currentBit);
    }

    if (currentBit == 8) {

#ifdef HexOutput
      Serial.write(currentByte);
#else
      if (currentByte <= 0xF) {
        Serial.print(0, HEX);
      }
      Serial.print(currentByte, HEX);
#endif
      currentBit = 0;
    }
  }

  if (partialOutput && isBusIdle()) {
#ifdef HexOutput
    Serial.write(0x0A);
#else
    Serial.print('\n');
#endif
    partialOutput = false;
  }
}

bool isBusIdle()
{
  noInterrupts();
  bool isBusIdle = micros() - timeLowTransition > 1200 + 600 + 20000;
  interrupts();
  return isBusIdle;
}

void sendPulseDelimiter()
{
  digitalWrite(OUTPUT_PIN, LOW);
  delayMicroseconds(600);
}

void sendSyncPulse()
{
  digitalWrite(OUTPUT_PIN, HIGH);
  delayMicroseconds(2400);
  sendPulseDelimiter();
}

void sendBit(int bit)
{
  digitalWrite(OUTPUT_PIN, HIGH);
  if (bit) {
    delayMicroseconds(1200);
  }
  else {
    delayMicroseconds(600);
  }
  sendPulseDelimiter();
}

void sendByte(int value)
{
  for (int i = 7; i >= 0; --i) {
    sendBit(bitRead(value, i));
  }
}

void idleAfterCommand()
{
  delayMicroseconds(20000);
}

bool sendCommand(byte command[], int commandLength)
{
  if (!isBusIdle()) {
    return false;
  }

  noInterrupts();
  sendSyncPulse();
  for (int i = 0; i < commandLength; ++i) {
    sendByte(command[i]);
  }

  // Clear interrupt flags because interrupts triggered when we sent
  // the command and the interrupts are queued for processing once
  // interrupts are re-enabled.
  EIFR = bit(INTF0) | bit(INTF1);

  interrupts();
  idleAfterCommand();
  return true;
}

void processSerialInput()
{
#ifdef HexOutput
  static byte ByteBuffer[numByteBuffer];
  static boolean newData = false;
  static int BufferIndex = 0;
  byte ReadByte;
  
  while (Serial.available() > 0 && newData == false) {
      ReadByte = Serial.read();
      if (ReadByte != endMarker) {
        ByteBuffer[BufferIndex] = ReadByte;
        BufferIndex++;
        if (BufferIndex >= numByteBuffer) {
          BufferIndex = numByteBuffer - 1;
        }
      }
      else {
        newData = true;
      }
    }
  if (newData == true) {
    byte commandBytes[BufferIndex];
    for (int i = 0; i <= BufferIndex - 1 ; ++i) {
      commandBytes[i] = ByteBuffer[i];
    }

    if (sendCommand(commandBytes, BufferIndex)) {
    // If sendCommand is successful read setup to read the next command from the Serial
      newData = false;
      BufferIndex = 0;
    }
  }
#else
  static String bytesReceived;

  while (Serial.available()) {
    bytesReceived += char(Serial.read());
  }

  const int eolPos = bytesReceived.indexOf("\n");
  if (eolPos == -1) {
    return;
  }

  const String command = bytesReceived.substring(0, eolPos);
  bytesReceived.remove(0, command.length() + 1);

#ifdef DEBUG_PULSES
  if (command == "pulsedump") {
    Serial.println(pulseLengths);
    return;
  }
#endif

  // A hexadecimal command is expected
  if (command.length() % 2 != 0) {
    Serial.println(F("Uneven length of serial input"));
    return;
  }

  for (int i = 0; i < command.length(); ++i) {
    if (!isHexadecimalDigit(command[i])) {
      Serial.println(F("Non-hexadecimal serial input"));
      return;
    }
  }

  byte commandBytes[command.length() / 2];
  for (int i = 0; i < sizeof(commandBytes); ++i) {
    String hexByte = command.substring(2 * i, 2 * i + 2);
    commandBytes[i] = strtol(hexByte.c_str(), NULL, 16);
  }

  if (!sendCommand(commandBytes, sizeof(commandBytes))) {
    // If send fails, re-queue command
    bytesReceived = command + "\n" + bytesReceived;
  } 
#endif
}

void loop()
{
  processSlinkInput();
  processSerialInput();
}
