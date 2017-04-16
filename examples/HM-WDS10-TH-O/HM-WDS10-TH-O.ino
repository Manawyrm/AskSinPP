//- -----------------------------------------------------------------------------------------------------------------------
// AskSin++
// 2016-10-31 papa Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//- -----------------------------------------------------------------------------------------------------------------------

/*
 * Setup defines to configure the library.
 */
// #define USE_AES
// #define HM_DEF_KEY 0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,0x10
// #define HM_DEF_KEY_INDEX 0

#include <EnableInterrupt.h>
#include <AskSinPP.h>
#include <TimerOne.h>
#include <LowPower.h>

#include <MultiChannelDevice.h>

// define this to read the device id, serial and device type from bootloader section
// #define USE_OTA_BOOTLOADER

#ifdef USE_OTA_BOOTLOADER
  #define OTA_MODEL_START  0x7ff0 // start address of 2 byte model id in bootloader
  #define OTA_SERIAL_START 0x7ff2 // start address of 10 byte serial number in bootloader
  #define OTA_HMID_START   0x7ffc // start address of 3 byte device id in bootloader
#else
  // device ID
  #define DEVICE_ID HMID(0x34,0x56,0x78)
  // serial number
  #define DEVICE_SERIAL "papa111111"
#endif

// we use a Pro Mini
// Arduino pin for the LED
// D4 == PIN 4 on Pro Mini
#define LED_PIN 4
// Arduino pin for the config button
// B0 == PIN 8 on Pro Mini
#define CONFIG_BUTTON_PIN 8


// number of available peers per channel
#define PEERS_PER_CHANNEL 6


// all library classes are placed in the namespace 'as'
using namespace as;

/**
 * Configure the used hardware
 */
typedef AvrSPI<10,11,12,13> RadioSPI;
typedef AskSin<StatusLed<4>,BatterySensor<22,19>,Radio<RadioSPI,2> > BaseHal;
class Hal : public BaseHal {
public:
  void init () {
    BaseHal::init();
    // set low voltage to 2.2V
    // measure battery every 1h
    battery.init(seconds2ticks(60UL*60),sysclock);
  }
} hal;

class WeatherEventMsg : public Message {
public:
  void init(uint8_t msgcnt,int16_t temp,uint8_t humidity, bool batlow) {
    uint8_t t1 = (temp >> 8) & 0x7f;
    uint8_t t2 = temp & 0xff;
    if( batlow == true ) {
      t1 |= 0x80; // set bat low bit
    }
    Message::init(0xc,msgcnt,0x70,Message::BIDI,t1,t2);
    pload[0] = humidity;
  }
};

class WeatherChannel : public Channel<Hal,List1,EmptyList,List4,PEERS_PER_CHANNEL>, public Alarm {

  WeatherEventMsg msg;
  uint8_t         msgcnt;
  int16_t         temp;
  uint8_t         humidity;

public:
  WeatherChannel () : Channel(), Alarm(5), msgcnt(0), temp(0), humidity(50) {}
  virtual ~WeatherChannel () {}

  virtual void trigger (AlarmClock& clock) {
    // reactivate for next measure
    tick = delay();
    clock.add(*this);
    DPRINT("Measure...\n");
    measure();

    msg.init(msgcnt,temp,humidity,false);
    device().sendPeerEvent(msg,*this);
  }

  // here we do the measurement
  void measure () {
    static int16_t tdx = -7;
    static int8_t  hdx = 1;
    temp += tdx;
    humidity += hdx;
    if( temp >= 40*10 || temp <= -15*10 ) tdx = -tdx;
    if( humidity == 99 || humidity == 5) hdx = -hdx;
  }

  // here we calc when to send next value
  uint32_t delay () {
    // for testing we use delay of 5sec
    return seconds2ticks(5);
  }

  void setup(Device<Hal>* dev,uint8_t number,uint16_t addr) {
    Channel::setup(dev,number,addr);
    sysclock.add(*this);
  }

  uint8_t status () const {
    return 0;
  }

  uint8_t flags () const {
    return 0;
  }

};


typedef MultiChannelDevice<Hal,WeatherChannel,1> WeatherType;
WeatherType sdev(0x20);

ConfigButton<WeatherType> cfgBtn(sdev);

void setup () {
  DINIT(57600,ASKSIN_PLUS_PLUS_IDENTIFIER);

  if( storage.setup(sdev.checksum()) == true ) {
    sdev.firstinit();
  }

  buttonISR(cfgBtn,CONFIG_BUTTON_PIN);

#ifdef USE_OTA_BOOTLOADER
  sdev.init(hal,OTA_HMID_START,OTA_SERIAL_START);
  sdev.setModel(OTA_MODEL_START);
#else
  sdev.init(hal,DEVICE_ID,DEVICE_SERIAL);
  sdev.setModel(0x00,0x3d);
#endif
  sdev.setFirmwareVersion(0x10);
  sdev.setSubType(DeviceType::THSensor);
  sdev.setInfo(0x03,0x01,0x00);

  hal.init();
}

void loop() {
  bool worked = hal.runready();
  bool poll = sdev.pollRadio();
  if( worked == false && poll == false ) {
    hal.activity.savePower<Sleep<>>(hal);
  }
}
