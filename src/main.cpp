#include "Arduino.h"

#define SER 2
#define SRCLK 3
#define RCLK 4
#define EEPROM_IO0 5
#define EEPROM_IO7 12
#define EEPROM_WE 13

void setAddress(int address, bool outputEnable) {
    digitalWrite(RCLK, LOW);
    digitalWrite(SRCLK, LOW);

    shiftOut(SER, SRCLK, MSBFIRST, (address >> 8) | (outputEnable ? 0x00 : 0x80));
    shiftOut(SER, SRCLK, MSBFIRST, address);

    digitalWrite(RCLK, HIGH);
    digitalWrite(RCLK, LOW);
}

byte readEeprom(int address) {
    for (int pin = EEPROM_IO0; pin <= EEPROM_IO7; ++pin) {
        pinMode(pin, INPUT);
    }
    setAddress(address, true);

    byte data = 0;
    for (int pin = EEPROM_IO7; pin >= EEPROM_IO0; --pin) {
        data = (data << 1) | digitalRead(pin);
    }
    return data;
}

void writeEeprom(int address, byte data) {
    setAddress(address, false);
    int lastbit;
    for (int pin = EEPROM_IO0; pin <= EEPROM_IO7; ++pin) {
        pinMode(pin, OUTPUT);
        lastbit = data & 1;
        digitalWrite(pin, lastbit);
        data >>= 1;
    }
    digitalWrite(EEPROM_WE, LOW);
    delayMicroseconds(1);
    digitalWrite(EEPROM_WE, HIGH);

    // write cycle is finished when IO7 shows the correct data
    for (int pin = EEPROM_IO0; pin <= EEPROM_IO7; ++pin) {
        pinMode(pin, INPUT);
    }
    setAddress(address, true);
    while (digitalRead(EEPROM_IO7) != lastbit) delayMicroseconds(1);
}

void printContents() {
    for (int addr = 0; addr < 256; addr += 16) {
        byte data[16];
        for (int offset = 0; offset < 16; offset++) {
            data[offset] = readEeprom(addr+offset);
        }

        char buf[80];
        sprintf(buf,
                "%03x:  %02x %02x %02x %02x %02x %02x %02x %02x   "
                "%02x %02x %02x %02x %02x %02x %02x %02x",
                addr,
                data[0], data[1], data[2], data[3],
                data[4], data[5], data[6], data[7],
                data[8], data[9], data[10], data[11],
                data[12], data[13], data[14], data[15]);
        Serial.println(buf);
    }
}

// 4-bit hex decoder for common anode 7-segment display
byte data[] = { 0x81, 0xcf, 0x92, 0x86, 0xcc, 0xa4, 0xa0, 0x8f, 0x80, 0x84, 0x88, 0xe0, 0xb1, 0xc2, 0xb0, 0xb8 };

void setup()
{
    pinMode(SER, OUTPUT);
    pinMode(SRCLK, OUTPUT);
    pinMode(RCLK, OUTPUT);
    digitalWrite(EEPROM_WE, HIGH);
    pinMode(EEPROM_WE, OUTPUT);

    Serial.begin(57600);

    Serial.println("Erasing EEPROM...");
    for (int addr = 0; addr < 2048; addr++) {
        writeEeprom(addr, 0xff);
        if (addr % 64 == 0) {
            Serial.print(".");
        }
    }
    Serial.println("");

    Serial.println("Writing EEPROM...");
    for (int addr = 0; addr < sizeof(data); addr++) {
        writeEeprom(addr, data[addr]);
    }

    Serial.println("Reading EEPROM...");

    printContents();
}

void loop()
{
}
