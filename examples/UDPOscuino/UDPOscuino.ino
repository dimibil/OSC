
// UDP OSCuino
// system, analog and digital pin control and monitoring for Arduino
// Yotam Mann and Adrian Freed


#include <Ethernet.h>
#include <EthernetUdp.h>
#include <SPI.h>    
#include <OSCBundle.h>

EthernetUDP Udp;

//the Arduino's IP
IPAddress ip(128, 32, 122, 252);

//port numbers
const unsigned int inPort = 8888;
const unsigned int outPort = 9999;

//everything on the network needs a unique MAC 
#if defined(__MK20DX128__)
// Teensy 3 has MAC burned in
static byte mac[6];
void read(uint8_t word, uint8_t *mac, uint8_t offset) {
  FTFL_FCCOB0 = 0x41;             // Selects the READONCE command
  FTFL_FCCOB1 = word;             // read the given word of read once area

  // launch command and wait until complete
  FTFL_FSTAT = FTFL_FSTAT_CCIF;
  while(!(FTFL_FSTAT & FTFL_FSTAT_CCIF));

  *(mac+offset) =   FTFL_FCCOB5;       // collect only the top three bytes,
  *(mac+offset+1) = FTFL_FCCOB6;       // in the right orientation (big endian).
  *(mac+offset+2) = FTFL_FCCOB7;       // Skip FTFL_FCCOB4 as it's always 0.
}
void read_mac() {
  read(0xe,mac,0);
  read(0xf,mac,3);
}
#else
void read_mac() {}
 byte mac[] = {  
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED }; // you can find this written on the board of some Arduino Ethernets or shields
#endif


//outgoing messages

OSCBundle bundleOUT;

//converts the pin to an osc address
const char * numToOSCAddress( int pin){
  static char s[10] ="/";
   
   itoa(pin, s+1, 10);
   return s;
}

/**
 * ROUTES
 * 
 * these are where the routing functions go
 * 
 */

/**
 * DIGITAL
 * 
 * called when address matched "/d"
 * expected format:
 * /d/(pin)
 *   /u = digitalRead with pullup
 *   (no value) = digitalRead without pullup
 *   (value) = digital write on that pin
 * 
 */

void routeDigital(OSCMessage &msg, int addrOffset ){
  //match input or output
  for(byte pin = 0; pin < NUM_DIGITAL_PINS; pin++){
    //match against the pin number strings
    int pinMatched = msg.match(numToOSCAddress(pin), addrOffset);
    if(pinMatched){
      //if it has an int, then it's a digital write
      if (msg.isInt(0)){
        pinMode(pin, OUTPUT);
        digitalWrite(pin, (msg.getInt(0)>0) ? HIGH:LOW);
      } //otherwise it's an digital read
      //with a pullup?
      else if (msg.fullMatch("/u", pinMatched+addrOffset)){
        //set the pullup
        pinMode(pin, INPUT_PULLUP);
        //setup the output address which should be /d/(pin)/u
        char outputAddress[9];
        strcpy(outputAddress, "/d");
        strcat(outputAddress, numToOSCAddress(pin));
        strcat(outputAddress,"/u");
        //do the digital read and send the results
        bundleOUT.add(outputAddress).add(digitalRead(pin));       
      } //else without a pullup   
      else {
        //set the pinmode
        pinMode(pin, INPUT);
        //setup the output address which should be /d/(pin)
        char outputAddress[6];
        strcpy(outputAddress, "/d");
        strcat(outputAddress, numToOSCAddress(pin));
        //do the digital read and send the results
        bundleOUT.add(outputAddress).add(digitalRead(pin));         
      }
    }
  }
}

/**
 * ANALOG
 * 
 * called when the address matches "/a"
 * 
 * format:
 * /a/(pin)
 *   /u = analogRead with pullup
 *   (no value) = analogRead without pullup
 *   (digital value) = digital write on that pin
 *    (float value) = analogWrite on that pin
 * 
 **/

void routeAnalog(OSCMessage &msg, int addrOffset ){
  //iterate through all the analog pins
  for(byte pin = 0; pin < NUM_ANALOG_INPUTS; pin++){
    //match against the pin number strings
    int pinMatched = msg.match(numToOSCAddress(pin), addrOffset);
    if(pinMatched){
      //if it has an int, then it's a digital write
      if (msg.isInt(0)){
        pinMode(analogInputToDigitalPin(pin), OUTPUT);
        digitalWrite(analogInputToDigitalPin(pin), (msg.getInt(0) > 0)? HIGH: LOW);
      } //otherwise it's an analog read
      else if(msg.isFloat(0)){
        analogWrite(pin, (int)(msg.getFloat(0)*255.0f));
      }
      //with a pullup?
      else if (msg.fullMatch("/u", pinMatched+addrOffset)){
        //set the pullup

        pinMode(analogInputToDigitalPin(pin), INPUT_PULLUP);

        //setup the output address which should be /a/(pin)/u
        char outputAddress[9];
        strcpy(outputAddress, "/a");
        strcat(outputAddress, numToOSCAddress(pin));
        strcat(outputAddress,"/u");
        //do the analog read and send the results
        bundleOUT.add(outputAddress).add(analogRead(pin));       
      } //else without a pullup   
      else {
        //set the pinmode
        // This fails on Arduino 1.04 on Leanardo, I added this to fix it: #define analogInputToDigitalPin(p)  (p+18)

        pinMode(analogInputToDigitalPin(pin), INPUT);
        //setup the output address which should be /a/(pin)
        char outputAddress[6];
        strcpy(outputAddress, "/a");
        strcat(outputAddress, numToOSCAddress(pin));
        //do the analog read and send the results
        bundleOUT.add(outputAddress).add(analogRead(pin));         
      }
    }
  }
}


/**
 * TONE
 * 
 * square wave output "/tone"
 * 
 * format:
 * /tone/pin
 *   
 *   (digital value) (float value) = freqency in Hz
 *   (no value) disable tone
 * 
 **/

void routeTone(OSCMessage &msg, int addrOffset ){
  //iterate through all the analog pins
  for(byte pin = 0; pin < NUM_DIGITAL_PINS; pin++){
    //match against the pin number strings
    int pinMatched = msg.match(numToOSCAddress(pin), addrOffset);
    if(pinMatched){
      unsigned int frequency = 0;
      //if it has an int, then it's an integers frequency in Hz
      if (msg.isInt(0)){        
        frequency = msg.getInt(0);
      } //otherwise it's a floating point frequency in Hz
      else if(msg.isFloat(0)){
        frequency = msg.getFloat(0);
      }
      else
        noTone(pin);
      if(frequency>0)
      {
         if(msg.isInt(1))
           tone(pin, frequency, msg.getInt(1));
         else
           tone(pin, frequency);
      }
    }
  }
}
#if defined (__MK20DX128__)
#define TOUCHSUPPORT
#endif

#ifdef TOUCHSUPPORT
#define NTPINS 12
const int cpins[NTPINS] = {22,23,19,18,17,16,15,0,1,25,32, 33 }; 
void routeTouch(OSCMessage &msg, int addrOffset )
{
  for(int i=0;i<NTPINS;++i)
   {
const char *name = numToOSCAddress(cpins[i]);
     int pinMatched = msg.match(name, addrOffset);
    if(pinMatched)
    {
       char outputAddress[9];
        strcpy(outputAddress, "/c");
        strcat(outputAddress, name);
        bundleOUT.add(outputAddress).add(touchRead(cpins[i]));

    }
   }
}
#endif

#if !defined(__AVR_ATmega8__) && !defined(__MK20DX128__)

// power supply measurement on some Arduinos 
float getSupplyVoltage(){
    // powersupply
    int result;
    // Read 1.1V reference against AVcc
#if defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
    ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  #elif defined (__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__)
    ADMUX = _BV(MUX5) | _BV(MUX0);
  #elif defined (__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__)
    ADMUX = _BV(MUX3) | _BV(MUX2);
  #elif  defined(__AVR_AT90USB646__) || defined(__AVR_AT90USB1286__)    || defined(__AVR_ATmega1280__) 
    ADMUX = 0x40| _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1) ;
    ADCSRB =  0;
  #else
    ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  #endif  
    delayMicroseconds(300); // wait for Vref to settle
    ADCSRA |= _BV(ADSC); // Convert
    while (bit_is_set(ADCSRA,ADSC));
	result = ADCL;
    result |= ADCH<<8;
      
    float supplyvoltage = 1.1264f *1023 / result;
    return supplyvoltage;	
}

// temperature
float getTemperature(){	
	int result;
#if defined(__AVR_ATmega32U4__) ||    (!defined(__AVR_ATmega1280__) && !defined(__AVR_ATmega8__) && !defined(__AVR_AT90USB646__) && !defined(__AVR_AT90USB1286__) &&! defined(__AVR_ATmega2560__))
	
#if defined(__AVR_ATmega32U4__)
	ADMUX =  _BV(REFS1) | _BV(REFS0) | _BV(MUX2) | _BV(MUX1) | _BV(MUX0);
	ADCSRB =  _BV(MUX5);
#else
	ADMUX = _BV(REFS1) | _BV(REFS0) | _BV(MUX3);
#endif	
	delayMicroseconds(200); // wait for Vref to settle
	ADCSRA |= _BV(ADSC); // Convert
	while (bit_is_set(ADCSRA,ADSC));
	result = ADCL;
	result |= ADCH<<8;

	analogReference(DEFAULT);
	
	return  result/1023.0f;
#else
      return 0.0f;
#endif	
}
#endif  

#if defined(__MK20DX128__)
float getTemperature()
{
        analogReference(INTERNAL);
        delay(1);
    int val = analogRead(38); // seems to be flakey
  analogReference(DEFAULT);

  return val; //need to compute something here to get to degrees C
}
float getSupplyVoltage()
{
  int val = analogRead(39); 
  return val>0? (1.20f*1023/val):0.0f; 
}

#endif
/**
 * SYSTEM MESSAGES
 * 
 * expected format:
 * /s
 *   /m = microseconds
 *   /d = number of digital pins
 *   /a = number of analog pins
 *  /l integer = set the led
 *  /t = temperature
 *  /s = power supply voltage
 */
// 
void routeSystem(OSCMessage &msg, int addrOffset ){
  if (msg.fullMatch("/t", addrOffset)){
    bundleOUT.add("/s/t").add(getTemperature());
  }
  if (msg.fullMatch("/s", addrOffset)){
    bundleOUT.add("/s/s").add(getSupplyVoltage());
  }
  if (msg.fullMatch("/m", addrOffset)){
    bundleOUT.add("/s/m").add((int32_t)micros());
  }
  if (msg.fullMatch("/d", addrOffset)){
    bundleOUT.add("/s/d").add(NUM_DIGITAL_PINS);
  }
  if (msg.fullMatch("/a", addrOffset)){
    bundleOUT.add("/s/a").add(NUM_ANALOG_INPUTS);
  }
#ifdef ENABLE_LED
// this is disabled because many ethernet boards use the
// LED pin for ethernet pin CS
  if (msg.fullMatch("/l", addrOffset)){

    if (msg.isInt(0)){
             pinMode(LED_BUILTIN, OUTPUT);
      int i = msg.getInt(0);
        pinMode(LED_BUILTIN, OUTPUT);
        digitalWrite(LED_BUILTIN, (i > 0)? HIGH: LOW);
        bundleOUT.add("/s/l").add(i);
      }
  }
#endif
}

/**
 * MAIN METHODS
 * 
 * setup and loop, bundle receiving/sending, initial routing
 */
void setup() {
  //setup ethernet port
  read_mac();
  Ethernet.begin(mac,ip);
  Udp.begin(inPort);

}

//reads and routes the incoming messages
void loop(){ 
    OSCBundle bundleIN;
   int size;
 
   if( (size = Udp.parsePacket())>0)
   {
//       unsigned int outPort = Udp.remotePort();

     while(size--)
       bundleIN.fill(Udp.read());

    if(!bundleIN.hasError())
     {
        bundleIN.route("/s", routeSystem);
        bundleIN.route("/a", routeAnalog);
        bundleIN.route("/d", routeDigital);
        bundleIN.route("/tone", routeTone);
#ifdef TOUCHSUPPORT
        bundleIN.route("/c", routeTouch);
#endif

    }
    // send the response bundle back to where the request came from
    Udp.beginPacket(Udp.remoteIP(),outPort); 
    bundleOUT.send(Udp);
    Udp.endPacket();
    bundleOUT.empty(); // empty the bundle ready to use for new messages
   }
}








