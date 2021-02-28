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
    for (int addr = 0; addr < 2048; addr += 16) {
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

void eraseEeprom() {
    for (int addr = 0; addr < 2048; addr++) {
        writeEeprom(addr, 0xff);
    }
}

// writes decoder for 7-segment decimal display
void writeDisplay()
{
    // 4-bit hex decoder for common anode 7-segment display
    // byte data[] = {
    //     0x81, 0xcf, 0x92, 0x86, 0xcc, 0xa4, 0xa0, 0x8f,
    //     0x80, 0x84, 0x88, 0xe0, 0xb1, 0xc2, 0xb0, 0xb8
    // };

    // 4-bit hex decoder for common cathode 7-segment display
    byte data[] = {
        0x7e, 0x30, 0x6d, 0x79, 0x33, 0x5b, 0x5f, 0x70,
        0x7f, 0x7b, 0x77, 0x1f, 0x4e, 0x3d, 0x4f, 0x47
    };

    // unsigned output
    for (unsigned addr = 0; addr < 256; addr++) {
        // units
        writeEeprom(addr, data[addr % 10]);
        // tens
        writeEeprom(addr + 0x100, data[(addr/10) % 10]);
        // hundreds
        writeEeprom(addr + 0x200, data[addr/100]);
        // thousands
        writeEeprom(addr + 0x300, 0);
    }

    // signed output
    for (int addr = -128; addr < 128; addr++) {
        // units
        writeEeprom((byte)addr + 0x400, data[abs(addr) % 10]);
        // tens
        writeEeprom((byte)addr + 0x500, data[abs(addr/10) % 10]);
        // hundreds
        writeEeprom((byte)addr + 0x600, data[abs(addr/100)]);
        // thousands (sign)
        writeEeprom((byte)addr + 0x700, addr < 0 ? 1 : 0);
    }
}

// writes microcode
void writeMicrocode()
{
#define BIT(N) ((uint16_t) 1 << N)
#define HLT BIT(15)
#define MI BIT(14)
#define RI BIT(13)
#define RO BIT(12)
#define IO BIT(11)
#define II BIT(10)
#define AI BIT(9)
#define AO BIT(8)
#define EO BIT(7)
#define SU BIT(6)
#define BI BIT(5)
#define OI BIT(4)
#define CE BIT(3)
#define CO BIT(2)
#define JM BIT(1)
#define FI BIT(0)

    uint16_t basic_ucode[16][8] = {
        { CO|MI,   RO|II|CE,  0,     0,     0,           0, 0, 0 }, // 0000 - NOP
        { CO|MI,   RO|II|CE,  IO|MI, RO|AI, 0,           0, 0, 0 }, // 0001 - LDA
        { CO|MI,   RO|II|CE,  IO|MI, RO|BI, EO|AI|FI,    0, 0, 0 }, // 0010 - ADD
        { CO|MI,   RO|II|CE,  IO|MI, RO|BI, EO|AI|SU|FI, 0, 0, 0 }, // 0011 - SUB
        { CO|MI,   RO|II|CE,  IO|MI, AO|RI, 0,           0, 0, 0 }, // 0100 - STA
        { CO|MI,   RO|II|CE,  IO|AI, 0,     0,           0, 0, 0 }, // 0101 - LDI
        { CO|MI,   RO|II|CE,  IO|JM, 0,     0,           0, 0, 0 }, // 0110 - JMP
        { CO|MI,   RO|II|CE,  0,     0,     0,           0, 0, 0 }, // 0111 - JC
        { CO|MI,   RO|II|CE,  0,     0,     0,           0, 0, 0 }, // 1000 - JZ
        { CO|MI,   RO|II|CE,  0,     0,     0,           0, 0, 0 }, // 1001
        { CO|MI,   RO|II|CE,  0,     0,     0,           0, 0, 0 }, // 1010
        { CO|MI,   RO|II|CE,  0,     0,     0,           0, 0, 0 }, // 1011
        { CO|MI,   RO|II|CE,  0,     0,     0,           0, 0, 0 }, // 1100
        { CO|MI,   RO|II|CE,  0,     0,     0,           0, 0, 0 }, // 1101
        { CO|MI,   RO|II|CE,  AO|OI, 0,     0,           0, 0, 0 }, // 1110 - OUT
        { CO|MI,   RO|II|CE,  HLT,   0,     0,           0, 0, 0 }  // 1111 - HLT
    };

    uint16_t ucode[4][16][8];

#define FLAGS_Z0C0 0
#define FLAGS_Z0C1 1
#define FLAGS_Z1C0 2
#define FLAGS_Z1C1 3

#define JC 0b0111
#define JZ 0b1000

    memcpy(ucode[FLAGS_Z0C0], basic_ucode, sizeof(basic_ucode));

    memcpy(ucode[FLAGS_Z0C1], basic_ucode, sizeof(basic_ucode));
    ucode[FLAGS_Z0C1][JC][2] = IO|JM;

    memcpy(ucode[FLAGS_Z1C0], basic_ucode, sizeof(basic_ucode));
    ucode[FLAGS_Z1C0][JZ][2] = IO|JM;

    memcpy(ucode[FLAGS_Z1C1], basic_ucode, sizeof(basic_ucode));
    ucode[FLAGS_Z1C1][JC][2] = IO|JM;
    ucode[FLAGS_Z1C1][JZ][2] = IO|JM;

    for (int addr = 0; addr < 1024; addr++) {
        int flags = (addr >> 8) & 0x3;
        int byte_select = (addr >> 7) & 0x1;
        int opcode = (addr >> 3) & 0xF;
        int step = addr & 0x7;

        if (byte_select) {
            writeEeprom(addr, ucode[flags][opcode][step] >> 8);
        } else {
            writeEeprom(addr, ucode[flags][opcode][step]);
        }
    }
}

void setup()
{
    pinMode(SER, OUTPUT);
    pinMode(SRCLK, OUTPUT);
    pinMode(RCLK, OUTPUT);
    digitalWrite(EEPROM_WE, HIGH);
    pinMode(EEPROM_WE, OUTPUT);

    Serial.begin(57600);

    Serial.println("Writing EEPROM...");

#ifdef DISP

    writeDisplay();

#endif // DISP

#ifdef UCODE

    writeMicrocode();

#endif // UCODE

    Serial.println("Reading EEPROM...");

    printContents();
}

void loop()
{
}
