#include <TinyGPS++.h>
#include <SoftwareSerial.h>

TinyGPSPlus gps;
SoftwareSerial GSMSerial(10, 11);
SoftwareSerial gpsSerial(8, 9);

const byte GPS_LED = 2;
const byte GSM_LED = 3;

void setup() {

  pinMode(GPS_LED,OUTPUT);
  pinMode(GSM_LED,OUTPUT);

  Serial.begin(9600);
  while(!Serial){;}
  GSMSerial.begin(9600);
  gpsSerial.begin(9600);
  delay(200);

  digitalWrite(GPS_LED,1);
  digitalWrite(GSM_LED,1);

  Serial.println(F("Starting..."));
  delay(5000);
  
  digitalWrite(GPS_LED,0);
  digitalWrite(GSM_LED,1);

  GSMSerial.listen();
  delay(200);
  GSMSerial.println("AT"); //Once the handshake test is successful, it will back to OK
  delay(200);
  if(GSMSerial.find("OK"))
    Serial.println(F("GSM Module is Connected"));
  else
  {
    while(!GSMSerial.find("OK"))
    {
      Serial.println(F("GSM Module is NOT Connected!"));
      GSMSerial.println(F("AT"));
      delay(200);
    }
    Serial.println(F("GSM Module is Connected"));
  }
  delay(200);

  GSMSerial.println("ATZ"); // Reset all configurations to default
  delay(200);

  GSMSerial.println("AT+CREG?"); // Cheack the network connection
  delay(200);
  if(GSMSerial.find(",1"))
    Serial.println(F("GSM Signal is Connected"));
  else
  {
    while(!GSMSerial.find(",1"))
    {
      Serial.println(F("GSM Signal is NOT Connected!"));
      GSMSerial.println("AT+CREG?");
      delay(200);
    }
    Serial.println(F("GSM Signal is Connected"));
  }
  delay(200);

  GSMSerial.println("AT+CSMP=17,167,0,0"); //Configure module for sending text in "Irancell" Operator Network
  delay(200);

  GSMSerial.println("AT+CMGF=1"); // Configuring TEXT mode
  delay(200);

  GSMSerial.println("AT+CNMI=2,2,0,0,0"); // Decides how newly arrived SMS messages should be handled
  delay(200);
  
  GSMSerial.println("AT+CMGD=4"); // Deletes all old messages
  delay(200);

  digitalWrite(GSM_LED,0);
  digitalWrite(GPS_LED,1);

  gpsSerial.listen();
  Serial.println(F("GPS Test..."));
  while(1)
  {
    if(gpsSerial.available() > 0)
      if(gps.encode(gpsSerial.read()));
        if(gps.location.isValid())
          break;
  }
  Serial.println(F("GPS is Connected"));
  digitalWrite(GPS_LED,0);

  GSMSerial.listen();
  Serial.println(F("Wating for SMS..."));

}

String gmaps = "" , sms = "", number = "", dcot;
double longitude, latitude, speed;
int index;

void loop() {

  digitalWrite(GSM_LED,1);
  ReadGSM();
  if(sms.indexOf("Location") > -1 || sms.indexOf("location") > -1)
  { 
    digitalWrite(GSM_LED,0);
    Serial.println(F("SMS Received!"));
    delay(200);
    Serial.println(F("GPS Serial Started..."));
    delay(1000);
    Serial.println(F("In while..."));
    gpsSerial.listen();
    while(1)
    {
      if(gpsSerial.available() > 0)
      {
        if(gps.encode(gpsSerial.read()));
        {
          if(gps.location.isValid())
          {
            digitalWrite(GPS_LED,1);
            Serial.println(F("GPS is Detected"));

            longitude = gps.location.lng();
            latitude = gps.location.lat();
          
            Serial.println(F("Calculating Speed..."));
            while(1)
            {
              if(gps.speed.isValid())
              {
                speed = gps.speed.kmph();
                break;
              }
            }
            Serial.println(F("Speed is Calculated"));
            delay(200);
            gmaps = "https://maps.google.com/?q=" + String(latitude,6) + ',' + String(longitude,6) ;
            break;
          }
        }
      }
    }

    digitalWrite(GSM_LED,1);
    dcot = '"';
    GSMSerial.listen();
    Serial.println(F("Sending Location via SMS..."));
    delay(1000);
    GSMSerial.println("AT+CMGS=" + dcot + number + dcot); // Use your number instead of '*' characters. Example: +989123456789
    delay(200);
    GSMSerial.print("Longitude: ");
    delay(200);
    GSMSerial.println(longitude);
    delay(200);
    GSMSerial.print("Latitude: ");
    delay(200);
    GSMSerial.println(latitude);
    delay(200);
    GSMSerial.print("Speed: ");
    delay(200);
    GSMSerial.print(speed);
    delay(200);
    GSMSerial.println(" km/h");
    delay(200);
    GSMSerial.println();
    delay(200);
    GSMSerial.print("Link: ");
    delay(200);
    GSMSerial.print(gmaps);
    delay(200);
    GSMSerial.write(26);
    delay(200);
    Serial.println(F("SMS Sent Successfully"));

    digitalWrite(GPS_LED,0);

    GSMSerial.println("AT+CMGD=4"); // Deletes all old messages
    delay(200);
    
    Serial.println(F("Wating for SMS..."));
  }

  digitalWrite(GSM_LED,0);
  delay(500);
}

void ReadGSM()
{
  delay(500);
  while(GSMSerial.available())
  {
    sms = GSMSerial.readString();
    index = sms.indexOf("+98");
    if(index > -1)
    {
      number = sms.substring(index,index+13);
      Serial.println("SMS FROM:" + number);
    }
  }
}
