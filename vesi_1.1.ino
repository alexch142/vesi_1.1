/* **********************************************************************************************************************************************************************************************************************************
Версия под Arduino Uno .
Реализовано:
- коннект в сеть
- проверка коннекта только в начале работы
- установка тары
- отображения веса , времени , состояния подключения на lcd дисплее
- смс по расписанию , по запросу , при изменении веса на 2кг за один цикл измерения(~ 1,5 сек.)
Не реализовано:
- контроль зависания(watch dog)
- перезагрузка при зависании
- контроль соединения в процессе работы 
- востановление соединения при потере в процессе работы
- презагрузка модема при зависании модема 
- контроль баланса сим-карты
- прием и разбор конфигурационного смс
- установка времени (смс)
- корректировка времени при уходе часов на фиксированную величину (смс)
- установка коэффициента для весов (смс)
- установка номера для контроля (смс)
- пепредача данных на сервер
- сохранение данных на карту
 



*********************************************************************************************************************************************************************************************************************************** */
#include "HX711.h"
#include <EEPROM.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <TimeLib.h>
#include <DS1307RTC.h>

//#define SerialDebag
//#define debag_lcd

#define  TARE_PIN  11
#define  HX711_DOUT_pin A1
#define  HX711_PD_SCK_pin A0
#define SCALE_KOEF 11219.70
#define RTC_GND_PIN A2
#define RTC_POW_PIN A3
#define GSM_On_Off 9
#define ALARM_H_1 7
#define ALARM_H_2 15
#define ALARM_H_3 20
#define PERIOD_OF_SAVING 15
#define SCALING_TAIME 100 //skolko raz vzveshivat
#define ADRES_EEPROM_1 10  // VES 
#define ADRES_EEPROM_2 20  // TARA
#define ADRES_EEPROM_FIRST_TIME 0 // proverka pervogo vkluchenia
#define ADRES_EEPROM_FIRST_TIME_T 2 // proverka pervogo vkluchenia

HX711 scale;
LiquidCrystal lcd (2,3,4,5,6,7);
tmElements_t tm;

const char *monthName[12] = {
  "Jan", "Feb", "Mar", "Apr", "May", "Jun",
  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};


 
byte antenn[8] = {
     B10101,
     B10101,
     B01110,
     B00100,
     B00100,
     B00100,
     B00100,
     B00100
};



float WorkMass = 0.0; // mass of  hive
//float LastMass = 0.0; //  mass of last scaling of hive
float New_wight = 0.0;
float Old_wight = 0.0;
String Number_for_Control = "+79187460224";
String inputString = "";         // a string to hold incoming data from gsm modem
boolean stringComplete = false;  // whether the string is complete



int RingCount = 0;
boolean CLCC_Ok = false;
boolean must_send_sms = false;// flag otsilki sms
boolean connected_gsm = false;  // est sviaz 
boolean must_safe = false; //flag vzveshivania

boolean AlarmFlag = false; // budilnik srabotal
boolean AlarmSmsSended = false; // sms po raspisaniyu otpravili
boolean safe_compleat = false; // vzveshivanie proizoshlo
boolean scale_flag = false; // rezkoe izmenenie vesa

void setup() {
  // put your setup code here, to run once:

  // reserve 200 bytes for the inputString:
  inputString.reserve(200);
  
  pinMode(TARE_PIN, INPUT_PULLUP);
  
  pinMode(RTC_GND_PIN,OUTPUT);
  pinMode(RTC_POW_PIN,OUTPUT);
  digitalWrite(RTC_GND_PIN,LOW);
  digitalWrite(RTC_POW_PIN,HIGH);
  
    Serial.begin(9600);


  
    
   lcd.begin(16,2);
   lcd.createChar(0,antenn); //sozdali znachek antenni
   lcd.clear();
   lcd.setCursor(0,0);
 //  lcd.print("TIME: ");
   lcd.setCursor(6,0);
   lcd.print("CTAPT... ");

   # ifdef SerialDebag
          
          Serial.println("CTAPT...");
  #endif




   
   powerUpOrDown(GSM_On_Off);
   delay(3000);
   delay(8000);
   Serial.println("ATE0"); 

 
   delay(5000); 
   
   test_gsm_connect();

   

  
   for(int i = 0; i<10 ; i++){ 
                                  #ifdef debag_lcd
                                    lcd.setCursor(0,1);
                                    lcd.print(i);
                                  #endif
                                
                                
                               while (Serial.available()) {
                                           // get the new byte:
                                      char inChar = (char)Serial.read();
                                            // add it to the inputString:
                                      inputString += inChar;
                                                                                 
                                          // if the incoming character is a newline, set a flag
                                         // so the main loop can do something about it:
                                       if (inChar == '\n') {
                                               stringComplete = true;
                                               break;
                                       }
                                }
                             if (stringComplete) {
                                    StrHandler(inputString);
                               // clear the string:
                              inputString = "";
                              stringComplete = false;
                                                   
                            };

                           if(connected_gsm){break;};
                           delay(1000);
                             
                       
   } ;
   
   
  if(connected_gsm){  
                       #ifdef SerialDebag
                          Serial.println("CONNECT...");
                       #endif
                     lcd_print_connect();
                     
  } else{ 
           #ifdef SerialDebag
               Serial.println("NOT CONNECT...");
           #endif
            lcd_print_not_connect();
  };
 

/*

  char test_eeprom = EEPROM.read(ADRES_EEPROM_FIRST_TIME);
  if (test_eeprom == 'W'){
                           LastMass = EEPROM_float_read(ADRES_EEPROM_1);
    
  } else { 
           EEPROM.write(ADRES_EEPROM_FIRST_TIME, 'W');
           EEPROM_float_write(ADRES_EEPROM_1, 0.0);
           LastMass = EEPROM_float_read(ADRES_EEPROM_1);
    };

  */  


// Serial.println("Initializing the scale");
  // parameter "gain" is ommited; the default value 128 is used by the library
  // HX711.DOUT  - pin #A1
  // HX711.PD_SCK - pin #A0

 scale.begin(HX711_DOUT_pin,HX711_PD_SCK_pin,64);

 char test_eeprom = EEPROM.read(ADRES_EEPROM_FIRST_TIME);
  if (test_eeprom == 'W'){
                           long tar = EEPROM_long_read(ADRES_EEPROM_2);
                           scale.set_offset(tar);

                           #ifdef SerialDebag
                             Serial.print("tar=");
                             Serial.println(tar);
                            #endif
    
  } else { 
           EEPROM.write(ADRES_EEPROM_FIRST_TIME, 'W');
           EEPROM_long_write(ADRES_EEPROM_2, 0);
           scale.set_offset(EEPROM_long_read(ADRES_EEPROM_2));

           #ifdef SerialDebag
               Serial.print("first_tare=");
               Serial.println(EEPROM_long_read(ADRES_EEPROM_2));
           #endif
    };

/*

  char test_eeprom_t = EEPROM.read(ADRES_EEPROM_FIRST_TIME_T);
  if (test_eeprom_t != 'T'){
                           // get the date and time the compiler was run
                          if (getDate(__DATE__) && getTime(__TIME__)) {
   
                          // and configure the RTC with this info
                         RTC.write(tm) ;
                         EEPROM.write(ADRES_EEPROM_FIRST_TIME_T, 'T');
  };
    
  } 

*/
   
    
    scale.set_scale(SCALE_KOEF);
    WorkMass = scaling() ;

    #ifdef SerialDebag
      Serial.println("end of setup");
    #endif
}




bool getTime(const char *str)
{
  int Hour, Min, Sec;

  if (sscanf(str, "%d:%d:%d", &Hour, &Min, &Sec) != 3) return false;
  tm.Hour = Hour;
  tm.Minute = Min;
  tm.Second = Sec;
  return true;
}

bool getDate(const char *str)
{
  char Month[12];
  int Day, Year;
  uint8_t monthIndex;

  if (sscanf(str, "%s %d %d", Month, &Day, &Year) != 3) return false;
  for (monthIndex = 0; monthIndex < 12; monthIndex++) {
    if (strcmp(Month, monthName[monthIndex]) == 0) break;
  }
  if (monthIndex >= 12) return false;
  tm.Day = Day;
  tm.Month = monthIndex + 1;
  tm.Year = CalendarYrToTm(Year);
  return true;
}


  
void lcd_print_connect(){
                     lcd.clear();
                     lcd.setCursor(0,0);
                     lcd_show_antenn();
                     lcd.setCursor(0,1);
                     lcd.print("BEC:");
  
}

void lcd_print_not_connect(){
          lcd.clear();
          lcd.setCursor(0,0);
          lcd.print("GSM ERROR"); 
          lcd.setCursor(0,1);
          lcd.print("BEC:");
  
}


void lcd_show_antenn(){

                   lcd.write((byte)0);
  
};

void test_gsm_connect(){

                   Serial.println("AT+CREG?");
                   
                   
};

void StrHandler(String str){

    
      if(str.indexOf('\r')==0){
      
        inputString = "";
       return;
    };
  
     if(str.indexOf("RING")==0){
        ring();
        return;
     };
    if(str.indexOf("OK")==0){
      
        inputString = "";
       return;
    };

    
  
    if(str.indexOf("+CLCC")==0){
      must_send_sms = clcc(str,Number_for_Control);
      return;
    };

     if(str.indexOf("+CMTI")==0){
      cmti();
      return;
    };
    
    if(str.indexOf("+CREG:")==0){
    
      creg();
      return;
    };
    
}


void ring(){
              

              if(RingCount>=4){ Serial.println("AT+CLCC");
                                  RingCount=0;
                                //   lcd.setCursor(11,1);
                                //   lcd.print(RingCount );

                
              }else {
                     RingCount++;
                   //   lcd.setCursor(8,1);
                  //    lcd.print(RingCount );
                    
               };

          
}

boolean clcc(String str, String str2){
           boolean ret = false;

           int plusPosition = str.indexOf('"');

            
           String num = str.substring((plusPosition+1),( plusPosition+13));
           

           if (num == str2){ret = true; 
                          //  lcd.setCursor(14,1);
                         //    lcd.print("P");
                         };
                         
                      
           Serial.println("AT+CHUP");
           
           CLCC_Ok=true;
           
           // lcd.clear();
          //  lcd.setCursor(0,1);
          //  lcd.print(num );
      
           return ret;
}

void cmti(){
     
          Serial.println("CMTI_OK");
         
}

void creg(){
           connected_gsm = true;  
}

void sendSMS(String txt){

            sendTextMessag(Number_for_Control,txt);
            
            if(must_send_sms){must_send_sms = false;};
            if(AlarmFlag){AlarmSmsSended = true;};
            if(scale_flag){scale_flag = false;};
}

String buildSMS(){
      String txtMsg;
      
      WorkMass= scaling();  
      
      txtMsg = 
       l_2digits(tm.Month) +'/'
      +l_2digits(tm.Day )+'-'
      +l_2digits(tm.Hour)+':'
      +l_2digits(tm.Minute)+':'
      +l_2digits(tm.Second)+" BEC:" 
      +String(WorkMass);
                  
      
      return txtMsg;
}

String buildExtraSms(){
       String txtMsg;

       txtMsg = 
       l_2digits(tm.Month) +'/'
      +l_2digits(tm.Day )+'-'
      +l_2digits(tm.Hour)+':'
      +l_2digits(tm.Minute)+':'
      +l_2digits(tm.Second)+"EXTRA CHENGE WIGHT" ;
      

       return txtMsg;
}

void saving(){
      EEPROM_float_write(ADRES_EEPROM_1, scaling());
      safe_compleat = true;
      
}

float scaling(){
        WorkMass = scale.get_units(SCALING_TAIME)  ; ///??????????????????
        
        return WorkMass;   //    ОКРУГЛИТЬ!!!!!!!!!!!!!!!     
}


void ShowMass_tar(){

          if(connected_gsm){  
                  lcd_print_connect();
                  lcd.setCursor(6,1);
                  lcd.print(scaling(),1 );
                     
          } else{ 
                  lcd_print_not_connect();
                  lcd.setCursor(6,1);
                  lcd.print(scaling(),1 );
           };
  
        
}


void ShowMass(float wight){

 if(connected_gsm){    
                  lcd.setCursor(9,1);
                  lcd.print("     ");
                  lcd.setCursor(6,1);
                  lcd.print(wight,1 );
 } else{
                  lcd.setCursor(13,1);
                  lcd.print("  ");
                  lcd.setCursor(10,1);
                  lcd.print(wight,1 );
                    
  };

 }                  

 void TARE(float k){
              
          long tare = 0;

              #ifdef SerialDebag
               Serial.println("start tare");
               #endif
          
               scale.set_scale();                      // this value is obtained by calibrating the scale with known weights; see the README for details
               scale.tare(); 
               delay(5000);
               delay(5000); 
               scale.set_scale(k);                      // this value is obtained by calibrating the scale with known weights; see the README for details
               scale.tare(SCALING_TAIME); 
               tare =  scale.get_offset(); 
               EEPROM_long_write(ADRES_EEPROM_2, tare);
               
               #ifdef SerialDebag
               Serial.print("taring=");
               Serial.println(tare);
               #endif
               
 }

        // запись в ЕЕПРОМ float
 void EEPROM_float_write(int addr, float val){   
           byte *x = (byte *)&val;
           for(byte i = 0; i < 4; i++) EEPROM.write(i+addr, x[i]);
}

 
     // чтение из ЕЕПРОМ float
float EEPROM_float_read(int addr) {
         byte x[4];
         for(byte i = 0; i < 4; i++) x[i] = EEPROM.read(i+addr);
         float *y = (float *)&x;
         return y[0];
}


       // запись в ЕЕПРОМ long
 void EEPROM_long_write(int addr, long val){   
           byte *x = (byte *)&val;
           for(byte i = 0; i < 4; i++) EEPROM.write(i+addr, x[i]);
}

 
     // чтение из ЕЕПРОМ long
float EEPROM_long_read(int addr) {
         byte x[4];
         for(byte i = 0; i < 4; i++) x[i] = EEPROM.read(i+addr);
         long *y = (long *)&x;
         return y[0];
}



void l_print2digits(int number) {
  if (number >= 0 && number < 10) {
    lcd.print('0');
  }
    lcd.print(number);
}

String l_2digits(int number){
     String str;
      if (number >= 0 && number < 10) {
    str ='0';
  }
    str = str + number;
    return str;
  
}

void powerUpOrDown(int pin){

 pinMode(pin, OUTPUT);
 digitalWrite(pin,LOW);
 delay(1000);
 digitalWrite(pin,HIGH);
 delay(2000);
 digitalWrite(pin,LOW);
 delay(3000);
}



void sendTextMessag(String remoteNum,String txt){
     
    // Устанавливает текстовый режим для SMS-сообщений
    Serial.println("AT+CMGF=1");
    delay(200); // даём время на усваивание команды
    // Устанавливаем адресата: телефонный номер в международном формате
    Serial.println("AT+CMGS=\"" + remoteNum + "\"");
    delay(500);// ждем прихода символа ">"
    // Пишем текст сообщения
    Serial.println(txt);
   // Serial.println(scaling(),1 );
    delay(200);
    // Отправляем Ctrl+Z, обозначая, что сообщение готово
    Serial.println((char)26);
    delay(200);


     CLCC_Ok= false;
    
    }     

 
void loop() {

int scalePeriod ;

         // print the string when a newline arrives:
  if (stringComplete) {
    

    
    StrHandler(inputString);
    // clear the string:
    inputString = "";
    stringComplete = false;
  }


  if( !digitalRead(TARE_PIN) ){
                              delay(500);
                                                            
                              if( !digitalRead(TARE_PIN) ){
                                             lcd.clear();
                                             lcd.setCursor(0,3);
                                             lcd.print("SET TARA...");
                                             delay(4000);
                                             TARE(SCALE_KOEF);
                                             ShowMass_tar();
                                
                              };
    
  };


 if(connected_gsm){
  
  if(AlarmFlag and !AlarmSmsSended){sendSMS(buildSMS());}; 
  if (must_send_sms){sendSMS(buildSMS());};
   // if(must_safe and !safe_compleat){saving();};
  if(scale_flag){sendSMS(buildExtraSms());};

 };
       
      if(connected_gsm){
          lcd.setCursor(6,0);
      } else {
          lcd.setCursor(10,0);
      };
      
       if (RTC.read(tm)) {  switch (tm.Hour) {
                                        case ALARM_H_1:if((tm.Minute==0)and(AlarmSmsSended ==false)){
                                                              AlarmFlag = true;
                                                        }
                                                                             else{
                                                                                   if(tm.Minute==0){
                                                                                       AlarmFlag = false;
                                                                                   }
                                                                                    else{
                                                                                       AlarmSmsSended = false;
                                                                                       AlarmFlag = false;
                                                                                    }; 
                                                                              };
                                        break;
                                        case ALARM_H_2:if((tm.Minute==0)and(AlarmSmsSended ==false)){
                                                              AlarmFlag = true;
                                                        }
                                                                             else{
                                                                                   if(tm.Minute==0){
                                                                                       AlarmFlag = false;
                                                                                   }
                                                                                    else{
                                                                                       AlarmSmsSended = false;
                                                                                       AlarmFlag = false;
                                                                                    }; 
                                                                              };
                                        break;
                                        case ALARM_H_3:if((tm.Minute==0)and(AlarmSmsSended ==false)){
                                                              AlarmFlag = true;
                                                        }
                                                                             else{
                                                                                   if(tm.Minute==0){
                                                                                       AlarmFlag = false;
                                                                                   }
                                                                                    else{
                                                                                       AlarmSmsSended = false;
                                                                                       AlarmFlag = false;
                                                                                    }; 
                                                                              };
                                                                              
                                        break;
                                
                                    
        
                             };
                                  
                                  //  if(tm.Minute==0){ scalePeriod = 1;}else{  scalePeriod = tm.Minute % PERIOD_OF_SAVING;} ;
                                  //  if(scalePeriod == 0)  { must_safe = true;} else {must_safe = false; safe_compleat = false;}; 

                             
    
                             l_print2digits(tm.Hour);
                             lcd.print(':');
                             l_print2digits(tm.Minute);
                            // lcd.print(':');
                            // l_print2digits(tm.Second);
    
    
       }    else {
                   if (RTC.chipPresent()) { lcd.clear();
                                            lcd.setCursor(0,0);
                                            lcd.print("DS1307 is stop!");
                                            
                   } else { lcd.clear();
                            lcd.setCursor(0,0);
                            lcd.print("DS1307 read err!");
                            
                     }
                      delay(9000);
          }
          
 
   #ifdef SerialDebag
          Serial.println(scaling(),1 );
   #endif
   



 New_wight = scaling();
 
     if(((New_wight- Old_wight)>2.0) or ((New_wight- Old_wight)<(-2.0))) {scale_flag= true;};
 Old_wight = New_wight;
    ShowMass(New_wight);
}

void serialEvent() {
  while (Serial.available()) {
    // get the new byte:
    char inChar = (char)Serial.read();
    // add it to the inputString:

    inputString += inChar;

    // if the incoming character is a newline, set a flag
    // so the main loop can do something about it:
    if (inChar == '\n') {
      stringComplete = true;
        
      break;
      
    }
  }
}
