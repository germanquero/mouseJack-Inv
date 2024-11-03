#include "Arduino.h"
#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "printf.h"

#define CE 5
#define CSN 6
#define PKT_SIZE 37
#define PAY_SIZE 32

RF24 radio(CE, CSN);

long time;
int ledpin = 13;
uint64_t promisc_addr = 0xAALL;
uint8_t channel = 25;
uint64_t address;
uint8_t payload[PAY_SIZE];
uint8_t payload_size;

void print_payload_details(){
  Serial.print(">>> >>> CHANNEL: ");
  Serial.print(channel);
  Serial.print("   ///   PAYLOAD_SIZE: ");
  Serial.print(payload_size);
  Serial.print("   ///   ADDRESS: ");
  for (int j = 0; j < 5; j++)
  {
    Serial.print((uint8_t)(address >> (8 * j) & 0xff), HEX);
    Serial.print(" ");
  }
  Serial.print("   ///   PAYLOAD: ");
  for (int j = 0; j < payload_size; j++)
  {
    Serial.print(payload[j], HEX);
    Serial.print(" ");
  }
  Serial.println("");
  return;
}

// Update a CRC16-CCITT with 1-8 bits from a given byte
uint16_t crc_update(uint16_t crc, uint8_t byte, uint8_t bits){
  crc = crc ^ (byte << 8);
  while(bits--)
    if((crc & 0x8000) == 0x8000) crc = (crc << 1) ^ 0x1021;
    else crc = crc << 1;
  crc = crc & 0xFFFF;
  return crc;
}

uint8_t writeRegister(uint8_t reg, uint8_t value){
  uint8_t status;

  digitalWrite(CSN, LOW);
  status = SPI.transfer( W_REGISTER | ( REGISTER_MASK & reg ) );
  SPI.transfer(value);
  digitalWrite(CSN, HIGH);
  return status;
}

uint8_t writeRegister(uint8_t reg, const uint8_t* buf, uint8_t len){
  uint8_t status;

  digitalWrite(CSN, LOW);
  status = SPI.transfer( W_REGISTER | ( REGISTER_MASK & reg ) );
  while (len--)
    SPI.transfer(*buf++);
  digitalWrite(CSN, HIGH);

  return status;
}


void scan() {
  Serial.println("- Starting scan...");

  int x, offset;
  uint8_t buf[PKT_SIZE];
  uint16_t wait = 100;
  uint8_t payload_length;
  uint16_t crc, crc_given;

  // do NOT alter this part
  radio.setAutoAck(false);
  writeRegister(RF_SETUP, 0x09); // Disable PA, 2M rate, LNA enabled
  radio.setPayloadSize(32);
  radio.setChannel(channel);
  writeRegister(EN_RXADDR, 0x00);
  writeRegister(SETUP_AW, 0x00); // promiscous address
  radio.openReadingPipe(0, promisc_addr);
  radio.disableCRC();
  radio.startListening();
  
  // debug
  //radio.printDetails();


  while (1) {
    channel++;
    if (channel > 84) {
      Serial.println("--- CHANNEL SWEEP ---");
      digitalWrite(ledpin, HIGH);
      channel = 2;
    }

    if (channel == 4) {
      digitalWrite(ledpin, LOW);
    }

    if (channel == 42) {
      digitalWrite(ledpin, HIGH);
    }

    if (channel == 44) {
      digitalWrite(ledpin, LOW);
    }

    Serial.print("---> tuning to: ");
    Serial.println(channel);
    radio.setChannel(channel);

    time = millis();
    while (millis() - time < wait)
    {
      if (radio.available())
      {
        radio.read(&buf, sizeof(buf));

		// if CRC check fails shift by one bit
        for (offset = 0; offset < 2; offset++) {
          if (offset == 1) {
            for (x = 31; x >= 0; x--) {
              if (x > 0) buf[x] = buf[x - 1] << 7 | buf[x] >> 1;
              else buf[x] = buf[x] >> 1;
            }
          }

          // Read the payload length
          payload_length = buf[5] >> 2;

		  // check payload legnth < (32 - 9 for header, CRC, part of all adress bytes)
          if (payload_length <= (PAY_SIZE-9))
          {
            // read CRC
            crc_given = (buf[6 + payload_length] << 9) | ((buf[7 + payload_length]) << 1);
            crc_given = (crc_given << 8) | (crc_given >> 8);
            if (buf[8 + payload_length] & 0x80) crc_given |= 0x100;

            // calculate CRC
            crc = 0xFFFF;
            for (x = 0; x < 6 + payload_length; x++) crc = crc_update(crc, buf[x], 8);
            crc = crc_update(crc, buf[6 + payload_length] & 0x80, 1);
            crc = (crc << 8) | (crc >> 8);

            // verify CRC
            if (crc == crc_given) {
              Serial.print("- [!!!] >>> >>> FOUNDED PACKET (valid crc)");

              if (payload_length > 0) {
                Serial.print("   ///   payload length is ");
                Serial.println(payload_length);
                // address
                address = 0;
                for (int i = 0; i < 4; i++)
                {
                  address += buf[i];
                  address <<= 8;
                }
                address += buf[4];

                // payload
                for(x = 0; x < payload_length + 3; x++)
                  payload[x] = ((buf[6 + x] << 1) & 0xFF) | (buf[7 + x] >> 7);
                payload_size = payload_length;

                print_payload_details();
                return;
              } else {
                Serial.println("   ///   payload is empty.");
              }
            }
          }
        }
      }
    }
  }
}



void reset() {
  payload_size = 0;
  for (int i = 0; i < PAY_SIZE; i++) {
    payload[i] = 0;
  }
  radio.begin();
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
  }
  printf_begin();
  pinMode(ledpin, OUTPUT);
  digitalWrite(ledpin, LOW);
}

void loop() {
  Serial.println("** Reseting...");
  reset();
  Serial.println("** Scanning...");
  scan();
}
