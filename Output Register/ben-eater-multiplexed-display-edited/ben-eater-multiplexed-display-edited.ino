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

// Which 256-byte table each digit uses.
//
// The digit counter (74LS107) counts 0,1,2,3. A8/A9 follow the count like
// Ben's video, so the EEPROM reads table = count. The 74LS139 lights one
// display per count. This chip has a dead block at 0x400..0x4ff (A10 high,
// table 0) where only a constant survives, so the SIGN digit must be the one
// lit during count 0. The display board is therefore wired in the reverse
// order of Ben's video:
//
//   74LS139 output     Ben's video   this board    table
//   2Y0 (pin 12)       ones          SIGN          0
//   2Y1 (pin 11)       tens          HUNDREDS      1
//   2Y2 (pin 10)       hundreds      TENS          2
//   2Y3 (pin 9)        sign          ONES          3
//
// With a healthy chip wired exactly like Ben, use ONES 0, TENS 1, HUNDREDS 2,
// SIGN 3 and set UNSIGNED_ON_A10_HIGH to 0.
#define SIGN_TABLE      0
#define HUNDREDS_TABLE  1
#define TENS_TABLE      2
#define ONES_TABLE      3

#define SIGN_BASE       (SIGN_TABLE * 256)
#define HUNDREDS_BASE   (HUNDREDS_TABLE * 256)
#define TENS_BASE       (TENS_TABLE * 256)
#define ONES_BASE       (ONES_TABLE * 256)

// Which half of the chip (A10 low or A10 high) holds the twos complement tables.
//
// UNSIGNED_ON_A10_HIGH = 1: A10 HIGH = unsigned, A10 LOW = twos complement.
//   Together with SIGN_TABLE = 0 this puts the unsigned sign table (all
//   zeros) at 0x400..0x4ff, so a dead block there holds nothing but 00.
//   Flip your mode switch the other way round compared to Ben's video.
//
// UNSIGNED_ON_A10_HIGH = 0: Ben Eater's original (A10 HIGH = twos complement).
#define UNSIGNED_ON_A10_HIGH 1

#if UNSIGNED_ON_A10_HIGH
  #define UNSIGNED_HALF 1024
  #define SIGNED_HALF   0
#else
  #define UNSIGNED_HALF 0
  #define SIGNED_HALF   1024
#endif

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
   Erase the whole chip by writing the blank value to every address.
   Prints one dot per 64 bytes so you can see it is alive.
*/
void eraseEEPROM(byte blank) {
  for (int address = 0; address < 2048; address += 1) {
    writeEEPROM(address, blank);
    if (address % 64 == 63) {
      Serial.print(".");
    }
  }
  Serial.println();
}


/*
   Read every address back and make sure it holds the blank value.
   Returns the number of bytes that did not erase.
*/
int checkErased(byte blank) {
  int errors = 0;
  for (int address = 0; address < 2048; address += 1) {
    byte actual = readEEPROM(address);
    if (actual != blank) {
      errors += 1;
      if (errors <= 20) {
        char buf[64];
        sprintf(buf, "NOT ERASED at %03x: read %02x", address, actual);
        Serial.println(buf);
      }
    }
  }
  if (errors == 0) {
    Serial.println("Erase OK: all 2048 bytes blank");
  } else {
    Serial.print("Erase FAILED: ");
    Serial.print(errors);
    Serial.println(" bytes not blank (first 20 shown)");
  }
  return errors;
}


/*
   Read the contents of the EEPROM and print them to the serial monitor.
   Dumps the whole 2K chip (all eight 256-byte tables).
*/
void printContents() {
  for (int base = 0; base < 2048; base += 16) {
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


/*
   Work out what SHOULD be stored at a given address. This mirrors the
   programming loops in setup() and is only used to check the chip afterwards.
*/
byte expectedByte(int address, const byte digits[], byte minus) {
  bool isSigned = (address / 1024) == (SIGNED_HALF / 1024);
  int table = (address % 1024) / 256 * 256;            // 0, 256, 512 or 768
  int raw = address % 256;                              // the 8-bit register value

  if (!isSigned) {
    // Unsigned mode: value is 0..255, sign digit always blank
    if (table == ONES_BASE)     return digits[raw % 10];
    if (table == TENS_BASE)     return digits[(raw / 10) % 10];
    if (table == HUNDREDS_BASE) return digits[(raw / 100) % 10];
    return 0;
  } else {
    // Twos complement mode: value is -128..127
    int value = (int8_t)raw;
    if (table == ONES_BASE)     return digits[abs(value) % 10];
    if (table == TENS_BASE)     return digits[abs(value / 10) % 10];
    if (table == HUNDREDS_BASE) return digits[abs(value / 100) % 10];
    return value < 0 ? minus : 0;
  }
}


/*
   Read every byte back and compare it with what was written. With repair set,
   any byte that does not match is rewritten and checked again, up to 3 times,
   before it is reported as an error. Returns the number of bytes still wrong.
*/
int verifyContents(const byte digits[], byte minus, bool repair) {
  int repaired = 0;
  int errors = 0;
  for (int address = 0; address < 2048; address += 1) {
    byte expected = expectedByte(address, digits, minus);
    byte actual = readEEPROM(address);
    if (actual == expected) {
      continue;
    }

    // Try to fix it in place
    byte firstRead = actual;
    for (int attempt = 0; repair && attempt < 3 && actual != expected; attempt += 1) {
      writeEEPROM(address, expected);
      actual = readEEPROM(address);
    }

    char buf[80];
    if (actual == expected) {
      repaired += 1;
      sprintf(buf, "REPAIRED %03x (value %3d, table %d): was %02x, now %02x",
              address, address % 256, address / 256, firstRead, actual);
    } else {
      errors += 1;
      sprintf(buf, "MISMATCH at %03x (value %3d, table %d): expected %02x, read %02x",
              address, address % 256, address / 256, expected, actual);
    }
    if (repaired + errors <= 60) {
      Serial.println(buf);
    }
  }

  if (repair) {
    Serial.print("Repair pass done: ");
    Serial.print(repaired);
    Serial.println(" bytes rewritten");
    return errors;
  }
  if (errors == 0) {
    Serial.println("Verify OK: all 2048 bytes match");
  } else {
    Serial.print("Verify FAILED: ");
    Serial.print(errors);
    Serial.println(" bytes wrong");
  }
  return errors;
}


/*
   Check whether two addresses share the same storage cell. Writes a different
   test value to each, then checks whether the first one changed. Restores the
   correct data afterwards.
*/
void testAlias(int a, int b, const byte digits[], byte minus) {
  writeEEPROM(a, 0x55);
  writeEEPROM(b, 0xaa);
  byte readA = readEEPROM(a);
  byte readB = readEEPROM(b);

  char buf[80];
  sprintf(buf, "Alias test %03x / %03x: read %02x / %02x -> %s",
          a, b, readA, readB,
          (readA == 0x55 && readB == 0xaa) ? "separate (OK)" : "ALIASED (chip fault)");
  Serial.println(buf);

  writeEEPROM(a, expectedByte(a, digits, minus));
  writeEEPROM(b, expectedByte(b, digits, minus));
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

  // Step 1: wipe the chip so nothing old is left behind
  Serial.println("Erasing EEPROM (writing 0xff everywhere)");
  eraseEEPROM(0xff);
  Serial.println("Checking erase");
  checkErased(0xff);

  // Step 2: program all eight tables
  Serial.print("Layout: tables  SIGN=");
  Serial.print(SIGN_TABLE);
  Serial.print("  HUNDREDS=");
  Serial.print(HUNDREDS_TABLE);
  Serial.print("  TENS=");
  Serial.print(TENS_TABLE);
  Serial.print("  ONES=");
  Serial.println(ONES_TABLE);
#if UNSIGNED_ON_A10_HIGH
  Serial.println("Mode:   A10 HIGH = unsigned, A10 LOW = twos complement (flipped)");
#else
  Serial.println("Mode:   A10 LOW = unsigned, A10 HIGH = twos complement (Ben Eater original)");
#endif

  Serial.println("Programming ones place");
  for (int value = 0; value <= 255; value += 1) {
    writeEEPROM(UNSIGNED_HALF + ONES_BASE + value, digits[value % 10]);
  }
  Serial.println("Programming tens place");
  for (int value = 0; value <= 255; value += 1) {
    writeEEPROM(UNSIGNED_HALF + TENS_BASE + value, digits[(value / 10) % 10]);
  }
  Serial.println("Programming hundreds place");
  for (int value = 0; value <= 255; value += 1) {
    writeEEPROM(UNSIGNED_HALF + HUNDREDS_BASE + value, digits[(value / 100) % 10]);
  }
  Serial.println("Programming sign");
  for (int value = 0; value <= 255; value += 1) {
    writeEEPROM(UNSIGNED_HALF + SIGN_BASE + value, 0);
  }

  Serial.println("Programming ones place (twos complement)");
  for (int value = -128; value <= 127; value += 1) {
    writeEEPROM(SIGNED_HALF + ONES_BASE + (byte)value, digits[abs(value) % 10]);
  }
  Serial.println("Programming tens place (twos complement)");
  for (int value = -128; value <= 127; value += 1) {
    writeEEPROM(SIGNED_HALF + TENS_BASE + (byte)value, digits[abs(value / 10) % 10]);
  }
  Serial.println("Programming hundreds place (twos complement)");
  for (int value = -128; value <= 127; value += 1) {
    writeEEPROM(SIGNED_HALF + HUNDREDS_BASE + (byte)value, digits[abs(value / 100) % 10]);
  }
  Serial.println("Programming sign (twos complement)");
  for (int value = -128; value <= 127; value += 1) {
    if (value < 0) {
      writeEEPROM(SIGNED_HALF + SIGN_BASE + (byte)value, MINUS);
    } else {
      writeEEPROM(SIGNED_HALF + SIGN_BASE + (byte)value, 0);
    }
  }

  // Step 3: dump everything so it can be inspected by eye
  Serial.println("Reading EEPROM");
  printContents();

  // Step 4: compare every byte against what we meant to write, fixing what we can
  Serial.println("Verifying EEPROM (with repair)");
  verifyContents(digits, MINUS, true);

  // Step 5: a second read-only pass catches bytes that were broken again by a repair
  Serial.println("Verifying EEPROM (final check)");
  int stillWrong = verifyContents(digits, MINUS, false);

  // Step 6: if anything is still wrong, test whether address pairs share a cell
  if (stillWrong > 0) {
    Serial.println("Testing for aliased addresses");
    testAlias(0x000, 0x020, digits, MINUS);   // control: first table, A10 low
    testAlias(0x400, 0x420, digits, MINUS);   // suspect block 1 (first table, A10 high)
    testAlias(0x480, 0x4a0, digits, MINUS);   // suspect block 2 (first table, A10 high)
    testAlias(0x400, 0x480, digits, MINUS);   // suspect: A7 ignored too?
    testAlias(0x440, 0x460, digits, MINUS);   // same table, A6 high
    testAlias(0x500, 0x520, digits, MINUS);   // control: tens table, A10 high
  }

  if (stillWrong == 0) {
    Serial.println("DONE: chip erased, programmed and fully verified");
  } else {
    Serial.println("DONE WITH ERRORS: see the lines above");
  }
}


void loop() {
  // put your main code here, to run repeatedly:

}
