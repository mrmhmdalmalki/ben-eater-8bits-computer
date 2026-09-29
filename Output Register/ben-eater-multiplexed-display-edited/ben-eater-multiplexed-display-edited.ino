/**
 * This sketch is specifically for programming the EEPROM used in the 8-bit
 * decimal display decoder described in https://youtu.be/dLh1n2dErzE
 *
 * EDITED: the segment bit patterns are remapped so that the EEPROM data pins
 * connect to the 7-segment display pins in plain order:
 *
 *   I/O0 -> pin A   (middle bar,        standard segment g)
 *   I/O1 -> pin B   (upper-left bar,    standard segment f)
 *   I/O2 -> pin C   (top bar,           standard segment a)
 *   I/O3 -> pin D   (upper-right bar,   standard segment b)
 *   I/O4 -> pin E   (lower-left bar,    standard segment e)
 *   I/O5 -> pin F   (bottom bar,        standard segment d)
 *   I/O6 -> pin G   (lower-right bar,   standard segment c)
 *   I/O7 -> pin H   (decimal point,     standard segment dp)
 */
#define SHIFT_DATA 2
#define SHIFT_CLK 3
#define SHIFT_LATCH 4
#define EEPROM_D0 5
#define EEPROM_D7 12
#define WRITE_EN 13

// One bit per physical display segment, named by its position on the display.
// The bit number is the EEPROM I/O line (and therefore the display pin letter).
#define SEG_MIDDLE      0x01  // I/O0 -> pin A
#define SEG_UPPER_LEFT  0x02  // I/O1 -> pin B
#define SEG_TOP         0x04  // I/O2 -> pin C
#define SEG_UPPER_RIGHT 0x08  // I/O3 -> pin D
#define SEG_LOWER_LEFT  0x10  // I/O4 -> pin E
#define SEG_BOTTOM      0x20  // I/O5 -> pin F
#define SEG_LOWER_RIGHT 0x40  // I/O6 -> pin G
#define SEG_DOT         0x80  // I/O7 -> pin H

/*
   Output the address bits and outputEnable signal using shift registers.
*/
void setAddress(int address, bool outputEnable) {
  shiftOut(SHIFT_DATA, SHIFT_CLK, MSBFIRST, (address >> 8) | (outputEnable ? 0x00 : 0x80));
  shiftOut(SHIFT_DATA, SHIFT_CLK, MSBFIRST, address);

  digitalWrite(SHIFT_LATCH, LOW);
  digitalWrite(SHIFT_LATCH, HIGH);
  digitalWrite(SHIFT_LATCH, LOW);
}


/*
   Read a byte from the EEPROM at the specified address.
*/
byte readEEPROM(int address) {
  for (int pin = EEPROM_D0; pin <= EEPROM_D7; pin += 1) {
    pinMode(pin, INPUT);
  }
  setAddress(address, /*outputEnable*/ true);

  byte data = 0;
  for (int pin = EEPROM_D7; pin >= EEPROM_D0; pin -= 1) {
    data = (data << 1) + digitalRead(pin);
  }
  return data;
}


/*
   Write a byte to the EEPROM at the specified address.
*/
void writeEEPROM(int address, byte data) {
  setAddress(address, /*outputEnable*/ false);
  for (int pin = EEPROM_D0; pin <= EEPROM_D7; pin += 1) {
    pinMode(pin, OUTPUT);
  }

  for (int pin = EEPROM_D0; pin <= EEPROM_D7; pin += 1) {
    digitalWrite(pin, data & 1);
    data = data >> 1;
  }
  digitalWrite(WRITE_EN, LOW);
  delayMicroseconds(1);
  digitalWrite(WRITE_EN, HIGH);
  delay(10);
}


/*
   Read the contents of the EEPROM and print them to the serial monitor.
*/
void printContents() {
  for (int base = 0; base <= 255; base += 16) {
    byte data[16];
    for (int offset = 0; offset <= 15; offset += 1) {
      data[offset] = readEEPROM(base + offset);
    }

    char buf[80];
    sprintf(buf, "%03x:  %02x %02x %02x %02x %02x %02x %02x %02x   %02x %02x %02x %02x %02x %02x %02x %02x",
            base, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7],
            data[8], data[9], data[10], data[11], data[12], data[13], data[14], data[15]);

    Serial.println(buf);
  }
}


void setup() {
  // put your setup code here, to run once:
  pinMode(SHIFT_DATA, OUTPUT);
  pinMode(SHIFT_CLK, OUTPUT);
  pinMode(SHIFT_LATCH, OUTPUT);
  digitalWrite(WRITE_EN, HIGH);
  pinMode(WRITE_EN, OUTPUT);
  Serial.begin(57600);


  // Bit patterns for the digits 0..9, built from the segment names above.
  // Resulting bytes: 0x7e 0x48 0x3d 0x6d 0x4b 0x67 0x77 0x4c 0x7f 0x6f
  byte digits[] = {
    /* 0 */ SEG_TOP | SEG_UPPER_RIGHT | SEG_LOWER_RIGHT | SEG_BOTTOM | SEG_LOWER_LEFT | SEG_UPPER_LEFT,
    /* 1 */ SEG_UPPER_RIGHT | SEG_LOWER_RIGHT,
    /* 2 */ SEG_TOP | SEG_UPPER_RIGHT | SEG_MIDDLE | SEG_LOWER_LEFT | SEG_BOTTOM,
    /* 3 */ SEG_TOP | SEG_UPPER_RIGHT | SEG_MIDDLE | SEG_LOWER_RIGHT | SEG_BOTTOM,
    /* 4 */ SEG_UPPER_LEFT | SEG_UPPER_RIGHT | SEG_MIDDLE | SEG_LOWER_RIGHT,
    /* 5 */ SEG_TOP | SEG_UPPER_LEFT | SEG_MIDDLE | SEG_LOWER_RIGHT | SEG_BOTTOM,
    /* 6 */ SEG_TOP | SEG_UPPER_LEFT | SEG_MIDDLE | SEG_LOWER_LEFT | SEG_LOWER_RIGHT | SEG_BOTTOM,
    /* 7 */ SEG_TOP | SEG_UPPER_RIGHT | SEG_LOWER_RIGHT,
    /* 8 */ SEG_TOP | SEG_UPPER_LEFT | SEG_UPPER_RIGHT | SEG_MIDDLE | SEG_LOWER_LEFT | SEG_LOWER_RIGHT | SEG_BOTTOM,
    /* 9 */ SEG_TOP | SEG_UPPER_LEFT | SEG_UPPER_RIGHT | SEG_MIDDLE | SEG_LOWER_RIGHT | SEG_BOTTOM,
  };

  // Minus sign for negative numbers: just the middle bar.
  const byte MINUS = SEG_MIDDLE;

  Serial.println("Programming ones place");
  for (int value = 0; value <= 255; value += 1) {
    writeEEPROM(value, digits[value % 10]);
  }
  Serial.println("Programming tens place");
  for (int value = 0; value <= 255; value += 1) {
    writeEEPROM(value + 256, digits[(value / 10) % 10]);
  }
  Serial.println("Programming hundreds place");
  for (int value = 0; value <= 255; value += 1) {
    writeEEPROM(value + 512, digits[(value / 100) % 10]);
  }
  Serial.println("Programming sign");
  for (int value = 0; value <= 255; value += 1) {
    writeEEPROM(value + 768, 0);
  }

  Serial.println("Programming ones place (twos complement)");
  for (int value = -128; value <= 127; value += 1) {
    writeEEPROM((byte)value + 1024, digits[abs(value) % 10]);
  }
  Serial.println("Programming tens place (twos complement)");
  for (int value = -128; value <= 127; value += 1) {
    writeEEPROM((byte)value + 1280, digits[abs(value / 10) % 10]);
  }
  Serial.println("Programming hundreds place (twos complement)");
  for (int value = -128; value <= 127; value += 1) {
    writeEEPROM((byte)value + 1536, digits[abs(value / 100) % 10]);
  }
  Serial.println("Programming sign (twos complement)");
  for (int value = -128; value <= 127; value += 1) {
    if (value < 0) {
      writeEEPROM((byte)value + 1792, MINUS);
    } else {
      writeEEPROM((byte)value + 1792, 0);
    }
  }

  // Read and print out the contents of the EERPROM
  Serial.println("Reading EEPROM");
  printContents();
}


void loop() {
  // put your main code here, to run repeatedly:

}
