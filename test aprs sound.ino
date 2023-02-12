/* Koding asal saya copy dari :-

   Copyright (C) 2018 - Handiko Gesang - https://github.com/handiko/Arduino-APRS
  
   Saya edit: 
   
   1. Letak PTT_CTRL
   2. Letak LED_PTT indicator
   3. TX_delay tu, saya panjangkan
   4. NANO ada QRM bila guna koding ini
   5. UNO okay, tiada QRM

  de 9W2KEY 73                
 */
 
#include <math.h>
#include <stdio.h>

// Defines the Square Wave Output Pin
#define OUT_PIN 2       // audio out pin
int PTT_CTRL = 3;       // pin ctrl relay utk ctrl ptt handy bopeng UV-5R 
int LED_PTT = 13;       // LED PTT indicator

#define _1200   114:22:04.931 -> TX Preamble:  350
#define _2400   0

#define _FLAG       0x7e
#define _CTRL_ID    0x03
#define _PID        0xf0

#define _DT_EXP     ','
#define _DT_STATUS  '>'
#define _DT_POS     '!'

#define _NORMAL     1
#define _BEACON     2

#define _FIXPOS         1
#define _STATUS         2
#define _FIXPOS_STATUS  3

bool nada = _2400;

/*
 * SQUARE WAVE SIGNAL GENERATION
 * 
 * baud_adj lets you to adjust or fine tune overall baud rate
 * by simultaneously adjust the 1200 Hz and 2400 Hz tone,
 * so that both tone would scales synchronously.
 * adj_1200 determined the 1200 hz tone adjustment.
 * tc1200 is the half of the 1200 Hz signal periods.
 * 
 *      -------------------------                           -------
 *     |                         |                         |
 *     |                         |                         |
 *     |                         |                         |
 * ----                           -------------------------
 * 
 *     |<------ tc1200 --------->|<------ tc1200 --------->|
 *     
 * adj_2400 determined the 2400 hz tone adjustment.
 * tc2400 is the half of the 2400 Hz signal periods.
 * 
 *      ------------              ------------              -------
 *     |            |            |            |            |
 *     |            |            |            |            |            
 *     |            |            |            |            |
 * ----              ------------              ------------
 * 
 *     |<--tc2400-->|<--tc2400-->|<--tc2400-->|<--tc2400-->|
 *     
 */

// const float baud_adj = 0.975;  <-- asal
const float baud_adj = 0.975;
const float adj_1200 = 1.0 * baud_adj;
const float adj_2400 = 1.0 * baud_adj;
unsigned int tc1200 = (unsigned int)(0.5 * adj_1200 * 1000000.0 / 1200.0);
unsigned int tc2400 = (unsigned int)(0.5 * adj_2400 * 1000000.0 / 2400.0);

/*
 * This strings will be used to generate AFSK signals, over and over again.
 */
const char *mycall = "9W2KEY"; // change to your own callsign
char myssid = 1;

//const char *dest = "APRS";
const char *dest = "APZKY1";        // Destination address APZxxx Experimental http://www.aprs.org/aprs11/tocalls.txt
const char *dest_beacon = "BEACON";

const char *digi = "WIDE2";
char digissid = 1;

const char *mystatus = " Experimental APRS Arduino (UNO) ..:|Static station|:.. "; // 55 askara
const char *mystatus1 = " www.github.com/mzakiab "; // 24 askara

//QTH Pauh Butuk
const char *lat = "0557.94N";     // change to your own location
const char *lon = "10220.33E";    // change to your own location
const char sym_ovl = 'Y';
const char sym_tab = 'h';         // List symbols at http://www.aprs.org/symbols.html

unsigned int tx_delay = 5000;     // setting asal
// unsigned int tx_delay = 288000000;
unsigned int str_len = 400;

char bit_stuff = 0;
unsigned short crc=0xffff;

/*
 * 
 */
void set_nada_1200(void);
void set_nada_2400(void);
void set_nada(bool nada);

void send_char_NRZI(unsigned char in_byte, bool enBitStuff);
void send_string_len(const char *in_string, int len);

void calc_crc(bool in_bit);
void send_crc(void);

void send_packet(char packet_type, char dest_type);
void send_flag(unsigned char flag_len);
void send_header(char msg_type);
void send_payload(char type);

void set_io(void);
void print_code_version(void);
void print_debug(char type, char dest_type);

/*
 * 
 */
void set_nada_1200(void)
{
  digitalWrite(OUT_PIN, HIGH);
  delayMicroseconds(tc1200);
  digitalWrite(OUT_PIN, LOW);
  delayMicroseconds(tc1200);
}

void set_nada_2400(void)
{
  digitalWrite(OUT_PIN, HIGH);
  delayMicroseconds(tc2400);
  digitalWrite(OUT_PIN, LOW);
  delayMicroseconds(tc2400);
  
  digitalWrite(OUT_PIN, HIGH);
  delayMicroseconds(tc2400);
  digitalWrite(OUT_PIN, LOW);
  delayMicroseconds(tc2400);
}

void set_nada(bool nada)
{
  if(nada)
    set_nada_1200();
  else
    set_nada_2400();
}

/*
 * This function will calculate CRC-16 CCITT for the FCS (Frame Check Sequence)
 * as required for the HDLC frame validity check.
 * 
 * Using 0x1021 as polynomial generator. The CRC registers are initialized with
 * 0xFFFF
 */
void calc_crc(bool in_bit)
{
  unsigned short xor_in;
  
  xor_in = crc ^ in_bit;
  crc >>= 1;

  if(xor_in & 0x01)
    crc ^= 0x8408;
}

void send_crc(void)
{
  unsigned char crc_lo = crc ^ 0xff;
  unsigned char crc_hi = (crc >> 8) ^ 0xff;

  send_char_NRZI(crc_lo, HIGH);
  send_char_NRZI(crc_hi, HIGH);
}

void send_header(char msg_type)
{
  char temp;

  /*
   * APRS AX.25 Header 
   * ........................................................
   * |   DEST   |  SOURCE  |   DIGI   | CTRL FLD |    PID   |
   * --------------------------------------------------------
   * |  7 bytes |  7 bytes |  7 bytes |   0x03   |   0xf0   |
   * --------------------------------------------------------
   * 
   * DEST   : 6 byte "callsign" + 1 byte ssid
   * SOURCE : 6 byte your callsign + 1 byte ssid
   * DIGI   : 6 byte "digi callsign" + 1 byte ssid
   * 
   * ALL DEST, SOURCE, & DIGI are left shifted 1 bit, ASCII format.
   * DIGI ssid is left shifted 1 bit + 1
   * 
   * CTRL FLD is 0x03 and not shifted.
   * PID is 0xf0 and not shifted.
   */

  /********* DEST ***********/
  if(msg_type == _NORMAL)
  {
    temp = strlen(dest);
    for(int j=0; j<temp; j++)
      send_char_NRZI(dest[j] << 1, HIGH);
  }
  else if(msg_type == _BEACON)
  {
    temp = strlen(dest_beacon);
    for(int j=0; j<temp; j++)
      send_char_NRZI(dest_beacon[j] << 1, HIGH);
  }
  if(temp < 6)
  {
    for(int j=0; j<(6 - temp); j++)
      send_char_NRZI(' ' << 1, HIGH);
  }
  send_char_NRZI('0' << 1, HIGH);
  

  
  /********* SOURCE *********/
  temp = strlen(mycall);
  for(int j=0; j<temp; j++)
    send_char_NRZI(mycall[j] << 1, HIGH);
  if(temp < 6)
  {
    for(int j=0; j<(6 - temp); j++)
      send_char_NRZI(' ' << 1, HIGH);
  }
  send_char_NRZI((myssid + '0') << 1, HIGH);

  
  /********* DIGI ***********/
  temp = strlen(digi);
  for(int j=0; j<temp; j++)
    send_char_NRZI(digi[j] << 1, HIGH);
  if(temp < 6)
  {
    for(int j=0; j<(6 - temp); j++)
      send_char_NRZI(' ' << 1, HIGH);
  }
  send_char_NRZI(((digissid + '0') << 1) + 1, HIGH);

  /***** CTRL FLD & PID *****/
  send_char_NRZI(_CTRL_ID, HIGH);
  send_char_NRZI(_PID, HIGH);
}

void send_payload(char type)
{
  /*
   * APRS AX.25 Payloads
   * 
   * TYPE : POSITION
   * ........................................................
   * |DATA TYPE |    LAT   |SYMB. OVL.|    LON   |SYMB. TBL.|
   * --------------------------------------------------------
   * |  1 byte  |  8 bytes |  1 byte  |  9 bytes |  1 byte  |
   * --------------------------------------------------------
   * 
   * DATA TYPE  : !
   * LAT        : ddmm.ssN or ddmm.ssS
   * LON        : dddmm.ssE or dddmm.ssW
   * 
   * 
   * TYPE : STATUS
   * ..................................
   * |DATA TYPE |    STATUS TEXT      |
   * ----------------------------------
   * |  1 byte  |       N bytes       |
   * ----------------------------------
   * 
   * DATA TYPE  : >
   * STATUS TEXT: Free form text
   * 
   * 
   * TYPE : POSITION & STATUS
   * ..............................................................................
   * |DATA TYPE |    LAT   |SYMB. OVL.|    LON   |SYMB. TBL.|    STATUS TEXT      |
   * ------------------------------------------------------------------------------
   * |  1 byte  |  8 bytes |  1 byte  |  9 bytes |  1 byte  |       N bytes       |
   * ------------------------------------------------------------------------------
   * 
   * DATA TYPE  : !
   * LAT        : ddmm.ssN or ddmm.ssS
   * LON        : dddmm.ssE or dddmm.ssW
   * STATUS TEXT: Free form text
   * 
   * 
   * All of the data are sent in the form of ASCII Text, not shifted.
   * 
   */
  if(type == _FIXPOS)
  {
    send_char_NRZI(_DT_POS, HIGH);
    send_string_len(lat, strlen(lat));
    send_char_NRZI(sym_ovl, HIGH);
    send_string_len(lon, strlen(lon));
    send_char_NRZI(sym_tab, HIGH);
  }
  else if(type == _STATUS)
  {
    send_char_NRZI(_DT_STATUS, HIGH);
   // send_string_len(mystatus, strlen(mystatus));
    send_string_len(mystatus1, strlen(mystatus1));
  }
  else if(type == _FIXPOS_STATUS)
  {
    send_char_NRZI(_DT_POS, HIGH);
    send_string_len(lat, strlen(lat));
    send_char_NRZI(sym_ovl, HIGH);
    send_string_len(lon, strlen(lon));
    send_char_NRZI(sym_tab, HIGH);

    send_char_NRZI(' ', HIGH);
    send_string_len(mystatus, strlen(mystatus));
    // send_string_len(mystatus1, strlen(mystatus1));
  }
}

/*
 * This function will send one byte input and convert it
 * into AFSK signal one bit at a time LSB first.
 * 
 * The encode which used is NRZI (Non Return to Zero, Inverted)
 * bit 1 : transmitted as no change in tone
 * bit 0 : transmitted as change in tone
 */
void send_char_NRZI(unsigned char in_byte, bool enBitStuff)
{
  bool bits;
  
  for(int i = 0; i < 8; i++)
  {
    bits = in_byte & 0x01;

    calc_crc(bits);

    if(bits)
    {
      set_nada(nada);
      bit_stuff++;

      if((enBitStuff) && (bit_stuff == 5))
      {
        nada ^= 1;
        set_nada(nada);
        
        bit_stuff = 0;
      }
    }
    else
    {
      nada ^= 1;
      set_nada(nada);

      bit_stuff = 0;
    }

    in_byte >>= 1;
  }
}

void send_string_len(const char *in_string, int len)
{
  for(int j=0; j<len; j++)
    send_char_NRZI(in_string[j], HIGH);
}

void send_flag(unsigned char flag_len)
{
  for(int j=0; j<flag_len; j++)
    send_char_NRZI(_FLAG, LOW); 
}

/*
 * In this preliminary test, a packet is consists of FLAG(s) and PAYLOAD(s).
 * Standard APRS FLAG is 0x7e character sent over and over again as a packet
 * delimiter. In this example, 100 flags is used the preamble and 3 flags as
 * the postamble.
 */
void send_packet(char packet_type, char dest_type)
{
//print_debug(packet_type, dest_type);    // asal dia duk atas, nak try letak bawah dan letak delay 1 second
//digitalWrite(LED_BUILTIN, 1);           // asal comand ini duk atas sekali
  digitalWrite(PTT_CTRL, 1);              // ON Relay PTT
  digitalWrite(LED_PTT, 1);               // ON LED PTT
  digitalWrite(LED_BUILTIN, 1);           // nak cuba letak bawah pulak
  delay(1000);                            // tunggu metar, 1 saat je. tujuannya nak suruh PTT ON dulu baru audio tubik
  print_debug(packet_type, dest_type);    // audio baru tubik

  /*
   * AX25 FRAME
   * 
   * ........................................................
   * |  FLAG(s) |  HEADER  | PAYLOAD  | FCS(CRC) |  FLAG(s) |
   * --------------------------------------------------------
   * |  N bytes | 22 bytes |  N bytes | 2 bytes  |  N bytes |
   * --------------------------------------------------------
   * 
   * FLAG(s)  : 0x7e
   * HEADER   : see header
   * PAYLOAD  : 1 byte data type + N byte info
   * FCS      : 2 bytes calculated from HEADER + PAYLOAD
   */
  
  send_flag(100);
  crc = 0xffff;
  send_header(dest_type);
  send_payload(packet_type);
  send_crc();
  send_flag(3);

  delay(500);                     // Tunggu sebentar sebelum lepas PTT, nak suruh audio habis tubik dulu

  digitalWrite(LED_BUILTIN, 0);   // OFF built in LED
  digitalWrite(LED_PTT, 0);       // OFF LED PTT
  digitalWrite(PTT_CTRL, 0);      // OFF Relay PTT
}

/*
 * Function to randomized the value of a variable with defined low and hi limit value.
 * Used to create random AFSK pulse length.
 */
void randomize(unsigned int &var, unsigned int low, unsigned int high)
{
    var = random(low, high);
}

/*
 * 
 */
void set_io(void)
{
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(OUT_PIN, OUTPUT);
 
  Serial.begin(115200);
}

void print_code_version(void)
{
  Serial.println(" ");
  Serial.print("Sketch:   ");   Serial.println(__FILE__);
  Serial.print("Uploaded: ");   Serial.println(__DATE__);
  Serial.println(" ");
  
  Serial.println("Random String Pulsed AFSK Generator - Started \n");
}

void print_debug(char type, char dest_type)
{
  /*
   * PROTOCOL DEBUG.
   * 
   * Will outputs the transmitted data to the serial monitor
   * in the form of TNC2 string format.
   * 
   * MYCALL-N>APRS,DIGIn-N:<PAYLOAD STRING> <CR><LF>
   */
  
  /****** MYCALL ********/
  Serial.print(mycall);
  Serial.print('-');
  Serial.print(myssid, DEC);
  Serial.print('>');
  
  /******** DEST ********/
  if(dest_type == _NORMAL)
  {
    Serial.print(dest);
  }
  else if(dest_type == _BEACON)
  {
    Serial.print(dest_beacon);
  }
  Serial.print(',');

  /******** DIGI ********/
  Serial.print(digi);
  Serial.print('-');
  Serial.print(digissid, DEC);
  Serial.print(':');

  /******* PAYLOAD ******/
  if(type == _FIXPOS)
  {
    Serial.print(_DT_POS);
    Serial.print(lat);
    Serial.print(sym_ovl);
    Serial.print(lon);
    Serial.print(sym_tab);
  }
  else if(type == _STATUS)
  {
    Serial.print(_DT_STATUS);
    // Serial.print(mystatus);
    Serial.print(mystatus1);
  }
  else if(type == _FIXPOS_STATUS)
  {
    Serial.print(_DT_POS);
    Serial.print(lat);
    Serial.print(sym_ovl);
    Serial.print(lon);
    Serial.print(sym_tab);

    Serial.print(' ');
    Serial.print(mystatus);
   // Serial.print(mystatus1);
  }
  
  Serial.println(' ');
 
}

/*
 * 
 */
void setup()
{
  pinMode(PTT_CTRL, OUTPUT);    // PIN output mode
  pinMode(LED_PTT, OUTPUT);     // PIN output mode
  set_io();
  print_code_version();
}

void loop()
{
  send_packet(random(1,4), random(1,3));
  
  delay(tx_delay);
  randomize(tx_delay, 10, 5000); // setting asal 
  // randomize(tx_delay, 200000000, 288000000); // random TX 15 - 30 minit sekali
  randomize(str_len, 10, 420);
  delay(300000); // tunggu 5 minit sebelum TX data baru
}