// Electric Treasure Chest 1
// (c)2021 Fundylab K.Oshima
//  v1.2

#include <EEPROM.h>
#include "sounds.h"

// system settings
#define PORT_SPEAKER    3     
#define PORT_COIL       8
#define PORT_LED        9
#define PORT_SW1        2
#define PORT_SW2        4
#define PORT_SW3        5
#define PORT_SW4        6
#define PORT_SWE        7
#define AWMAX           255   // analog write max value
#define LED_RISE        (true)
#define LED_FALL        (false)
#define LED_OFF         0
#define LED_ON          1
#define SWSTATE_NOPUSH  0
#define SWSTATE_PUSH    1
#define TIMCHATTERING   50  // time to avoid chattering input 50[ms]
#define SOUND_MAXRAWLEN (3*8000)  // Maximum sound length 3[sec]
// open and flash
#define NOOAF           5    // Number Of Open And Flash
// flash time
#define TIMLEDINIT      ((int)(3.92 * 500))  // rise time of led 500[ms]
#define TIMLEDRISE      ((int)(3.92 * 250))  // rise time of led 250[ms]
#define TIMLEDFALL      ((int)(3.92 * 100))  // rise time of led 100[ms]
// password length
#define PASWDMAX        (4+1)   // Maximum number of digits in password and Enter digit

////////////////////// Variable init

int data_swin[PASWDMAX];    // sw data pressed in order
enum esound_s {SOUND_INIT, SOUND_CLICK, SOUND_OK, SOUND_NG, SOUND_CHANGE, ESOUNDMAX} ESOUNDNUM;
enum eswnum {NOON, SW1, SW2, SW3, SW4, SWE, ESWNUMMAX} ESWNUM;
enum eswnum pwd[PASWDMAX];

////////////////////// System

void read_password_fromEE(){
  for(int i = 0; i < PASWDMAX; i++){
    // read password to pwd[] from EE
    pwd[i] = EEPROM.read( i );
  }
}

void write_password_toEE(){
  for(int i = 0; i < PASWDMAX; i++){
    // write password to EE from pwd[]
    EEPROM.write( i, pwd[i]);
  }
}

void test_write_paswd(){
  pwd[0] = SW4;
  pwd[1] = SW1;
  pwd[2] = SW2;
  pwd[3] = SW3;
  pwd[4] = SWE;
  write_password_toEE();
}

enum eswnum check_swstate(){
  if(digitalRead(PORT_SW1) == HIGH) return SW1;
  else if(digitalRead(PORT_SW2) == HIGH) return SW2;
  else if(digitalRead(PORT_SW3) == HIGH) return SW3;
  else if(digitalRead(PORT_SW4) == HIGH) return SW4;
  else if(digitalRead(PORT_SWE) == HIGH) return SWE;
  else return NOON;
}

// p_nop: set the state to continue waiting
//        SWSTATE_PUSH 1: waiting during pushed,
//        SWSTATE_NOPUSH 0: waiting during nopushed
enum eswnum wait_changesw(bool p_nop){
  enum eswnum swstate;
  swstate = check_swstate();
  while((swstate == NOON) ^ p_nop) swstate = check_swstate();
  delay(TIMCHATTERING);
  return swstate;
}

////////////////////// Outputs

void drive_coil(){
  digitalWrite(PORT_COIL, HIGH);
}
void off_coil(){
  digitalWrite(PORT_COIL, LOW);
}

void play(enum esound_s esoundnum) {
  OCR2B = 0x80;
  int i = 0;
  while(i < SOUND_MAXRAWLEN && OCR2B != 0xff){
    switch (esoundnum){
      case SOUND_INIT: OCR2B = pgm_read_byte_near(&sound_init[i]); break;
      case SOUND_CLICK: OCR2B = pgm_read_byte_near(&sound_click[i]); break;
      case SOUND_OK: OCR2B = pgm_read_byte_near(&sound_ok[i]); break;
      case SOUND_NG: OCR2B = pgm_read_byte_near(&sound_ng[i]); break;
      case SOUND_CHANGE: OCR2B = pgm_read_byte_near(&sound_change[i]); break;
    }
    delayMicroseconds(125); // (62);
    i++;
  }
}

void flash_led(bool risefall, int cyclewidth){
  Serial.println(cyclewidth);//test
  for(int i = 0; i < AWMAX; i++){
    int t;
    if(risefall) t = i;
    else t = AWMAX - i;
    analogWrite(PORT_LED, t);
    delayMicroseconds(cyclewidth);
  }
  if(risefall) digitalWrite(PORT_LED, LED_ON);
  else digitalWrite(PORT_LED, LED_OFF);
}

////////////////////// Output design

void open_and_flash(){
  for(int i = 0; i < NOOAF; i++){
    drive_coil();
    flash_led(LED_RISE, TIMLEDRISE);
    off_coil();
    flash_led(LED_FALL, TIMLEDFALL);
  }
}

void play_and_flash(enum esound_s soundtype){
    flash_led(LED_RISE, TIMLEDINIT);
    play(soundtype);
    flash_led(LED_FALL, TIMLEDINIT);
}  

////////////////////// Validate functions

bool validate_paswdin(){
  int pos = 0;
  enum eswnum swstate;
  bool retval = true;   // password judgment flug
  // wait loop to avoid detecting long push
  wait_changesw(SWSTATE_PUSH);
  // loop until get the ON_SWE or over the PASWDMAX times
  while(pos < PASWDMAX){
    Serial.println("swwait to validate");
    // wait key input
    swstate = wait_changesw(SWSTATE_NOPUSH);
    // compare the entered value with the password
    Serial.print(swstate);Serial.println(" pushed");
    if(swstate != pwd[pos]) retval = false;
    if(swstate == SWE) break;
    // post processing
    play(SOUND_CLICK);
    pos++;
    // wait loop to avoid detecting long push
    wait_changesw(SWSTATE_PUSH);
  }
  Serial.print("Judgment: ");Serial.println(retval);
  return retval;
}

bool enter_newpaswd(){
  int pos = 0;
  enum eswnum tmp[PASWDMAX];
  bool retval = false;    // password judgment flug
  // wait loop to avoid detecting long push
  wait_changesw(SWSTATE_PUSH);
  // loop until get the ON_SWE or over the PASWDMAX times
  while(pos < PASWDMAX){
    Serial.println("swwait to enter new password");
    // wait key input and store to the tmp register
    tmp[pos] = wait_changesw(SWSTATE_NOPUSH);
    Serial.print(tmp[pos]);Serial.println(" pushed");
    if(tmp[pos] == SWE){
      // password ok and store to the pwd[]
      for(int i = 0; i <= pos; i++) pwd[i] = tmp[i];
      retval = true;
      break;
    }
    // post processing
    play(SOUND_CLICK);
    pos++;
    // wait loop to avoid detecting long push
    wait_changesw(SWSTATE_PUSH);
  }
  Serial.print("Judgment: ");Serial.println(retval);
  return retval;
}

bool init_checkbutton(){
  bool retval = false;
  // check sw SWE
  if(digitalRead(PORT_SWE) == HIGH){
    // enter to the change password mode
    play_and_flash(SOUND_CHANGE);
    // wait until released the sw
    while (digitalRead(PORT_SWE) == HIGH){
      delay(100);
    }
    // check old password
    if(validate_paswdin()){
      play_and_flash(SOUND_CHANGE);
      // enter new password
      if(enter_newpaswd()){
        //save password
        write_password_toEE();
        play_and_flash(SOUND_OK);
        retval = true;
      }
      else play(SOUND_NG);
    }
    else play(SOUND_NG);
  }
  return retval;
}  

////////////////////// Setup

void setup()
{
  pinMode(PORT_SW1, INPUT);
  pinMode(PORT_SW2, INPUT);
  pinMode(PORT_SW3, INPUT);
  pinMode(PORT_SW4, INPUT);
  pinMode(PORT_SWE, INPUT);
  pinMode(PORT_LED, OUTPUT);
  pinMode(PORT_COIL, OUTPUT);
  pinMode(PORT_SPEAKER, OUTPUT);
  TCCR2A = _BV(COM2B1) | _BV(WGM21) | _BV(WGM20);
  TCCR2B = _BV(CS20);
  
  Serial.begin(115200);
  Serial.println("***** Started! *****");
  
  // initial process
  //test_write_paswd(); //test
  read_password_fromEE();
  if(!init_checkbutton()){
    play_and_flash(SOUND_INIT);
  }
  Serial.println("Initialize done!");
}

////////////////////// Main

void loop() {
  if(validate_paswdin()){
    // OK action and open the lock
    play(SOUND_OK);
    open_and_flash();
  }
  else{
    // NG action
    play(SOUND_NG);
  }
}
