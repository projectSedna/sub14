#ifndef _SMC_DRIVER_
#define _SMC_DRIVER_

#include <SoftwareSerial.h>

const unsigned char CRC7_POLY = 0x91;
unsigned char CRCTable[256];

using namespace std;

class smcDriver{
private:
  int _rxPin;
  int _txPin;
  SoftwareSerial smcSerial;
public:
  smcDriver(int rxPin, int txPin);
  unsigned char getCRCForByte(unsigned char val);
  void buildCRCTable();
  unsigned char getCRC(unsigned char message[], unsigned char length);
  void sendCommand(unsigned char message[], unsigned char length);
  void exitSafeStart(uint8_t id);
  void setMotorSpeed(uint8_t id, int speed);
};

#endif 
