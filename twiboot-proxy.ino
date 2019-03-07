/*
 * twiboot-proxy.ino
 * 
 * This sketch allows a arduino leonardo or a ftDuino to act as
 * a regular arduino bootloader on USB side and as a twiboot host
 * on i2c side. It can be used to flash a Atmega328 based twiboot
 * device from within the Arduino IDE using a Arduino Leonardo as a proxy.
 * The Arduino IDE needs to be setup for a Arduino Uno/Nano or similar.
 *
 * This needs a Leonardo (or other 32u4 based device) as the leonardo
 * itself does not activate its own bootloader when the Arduino IDE tries
 * to access it as a Nano Uno or similar.
 * 
 * UART side: regular STK500 protocol as used by Arduino IDE for the 
 * Nano/Uno/ATmega328P
 * I2C side:  twiboot https://github.com/orempel/twiboot
 * 
 */

#include "stk500.h"

unsigned char cmd, parm;
uint16_t addr, bytes;
uint8_t memtype;
uint8_t state = 0;
uint8_t buffer[128];

#define ADDR  0x29

// We need to use a custom i2c implementation since the
// Wire lib does not cope with i2c transfers of the
// required length. Splitting long transfers into multiple
// short ones is not possible with the way twiboot works.

/* ----------------------------------------- */
void i2c_send_byte(uint8_t data) {
  TWDR = data;
  TWCR = _BV(TWINT)  |  _BV(TWEN);
  while( !(TWCR & _BV(TWINT)));
}

void i2c_start() {
  TWCR = _BV(TWINT) | _BV(TWSTA)  | _BV(TWEN);  // write
  while( !(TWCR & _BV(TWINT)));
  i2c_send_byte(ADDR<<1);
}

void i2c_start_r() {
  TWCR = _BV(TWINT) | _BV(TWSTA)  | _BV(TWEN);  // write
  while( !(TWCR & _BV(TWINT)));
  i2c_send_byte(ADDR<<1 | 1);
}

unsigned char i2c_readAck(void) {
  TWCR = (1<<TWINT) | (1<<TWEN) | (1<<TWEA);
  while(!(TWCR & (1<<TWINT)));    

  return TWDR;
}/* i2c_readAck */

unsigned char i2c_readNak(void) {
  TWCR = (1<<TWINT) | (1<<TWEN);
  while(!(TWCR & (1<<TWINT)));
  
    return TWDR;

}/* i2c_readNak */

void i2c_stop(void) {
  TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWSTO);
  while( (TWCR & _BV(TWSTO)));
}

void setup() {
  Serial.begin(115200);
  while(!Serial);

  TWSR = 0;
  TWBR = F_CPU/(2*100000)-8;

  state = 0;  // expecting cmd code
}

void i2c_read_version(char *data) {
#if 1
  i2c_start();
  i2c_send_byte(0x01);
  i2c_start_r();
  for(uint8_t i=0;i<15;i++)
    *data++ = i2c_readAck();
    
  *data++ = i2c_readNak();
  i2c_stop(); 
  *data++ = 0;
#else
  Wire.beginTransmission(ADDR);
  Wire.write(0x01);
  Wire.endTransmission(false);
  Wire.requestFrom(ADDR, 16);
  for(uint8_t i=0;i<16;i++)
    *data++ = Wire.read();
  *data++ = 0;
#endif
}

void i2c_read_signature(uint8_t *data) {
#if 1
  i2c_start();
  i2c_send_byte(0x02);
  i2c_send_byte(0x00);
  i2c_send_byte(0x00);
  i2c_send_byte(0x00);
  i2c_start_r();
  for(uint8_t i=0;i<3;i++)
    *data++ = i2c_readAck();
    
  *data++ = i2c_readNak();
  i2c_stop(); 
#else
  Wire.beginTransmission(ADDR);
  Wire.write(0x02);
  Wire.write(0x00);
  Wire.write(0x00);
  Wire.write(0x00);
  Wire.endTransmission(false);
  Wire.requestFrom(ADDR, 4);
  for(uint8_t i=0;i<4;i++)
    *data++ = Wire.read();
#endif
}

void i2c_write_block(uint16_t a, uint8_t *data) {
#if 1
  i2c_start();
  i2c_send_byte(0x02);
  i2c_send_byte(0x01);
  i2c_send_byte(a>>8);
  i2c_send_byte(a&0xff);
  for(uint8_t i=0;i<128;i++)
    i2c_send_byte(*data++);
  i2c_stop(); 
#else
  Wire.beginTransmission(ADDR);
  Wire.write((uint8_t)0x02);
  Wire.write((uint8_t)0x01);
  Wire.write((uint8_t)(a>>8));
  Wire.write((uint8_t)(a&0xff));
  for(uint8_t i=0;i<128;i++)
      Wire.write(data[i]);
  Wire.endTransmission();
#endif
}

void i2c_read_block(uint16_t a, uint8_t *data) {
#if 1
  i2c_start();
  i2c_send_byte(0x02);
  i2c_send_byte(0x01);
  i2c_send_byte(a>>8);
  i2c_send_byte(a&0xff);
  i2c_start_r();
  for(uint8_t i=0;i<127;i++)
    *data++ = i2c_readAck();
    
  *data++ = i2c_readNak();
  i2c_stop(); 
#else
  Wire.beginTransmission(ADDR);
  Wire.write((uint8_t)0x02);
  Wire.write((uint8_t)0x01);
  Wire.write((uint8_t)(a>>8));
  Wire.write((uint8_t)(a&0xff));
  Wire.endTransmission(false);
  Wire.requestFrom(ADDR, 128);
  for(uint8_t i=0;i<128;i++)
    *data++ = Wire.read();
#endif
}

void i2c_run(void) {
  i2c_start();
  i2c_send_byte(0x01);
  i2c_send_byte(0x80);
  i2c_stop();
}

void loop() {
  if(Serial.available()) {
   unsigned char chr = Serial.read();

    if(state == 0) {
      cmd = chr;
      state = 1;
    } else {
      // most requests are just ack'd
      if((cmd == STK_GET_SYNC) ||
         (cmd == STK_SET_DEVICE) ||
         (cmd == STK_SET_DEVICE_EXT) ||
         (cmd == STK_ENTER_PROGMODE) ||
         (cmd == STK_LEAVE_PROGMODE)) {
        if(chr == CRC_EOP) {
          Serial.write(STK_INSYNC);
          Serial.write(STK_OK);
          Serial.flush();
          state = 0;
        }        
      }

      // GET PARAMETER
      else if(cmd == STK_GET_PARAMETER) {
        if(state == 1) {
          parm = chr;
          state = 2;
        } else if(chr == CRC_EOP) {
          Serial.write(STK_INSYNC);
          switch(parm) {
            case STK_SW_MAJOR:
            case STK_SW_MINOR:
              Serial.write(4);
              break;
            default:
              Serial.write(3); // gives lots of weird numbers but optiboot does it this way
              break;
          }         
          Serial.write(STK_OK);
          Serial.flush();
          state = 0;
        } else
          state = 0;
      } 

      // read signature
      else if(cmd == STK_READ_SIGN) {
        if(chr == CRC_EOP) {
          Serial.write(STK_INSYNC);
          // obtain signature via i2c
          i2c_read_signature(buffer);
          Serial.write(buffer[0]);
          Serial.write(buffer[1]);
          Serial.write(buffer[2]);
          
          Serial.write(STK_OK);
          Serial.flush();
          state = 0;
        }
      }

      else if(cmd == STK_UNIVERSAL) {
        if(chr == CRC_EOP) {        
          Serial.write(STK_INSYNC);
          Serial.write((uint8_t)0x00);
          Serial.write(STK_OK);
          Serial.flush();
          state = 0;
        }
      }
      
      else if(cmd == STK_LOAD_ADDRESS) {
        if((state == 1) || (state == 2)) {
          addr = (addr >> 8) | (chr << 8);
          state++;
        } else {
          if(chr == CRC_EOP) {        
            Serial.write(STK_INSYNC);
            Serial.write(STK_OK);
            Serial.flush();
            state = 0;         
          }
        }
      }
      
      else if(cmd == STK_PROG_PAGE) {
        if((state == 1) || (state == 2)) {
          bytes = (bytes << 8) | chr;
          state++;
        } else if(state == 3) {
          memtype = chr;
          state++;
        } else if((state >= 4) && (state < 4+bytes)) {         
          if(state-4 < sizeof(buffer))
            buffer[state-4] = chr;
          state++;
        } else {
          if(chr == CRC_EOP) {
            i2c_write_block(addr<<1, buffer);
            Serial.write(STK_INSYNC);
            Serial.write(STK_OK);
            Serial.flush();
            state = 0;         
          }
        }  
      } 
           
      else if(cmd == STK_READ_PAGE) {
        if((state == 1) || (state == 2)) {
          bytes = (bytes << 8) | chr;
          state++;
        } else if(state == 3) {
          memtype = chr;
          state++;
        } else {
          if(chr == CRC_EOP) {        
            i2c_read_block(addr<<1, buffer);
            
            Serial.write(STK_INSYNC);
            // send dummy bytes
            for(uint8_t i=0;i<bytes;i++)
              Serial.write(buffer[i]);
            
            Serial.write(STK_OK);
            Serial.flush();


            state = 0;         
          }
        }  
      }      
                 
      else {
        state = 0;
      }
    }
  }
}
