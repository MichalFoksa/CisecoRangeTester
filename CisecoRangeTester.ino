/*
** Ciseco Range Tester https://github.com/MichalFoksa/CisecoRangeTester 
** 
** Author:  Michal FOKSA
** Contact: source [at] michalfoksa [dot] net
** License: MIT License
**
** Ciseco Range Tester is Arduino device to measure link quality and more importantly possible 
** link length between two Ciseco RF transmitters – local (master) and remote (slave).
** 
** It is reusing built-in feature when remote node, put into a special mode, replies with 
** incoming RSSI level ( more on  
** http://www.openmicros.org/index.php/articles/84-xrf-basics/146-rssimode ).
** 
** Replied RSSI value and average RSSI over multiple measurements is displayed on LCD 
** screen together with kind of a bar where each – (minus character) represents received 
** packet. Screen content is optimized for 16x2 LCD size.
** 
** The difference is that this device is to replace PC, as a master node, so it is more 
** portable. 

** Chosen I2C LCD display is cheap, highly available on Ebay, Deal Extreme, etc. and 
** included in many Arduino starter kits. 
** 
** How to setup remote end (slave):
**   1. Configure on your PC using XCM or AT commands
**   2. Change ATNT (node type) to 4 (ATNT4)
**   3. Write to permanent memory (ATWR) – this is optional step. 
**      If omitted, node returns to previous mode when restarted
**   4. Close the session (ATDN)
**
** Written and tested in Arduino 1.5.8 IDE
**
** TESTED with XinoRF and SainSmart I2C 20x4 LCD Screen. Content is optimized for 16x2 
** screen size. so you should not have a problem using a different screen size, just 
** change DISPLAY_ROWS and DISPLAY_COLUMNS constants.
**
**  CIRCUIT WIRING
**
**  XinoRF   | LCD
**  ----------------
**  +5V      | +5V
**  GND      | GND
**  Analog4  | SDA
**  Analog5  | SCL
*/

#define DISPLAY_ROWS            4
#define DISPLAY_COLUMNS         20

////////////////////////////////////////////////////////////
//     I2C Liquid Crystal LCD Screen stuff
//
// This sketch uses F Malpartida's NewLiquidCrystal library. 
// Obtain from:
// https://bitbucket.org/fmalpartida/new-liquidcrystal 
//
////////////////////////////////////////////////////////////
#include <Wire.h>
#include <LCD.h>
#include <LiquidCrystal_I2C.h>

// Default SainSmart I2C display address is 0x3F. If you are not sure use I2cScanner 
// to find the address of your own display: http://playground.arduino.cc/Main/I2cScanner

#define DISPLAY_I2C_ADDR        0x3F

// Following constants describe PCF8574 I2C IO expander wiring to the LCD display. 
// DO NOT MODIFY unless you know what you are doing!
#define BACKLIGHT_PIN     3
#define En_pin  2
#define Rw_pin  1
#define Rs_pin  0
#define D4_pin  4
#define D5_pin  5
#define D6_pin  6
#define D7_pin  7

LiquidCrystal_I2C lcd(DISPLAY_I2C_ADDR,En_pin,Rw_pin,Rs_pin,D4_pin,D5_pin,D6_pin,D7_pin);
////////////////////////////////////////////////////////////
//     End of I2C Liquid Crystal LCD Screen stuff      
////////////////////////////////////////////////////////////


// Debug output is being written into software serial port on pin 4 (RX) and 5 (TX)
// where I have a common HC-05 Bluetooth module connected:
// http://www.ebay.at/itm/Serial-HC05-Wireless-Bluetooth-Transceiver-HostSlave-integration-CP06011-H24-/271513590642
// 
// Comment label #DEBUG to remove all debugging output
#define DEBUG

////////////////////////////////////////////////////////////
/////////      BLUETOOTH LOGGER                  ///////////
////////////////////////////////////////////////////////////
#ifdef DEBUG
  #include <SoftwareSerial.h>
  #include"StreamLogger.h"
  // Software serial port speed.
  #define SS_SPEED 115200
  SoftwareSerial __btSerial( /*RX*/ 4  , /*TX*/ 5  ) ; 
  
  // StreamLogger is a singleton class, access to its instance must be over a pointer. 
  // More on singletons is on Wikipedia: http://en.wikipedia.org/wiki/Singleton_pattern
  StreamLogger *logger ;
#endif


// Ciseco RF module settings
#define RF_pin               8
#define SERIAL_PORT_SPEED    115200

// Received packet buffer
const int rf_buff_size = 48 ;
char rf_buff[rf_buff_size] ;
char *rf_buff_beg ;
char *rf_buff_end ;
// SRF/XRF RSSI monitor packets are 12 characters long, so ideal read block size is 12
const int rf_read_block_size = 12 ;

// Array to count average RSSI
const int avg_buff_size = 20 ;
int avg_rssi_buff[avg_buff_size] ;
int avg_buff_pos = 0 ;
int cnt = 0 ;
long avg_sum = 0 ;

// Received packet buffer
const int bar_size = 16 ;
char bar[bar_size +1] ;

// Print buffer
char buffer [32] ;

long rssi ; 

// Lost packets counter
int lost = 0 ;

////////////////////////////////////////////////////////////
/////////           S E T U P                    ///////////
////////////////////////////////////////////////////////////

void setup()
{
  for ( int i = 0 ; i < avg_buff_size ; i++ ) {
    avg_rssi_buff[i] = NULL ;               // reset array of last measured rssi values
  }  
  bar[bar_size] = 0 ;                        // string 0 termination character  
  
  rf_buff_beg = rf_buff_end = rf_buff ;
  
  Serial.begin( SERIAL_PORT_SPEED );         // start the serial port at 115200 baud (correct for XinoRF and RFu, if using XRF + Arduino you might need 9600)
  // This check is only needed on the Leonardo:
  while (!Serial) {
    ; // Wait for serial port to connect. Needed for Leonardo only
  }
  Serial.print("Serial initialized "); Serial.println( SERIAL_PORT_SPEED );    
  
  // DEBUG over Bluetooth stuff
#ifdef DEBUG
  logger = StreamLogger::getInstance() ;
  __btSerial.begin( SS_SPEED ) ;  
  logger->setStream ( &__btSerial ) ;
  logger->println ( "Bluetooth initialized" ) ;
#endif  

  lcd.begin (DISPLAY_COLUMNS , DISPLAY_ROWS ) ;    //  Initialize LCD screen
  // Switch on the backlight
  lcd.setBacklightPin(BACKLIGHT_PIN,POSITIVE);
  lcd.setBacklight(HIGH);
  lcd.home (); // go home

  lcd.setCursor (0,0);        
  lcd.print("Starting radio");
  delay( 500 ) ;
  
  pinMode(RF_pin, OUTPUT);              // initialize pin 8 to control the radio    
  digitalWrite(RF_pin, HIGH);           // Enable Radio
  delay( 500 ) ;
  
  Serial.flush(); 
}

////////////////////////////////////////////////////////////
/////////           L O O P                      ///////////
//////////////////////////////////////////////////////////// 


void loop()
{
  Serial.print( "aMMRSSI-----" ) ;
  int found = false ;
  do {
    if ( findPacketBegining ( "aSSRSSIS-" ) == NULL ) {                      // Read expected packet prefix "aSSRSSIS-"
#ifdef DEBUG 
    logger->println() ; 
#endif
      break ;
    } // if found packate prefix
    
    // Read RSSI value
    if ( rf_buff_end - rf_buff_beg < 3 ) {
      if ( readRf ( 3 ) < 3 ) {
        break ;
      }
    }
    // Reset number of lost packets in row counter
    lost = 0 ;
    
    char atol_buff[4] ;                                   // prepare NULL terminated buffer for atol
    atol_buff[3] = 0;
    memcpy(atol_buff , rf_buff_beg , 3 * sizeof(char) ) ;
    rssi = atol( atol_buff ) ;                          // Packet example: aSSRSSIS-047 - RSSI value starts on index 9 of the buffer */
    rf_buff_beg += 3 ;
    found = true ;
    avg_rssi_buff[avg_buff_pos++] = rssi ;

    // Count average RSSI
    cnt = 0 ;
    avg_sum = 0 ;
    for ( int i = 0 ; i <  avg_buff_size ; i++ ) {
      if ( avg_rssi_buff[i] == NULL ) {
        continue ;
      }
      cnt++ ;
      avg_sum += avg_rssi_buff[i] ;    
    } // for
  } while ( false ) ;
  
  // IF not found 
  if ( !found ) {
    lost++ ;
    avg_rssi_buff[avg_buff_pos++] = NULL ;
  } 

#ifdef DEBUG 
  // Print average RSSI array to console
//  for ( int i = 0 ; i <  avg_buff_size ; i++ ) {
//    sprintf( buffer , "%2i, " , avg_rssi_buff[i] ) ;
//    logger->print( buffer ) ;       
//  } // for
//  logger->println() ;
#endif  
 
  // Print RSSI, Average RSSI
  if ( lost == 0 ) {
    if (cnt > (avg_buff_size / 2) ) {    
      sprintf( buffer , "Rssi:%3ld Avg:%3ld" , rssi , avg_sum / cnt ) ;
    } else {
      sprintf( buffer , "Rssi:%3ld        " , rssi ) ;
    }
  } else if ( lost < 6) {
    if (cnt > (avg_buff_size / 2) ) {    
      sprintf( buffer , "Rssi:--- Avg:%3ld" , avg_sum / cnt ) ;
    } else {
      sprintf( buffer , "Rssi:---         " ) ;
    }    
  } else {
    sprintf( buffer , "Sig. lost:%3ds. " , lost / 2 > 999 ? 999 : lost / 2 ) ;
  }
  lcd.setCursor ( 0 , 0 ) ;
  lcd.print( buffer ) ;
  
  // Render received packet bar
  // j is index through packet bar from 0 to its end
  // i is index through average rssi array, it is going from current position backwards
  for ( int j = 0 , i = (avg_buff_pos - 1) ; (j < bar_size) && (i != avg_buff_pos) ; j++ , i-- ) {
    if ( i <= 0 ) {                    // When i points to first average RSSI array element (index zero), change it to the last one
     i = avg_buff_size - 1 ;
    }   
    
    if( avg_rssi_buff[i] == NULL ) {
      bar[j] = ' ' ;
    } else {
      bar[j] = '-' ;
    }
    
  } // for
  
  // Print signal bar
  lcd.setCursor ( 0 , 1 ) ;
  lcd.print( bar ) ;

  if ( avg_buff_pos == avg_buff_size ) {
    avg_buff_pos = 0 ;
  }
  
  delay (100) ;
}


// When the prefix found, function returns a pointer to a character right after 
// the last character of the prefix, otherwise it returns null.
//
// Function reads data in blocks, so it is possible that there are some unprocessed
// data left at the end of rf_buff.
char* findPacketBegining ( const char *prefix ) {
  
  int prefixLen = strlen ( prefix ) ;
  
  //rf_buff_beg = rf_buff_end = rf_buff ;
  
  if ( prefixLen > rf_buff_size ) {
    return NULL ;
  }
  
  // TODO - add some meaningfull exit condition
  for ( int j = 0 ; j < prefixLen * 2 ; j++  ) {
      int available_data_len = rf_buff_end - rf_buff_beg ;
  
    // TODO - Move this to the readRf
    // When lenght of available data in rf_buff in less then prefixLen, try to read additional data from radio
    // It attemps to read data 3 times, then the function returns -2. 
    // Between retry reads it waits 10ms
    for ( int i = 0 ; (i < 3) && (prefixLen > available_data_len) ; i++) {
      available_data_len = readRf ( prefixLen  - available_data_len ) ;
      if ( prefixLen > available_data_len ) {
#ifdef DEBUG 
        logger->println( "Retrying read" ) ;
#endif
        delay ( 10 ) ;
      }
    }
    
    if ( prefixLen > available_data_len ) {
      return NULL ;
    }
    
    if ( strncmp ( prefix , rf_buff_beg , prefixLen ) == 0 ) {
      return rf_buff_beg += prefixLen ;
    }
    rf_buff_beg++ ;
  } // for
  
  return NULL ;
}

void lShiftRfBuff() {

  char *i ;
  for ( i = rf_buff ; rf_buff_beg < rf_buff_end ; i++ , rf_buff_beg++ ){
    *i = *rf_buff_beg ;
  }
  rf_buff_end = i ;  
  rf_buff_beg = rf_buff ;

#ifdef DEBUG 
      logger->println() ;
#endif
  
}

int readRf( int bytes ) {
  
  // Count amount of blocks to read
  int to_read_bytes = bytes / rf_read_block_size ;  
  to_read_bytes += bytes % rf_read_block_size == 0 ? 0 : 1 ;
  // Multiply number of blocks by block size
  to_read_bytes *= rf_read_block_size ;
  
  // Chcek if there is enought space left at the end of buffer
  if ( (rf_buff + rf_buff_size) - rf_buff_end  < to_read_bytes ) {
    // If not shift valid data in buffer to left
    lShiftRfBuff() ;
    
    // Chcek if there is enought space left after the left shift
    if ( (rf_buff + rf_buff_size) - rf_buff_end  < to_read_bytes ) {
      // Out of buffer ;
#ifdef DEBUG 
      sprintf( buffer , "\nBuffer[%i] is too small to read additional %i bytes\n" , rf_buff_size , to_read_bytes ) ;
      logger->print( buffer ) ;
#endif      
      return -1 ; 
    }
  }
  
#ifdef DEBUG
  char *prev = rf_buff_end ;
#endif
  // Read radio and increas rf_buff_end by actually read amount of data
  rf_buff_end += Serial.readBytes( rf_buff_end , to_read_bytes ) ;
  
#ifdef DEBUG 
  memcpy ( buffer, prev, (rf_buff_end - prev) * sizeof(char) ) ;
  buffer[rf_buff_end - prev] = 0 ;
  logger->print( buffer ) ;
#endif
  
  // Return amount of available data in rf_buff
  return ( rf_buff_end - rf_buff_beg ) ;
}

