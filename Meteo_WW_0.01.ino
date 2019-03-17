#include <DHT.h>
#include <DHT_U.h>

/*

      World  Meteostation v.0.01


*/
// Serial 1 - RX 0, TX 1
// Serial 2 - RX 12, TX 10
// Serial 3 - RX 5, TX 4
#define GSMpow 8
#define GSMport Serial1
#define pin_v A1
#define Pin_sign 13
#include "DHT.h"
#define DHTPIN 9
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
#include <SFE_BMP180.h>
#include <Wire.h>
#include <RTCZero.h>
#include <TinyGPS++.h>
#include <BH1750.h>
#include "Timer.h"
#include <time.h>
#define BSPin 6
#define GSPin 7 

struct AllData{
  int hh,mm,ss,dow,dd,mon,yyyy; // время и дата
  float Volt;
  int Bat_lvl;
  float hum,temp;       // влажность температура от DHT22
  double p,p0;        // давление и температура ще BMP180
  double TV;
  uint16_t lux; // осв в люксах BH1750
  double alt;    //высота над ур моря
  double longi , lati; // долгота и широта
  int nos_napr;
  float nos_sil;  // напр и сила ветра
  };


int voskr, intrv = 20, shift = 2;
const unsigned long PERIOD1 = 1000;    //one second
const unsigned long PERIOD2 = 10000;
int bs_timer, gs_timer;
static const uint32_t GPSBaud = 9600; 
String s_tl = "+380632752853";
String gsm_bal, sms_text, sms_num;
int sms_kol;
boolean sms_sender=false;

RTCZero rtc;  
DHT dht(DHTPIN, DHTTYPE);
SFE_BMP180 pressure;
BH1750 lightMeter;
Timer tlight;  
TinyGPSPlus gps;

AllData wsd;
int input_v;
char statusPT;
String send_data, ans;
int cur_hh,cur_mm, cur_ss,cur_dow,cur_dd, cur_mon, cur_yyyy;
String GSM_Net;
int Time_Zone;
time_t epot;
tm tim;

//Чтение из буфера GSM
String GSM_buf()
{
    do { delay(50) ; } while (GSMport.available()==0);
    String s="";
    for (int k=0; k<10000; k++) {
          char c = GSMport.read();
//          Serial.write(c);
          delayMicroseconds(50);
          if ((byte)c!=255) s+=c;
          if (s.endsWith("OK\r\n")) break;
       }
//   Serial.println(s);
//   Serial.println("==============");
   
   return s;
}

// запуск GSM
String GSM_start(){
  
    GSMport.begin(9600);          
    GSMport.println("AT");
    delay(200);
    String r=GSM_buf();
    if ( r.indexOf("OK")==-1) {
      pinMode(GSMpow, OUTPUT);
      digitalWrite(GSMpow, HIGH);
      delay(1000);
      digitalWrite(GSMpow, LOW);
      delay(5000);
       }
    GSMport.println("AT"); r=GSM_buf();   
    GSMport.println("ATE0"); r=GSM_buf();   
    GSMport.println("AT+CMGF=1");  r=GSM_buf(); delay(500); 
    for (int i=0 ; i<=20; i++){
      GSMport.println("AT+COPS?");
      r=GSM_buf();
      if ( r.indexOf("0,0,")!=-1) { break;}
      delay(1000);
    }
        int opp=r.indexOf("\"")+1;
    int opk=r.lastIndexOf("\"");
    String op=r.substring(opp,opk);
    GSMport.println("AT+CPMS=\"SM\"");  r=GSM_buf(); 
    return op;
 }

// получение времени GSM
int GSM_clock(){
    for (int i=0 ; i<=20; i++){
        GSMport.println("AT+CCLK?");
        String r=GSM_buf();
        if ( (r.indexOf("00/01/01")==-1)&& (r.indexOf("+CCLK:")!=-1)) {
          int tn=r.indexOf("CCLK: \"")+7;
          int tk=tn+2;
          String clok=r.substring(tn,tk);
          int clokn=clok.toInt();
          rtc.setYear(clokn);    //setting year
          tn=tk+1;
           tk=tn+2;
           clok=r.substring(tn,tk);
           clokn=clok.toInt();
          rtc.setMonth(clokn);    //setting month
          tn=tk+1;
           tk=tn+2;
           clok=r.substring(tn,tk);
           clokn=clok.toInt();
          rtc.setDay(clokn);     //setting day
          tn=tk+1;
           tk=tn+2;
           clok=r.substring(tn,tk);
           clokn=clok.toInt();
          rtc.setHours(clokn);        //setting hour
          tn=tk+1;
           tk=tn+2;
           clok=r.substring(tn,tk);
           clokn=clok.toInt();
          rtc.setMinutes(clokn);  //setting minute
          tn=tk+1;
           tk=tn+2;
           clok=r.substring(tn,tk);
           clokn=clok.toInt();
          rtc.setSeconds(clokn);   //setting second
           tn=tk; 
           tk=tn+3;          
           clok=r.substring(tn,tk);          
           clokn=clok.toInt()/4;  
           return clokn;
          break;
          }
        delay(1000); 
        }
      
 }

  // отправка смс  GSM
void GSM_ssms(String tl, String txt){
        GSMport.println(String("AT+CMGS=\"")+tl+String("\""));
        delay(100);
        String r=GSM_buf();
        GSMport.println(txt+String (char(26)));
 }
  
  // получение силы  GSM
int GSM_sign(){
        GSMport.println("AT+CSQ");
          String r=GSM_buf();
          int tn=r.lastIndexOf("CSQ")+4;
          int tk=r.indexOf(",");     
          String s=r.substring(tn,tk);         
          int sn=s.toInt();    
          return sn;  
 }

  // получение баланса счета
String GSM_money(){
         GSMport.println("AT+CUSD=1,\"*111#\"");
         String r=GSM_buf();
         delay(1000);
         r=GSM_buf();
         int tn = r.indexOf("\"Balans")+1; 
        int tk = r.indexOf(",",tn); 
        String s=r.substring(tn,tk);
         return s;  
 }

 //КОЛВО смс
 int GSM_csms(){
        GSMport.println("AT+CPMS?");
        String r=GSM_buf(); //Serial.println(r);
        int tn = r.indexOf("\"SM\",")+5; 
        int tk = r.indexOf(",",tn); 
        String s=r.substring(tn,tk);
        int sn=s.toInt(); 
        return sn;  
}

// ЧТЕНИЕ смс №
String GSM_rsms(int nom_sms , String *tel_nom){
        GSMport.println("AT+CMGR="+String(nom_sms));
        String r=GSM_buf(); //Serial.println(r);
        String s,sn;
        int tn = r.indexOf("+CMGR:"); 
        if(tn==-1){
           s="";
           sn="";
        } else {
            tn = r.indexOf("\"",tn); 
            tn = r.indexOf("\"",tn+1); 
            tn = r.indexOf("\"",tn+1); 
        int tk = r.indexOf("\"",tn+1); 
            tn+=1;
            sn=r.substring(tn,tk); 
            tn = r.indexOf("\r\n",tn)+2; 
            tk = r.indexOf("\r\n",tn); 
            s=r.substring(tn,tk);};
        *tel_nom=sn;
        return s;
}

// Удаление смс №
void GSM_dsms(int nom_sms){
        GSMport.println("AT+CMGD="+String(nom_sms));
        String r=GSM_buf(); //  Serial.println(r);
}
 
// конверсия double (float) в строку
String DtoS(double a, byte dig) {
  double b,c;
  double* pc =&c;
  String s="";
  long bef;                      
  int i,digit;
  b=modf(a,pc);
  bef = c;
  s+=String(bef)+String(".");
  if (b<0) {b=-b;};
  for (i = 0; i < dig; i++) {
         b *= 10.0; 
         digit = (int) b;
         b = b - (float) digit;
     s+= String(digit);
     }
  return s;  
}

// преобразование времени в строку
String Tstr(int h, int m, int sec){
  String s= "";
  if (h<10){ s+="0"; }
  s += String (h);
  s+=":";
  
   if (m<10){ s+="0"; }
  s += String (m);
  s+=":";
  
   if (sec<10){ s+="0"; }
  s += String (sec);
  return s;
}

// преобразование даты в строку
String Dstr(int den, int chis, int mes, int god){
char* daynames[]={"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"};
  String s= "";
  s+=String (daynames[den]);
  s+=":";
  if (chis<10){ s+="0"; }
  s += String (chis);
  s+=".";
  if (mes<10){ s+="0"; }
  s += String (mes);
  s+=".";
  god=god-2000;
   if (god<10){ s+="0"; }
  s += String (god);
  return s;
}


// день недели
int dowFunc (int year, int month, int day)
{
  int leap = year*365 + (year/4) - (year/100) + (year/400);
  year += ((month+9)/12) - 1;
  month = (month+9) % 12;
  int zeller = leap  + month*30 + ((6*month+5)/10) + day + 1;
  return (zeller % 7);
}

// конверсия всех данных в строку
String con_DtoS(AllData d){
  String buf; 
  String s = "{";
  s +=String (d.hh); s+=";";
  s +=String (d.mm); s+=";";
  s +=String (d.ss); s+=";";
  s +=String (d.dow); s+=";";
  s +=String (d.dd); s+=";";
  s +=String (d.mon); s+=";";
  s +=String (d.yyyy); s+=";";
  buf = DtoS(d.Volt, 2); s += buf; s+=";";
  s +=String (d.Bat_lvl); s+=";";
  buf = DtoS(d.hum,3); s += buf; s+=";";
  buf = DtoS(d.temp,3); s += buf; s+=";";
  buf = DtoS(d.p,3); s += buf; s+=";";
  buf = DtoS(d.p0,3); s += buf; s+=";";
  buf = DtoS(d.TV,3); s += buf; s+=";";
  s +=String (d.lux); s+=";";
  buf = DtoS(d.alt,2); s += buf; s+=";";
  buf = DtoS(d.longi,7); s += buf; s+=";";
  buf = DtoS(d.lati,7); s += buf; s+=";";
  s +=String (d.nos_napr); s+=";"; 
  buf = DtoS(d.nos_sil,3); s += buf; s+=";"; 
  s +="}"; 
  
  return s;
}


void setup()
{
  pinMode(Pin_sign,OUTPUT);
  digitalWrite(Pin_sign,LOW);  
  
  // initialize serial for debugging
  Serial.begin(115200);

  //инит бле
  Serial3.begin(115200);   

  //инит гпс
  Serial2.begin(GPSBaud);     
  
  rtc.begin();
  dht.begin();
  pressure.begin();
  lightMeter.begin();
  
pinMode (GSPin,OUTPUT);
pinMode (BSPin,OUTPUT);
digitalWrite (GSPin,LOW);
digitalWrite (BSPin,LOW);

  wsd.alt = 0; 
  wsd.lati= 0.0;
  wsd.longi=0.0;
  wsd.nos_napr=0;
  wsd.nos_sil=0.0;
  wsd.Volt=0;
  wsd.Bat_lvl=0;

  // запуск гсм и счит времени
  GSM_Net=GSM_start();
          Serial.println(GSM_Net);
  Time_Zone=GSM_clock();
          Serial.println(Time_Zone);
  bs_timer=tlight.oscillate(BSPin, PERIOD1, HIGH);

  //запуск гпс
  while(true) {
    tlight.update();
    while (Serial2.available() > 0)
    if (gps.encode(Serial2.read())) {
    /* displayInfo()*/; }
    if (gps.location.isValid()) break;  
  }  
   gs_timer=tlight.oscillate(GSPin, PERIOD2, HIGH);

  // проверка баланса
  gsm_bal=GSM_money();
  
}


void loop()
{  
   tlight.update();
      
  // время и дата cur_hh,cur_mm, cur_ss,cur_dow,cur_dd, cur_mon, cur_yyyy;
  cur_hh=rtc.getHours();
  cur_mm=rtc.getMinutes();
  cur_ss=rtc.getSeconds();
  cur_dd=rtc.getDay();
  cur_mon=rtc.getMonth();
  cur_yyyy=rtc.getYear()+2000;

  // read GPS
  while (Serial2.available() > 0)
    if (gps.encode(Serial2.read()));

  if (cur_ss==10){
  wsd.hh=cur_hh; wsd.mm=cur_mm; wsd.ss=cur_ss;
  wsd.dow=cur_dow; wsd.dd=cur_dd; wsd.mon=cur_mon; wsd.yyyy=cur_yyyy;
  wsd.dow=dowFunc(wsd.yyyy,wsd.mon,wsd.dd);

  wsd.alt = gps.altitude.meters(); 
  wsd.lati= gps.location.lat();
  wsd.longi=gps.location.lng();
  
  // измерение напряжения батареи
  input_v = analogRead(pin_v);
  wsd.Volt = ((float)input_v/1023)*6.6;
  wsd.Bat_lvl = ((wsd.Volt - 2.4)/1.8)*100;

  // измерение влажности и температуры
  delay (1000);
  do {
  wsd.hum = dht.readHumidity();
  wsd.temp = dht.readTemperature();
  } while (isnan(wsd.hum));
  
 //проверка давления 
  statusPT = pressure.startTemperature();
  if (statusPT != 0)
  {
    // Wait for the measurement to complete:
    delay(statusPT); 
  statusPT = pressure.getTemperature(wsd.TV); }
  if (statusPT != 0)
    {
      // Start a pressure measurement:
      // The parameter is the oversampling setting, from 0 to 3 (highest res, longest wait).
      // If request is successful, the number of ms to wait is returned.
      // If request is unsuccessful, 0 is returned.

      statusPT = pressure.startPressure(3);
      if (statusPT != 0)
      {
        // Wait for the measurement to complete:
        delay(statusPT);
       statusPT = pressure.getPressure(wsd.p,wsd.TV);
       wsd.p0 = pressure.sealevel(wsd.p,wsd.alt); 
       wsd.p= wsd.p*0.7500637554;
       wsd.p0= wsd.p0*0.7500637554;
      }
    } 

  // проверка осв
  wsd.lux = lightMeter.readLightLevel();

  send_data=con_DtoS(wsd);
  send_data=String("POSTWT")+send_data+String("\r\n\r\n");
//  Serial.println(send_data);

// проверка баланса
  if((cur_mm%30)==0)
  gsm_bal=GSM_money();

// обработка смс с карточки
  if((cur_mm%5)==0) {
    sms_kol=GSM_csms();   
    for(int i= 1; i <=sms_kol; i++){
      sms_text=GSM_rsms(i,&sms_num);  
      GSM_dsms(i);  
      }    
  }

  // робот отправщик смс
//    Serial.println(cur_mm);  Serial.println(sms_sender);
  if (cur_mm==0 && sms_sender) {
//      Serial.print("===");   Serial.println(send_data);
  GSM_ssms(s_tl,send_data);  
  }
   
  }  

  //работа бле
    if (Serial3.available())
    {      ans=""; 
      int len=Serial3.available();
//    Serial.println(len);
    for (int i=0; i<len; i++){
    char c = Serial3.read();
    ans+=String(c);
    }
//           Serial.println(ans);
    if (ans.indexOf("OK+CONB")!=-1){
      delay(1000);
      Serial3.println("~~~~~~~~~~~~~~~~~~~~~");
      Serial3.println(" SiD World Travel ");      
      Serial3.println("      Welcome!    ");
      Serial3.println("~~~~~~~~~~~~~~~~~~~~~");
      Serial3.println("");
      tlight.stop(bs_timer);
      digitalWrite (BSPin,HIGH);
      }
      if (ans.indexOf("OK+LSTB")!=-1){
      delay(1000);
      bs_timer=tlight.oscillate(BSPin, PERIOD1, HIGH);
      }
      if (ans.indexOf("met")!=-1){
      Serial3.print("TEMP: ");
      Serial3.print(DtoS(wsd.temp,2));
      Serial3.println(" C*");
      Serial3.print("HUMIDITY: ");
      Serial3.print(DtoS(wsd.hum,2));
      Serial3.println(" %");
      Serial3.print("PRESSURE: ");
      Serial3.print(DtoS(wsd.p0,2));
      Serial3.println(" mm.Hg");
      Serial3.print("LIGHT: ");
      Serial3.print(wsd.lux);
      Serial3.println(" lux");
      Serial3.println("");
      }
      if (ans.startsWith("gps")){
      Serial3.print("LATITUDE: ");
      Serial3.print(DtoS(wsd.lati,6));
      Serial3.println(" ");
      Serial3.print("LONGITUDE: ");
      Serial3.print(DtoS(wsd.longi,6));
      Serial3.println("  ");
      Serial3.print("ELEVATION: ");
      Serial3.print(DtoS(wsd.alt,2));
      Serial3.println(" m");
      Serial3.println("");
      }
      if (ans.indexOf("sms")!=-1){
        if(ans.indexOf("on")!=-1){
          sms_sender=true;
          Serial3.println("SMS SENDER ON");         
        }
        if(ans.indexOf("off")!=-1){
          sms_sender=false;
          Serial3.println("SMS SENDER OFF");         
        }

      GSM_ssms(s_tl,send_data);
      Serial3.print("SENT TO ");
      Serial3.print(s_tl);
      Serial3.println("");
      }
      if (ans.indexOf("time")!=-1){
        if (ans.indexOf("setgsm")!=-1){
         Time_Zone=GSM_clock();
         Serial3.println("TIME WAS SET FROM GSM");
         cur_hh=rtc.getHours();
         cur_mm=rtc.getMinutes();
         cur_ss=rtc.getSeconds();
         cur_dd=rtc.getDay();
         cur_mon=rtc.getMonth();
         cur_yyyy=rtc.getYear()+2000; 
         cur_dow=dowFunc(cur_yyyy,cur_mon,cur_dd);       
         }
        if (ans.indexOf("setgps")!=-1){
         cur_hh=gps.time.hour();
         cur_mm=gps.time.minute();
         cur_ss=gps.time.second();
         cur_dd=gps.date.day();
         cur_mon=gps.date.month();
         cur_yyyy=gps.date.year();
         cur_dow=dowFunc(cur_yyyy,cur_mon,cur_dd);
         Serial3.print("GMT TIME: ");
         Serial3.print(Tstr(cur_hh,cur_mm,cur_ss));
         Serial3.println("  ");
         Serial3.print("GMT DATE: ");
         Serial3.print(Dstr(wsd.dow,cur_dd,cur_mon,cur_yyyy));
         Serial3.println("  ");
         Serial3.print("PLEASE ENTER YOUR TIMEZONE> "); 
         while (!Serial3.available());
         delay(100);
         ans=""; 
         len=Serial3.available();
         for (int i=0; i<len; i++){
         char c = Serial3.read();
         ans+=String(c);
         }
         int tz_sign;
         if (ans.startsWith("-")){
             tz_sign=-1;
             ans.replace("-" , "0");
         } else {
            tz_sign= 1;
         }
         Time_Zone=tz_sign*ans.toInt();
         Serial3.println(Time_Zone);
         tim.tm_year = gps.date.year()-1900;
         tim.tm_mon = gps.date.month()-1;           // Month, 0 - jan
         tim.tm_mday = gps.date.day();         // Day of the month
         tim.tm_hour = gps.time.hour();
         tim.tm_min = gps.time.minute();
         tim.tm_sec = gps.time.second();
         tim.tm_isdst = -1;        // Is DST on? 1 = yes, 0 = no, -1 = unknown
         epot = mktime(&tim);         
          epot=epot+Time_Zone*3600;
          rtc.setEpoch(epot);      
         Serial3.println("TIME WAS SET FROM GPS");
         cur_hh=rtc.getHours();
         cur_mm=rtc.getMinutes();
         cur_ss=rtc.getSeconds();
         cur_dd=rtc.getDay();
         cur_mon=rtc.getMonth();
         cur_yyyy=rtc.getYear()+2000; 
         cur_dow=dowFunc(cur_yyyy,cur_mon,cur_dd);
        }
      Serial3.print("TIME: ");
      Serial3.print(Tstr(cur_hh,cur_mm,cur_ss));
      Serial3.println("  ");
      Serial3.print("DATE: ");
      Serial3.print(Dstr(wsd.dow,cur_dd,cur_mon,cur_yyyy));
      Serial3.println("  ");
      Serial3.print("TIME ZONE: GMT ");
      Serial3.println(Time_Zone);
      Serial3.println("");
      }
      if (ans.indexOf("stat")!=-1){
      Serial3.print("GSM NET: ");
      Serial3.print(GSM_Net);
      Serial3.println("  ");
      int q=GSM_sign()/5;
      Serial3.print("GSM LvL: ");
      for (int i=0;i<6;i++){
        if (i<=q){
          Serial3.print("X");
        }
        else{
          Serial3.print("O");
        }
      }
      Serial3.println("");
      Serial3.print("BALANCE: ");
      Serial3.println(gsm_bal);
      Serial3.print("GPS: ");
      if (gps.satellites.isValid()){
        Serial3.println("OK");
        Serial3.print("SATELLITES: ");
        Serial3.println(gps.satellites.value());
      }
      else {
        Serial3.println("INVALID");  
      }
      Serial3.print("VOLTAGE: ");
      Serial3.print(DtoS(wsd.Volt,3));
      Serial3.println("V");    
      Serial3.print("BATTERY LvL: ");
      Serial3.print(wsd.Bat_lvl);
      Serial3.println("%");         
      Serial3.println("");
       }  
    Serial3.println("READY>");     
    }

}



