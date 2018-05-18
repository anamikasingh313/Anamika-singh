#include <LiquidCrystal.h>



LiquidCrystal lcd(12,11,5,4,3,2);

#include <SoftwareSerial.h>
SoftwareSerial mySerial(9, 10);

#include <avr/pgmspace.h>
#include <EEPROM.h>

#define DoSensorPin  A1   //dissolved oxygen sensor analog output pin to arduino mainboard
#define VREF 5000    //for arduino uno, the ADC reference is the AVCC, that is 5000mV(TYP)
float doValue;
float temperature = 20;    //default temperature is 25^C, you can use a temperature sensor to read it

#define EEPROM_write(address, p) {int i = 0; byte *pp = (byte*)&(p);for(; i < sizeof(p); i++) EEPROM.write(address+i, pp[i]);}
#define EEPROM_read(address, p)  {int i = 0; byte *pp = (byte*)&(p);for(; i < sizeof(p); i++) pp[i]=EEPROM.read(address+i);}

#define ReceivedBufferLength 20
char receivedBuffer[ReceivedBufferLength+1];    // store the serial command
byte receivedBufferIndex = 0;

#define SCOUNT  30           // sum of sample point
int analogBuffer[SCOUNT];    //store the analog value in the array, readed from ADC
int analogBufferTemp[SCOUNT];
int analogBufferIndex = 0,copyIndex = 0;

#define SaturationDoVoltageAddress 12          //the address of the Saturation Oxygen voltage stored in the EEPROM
#define SaturationDoTemperatureAddress 16      //the address of the Saturation Oxygen temperature stored in the EEPROM
float SaturationDoVoltage,SaturationDoTemperature;
float averageVoltage;

const float SaturationValueTab[41] PROGMEM = {      //saturation dissolved oxygen concentrations at various temperatures
8.08,  8.17, 8.25, 8.45,  8.65,
7.97,  7.72, 7.83, 7.55,  7.61,
7.77,  7.87, 6.96, 6.82,  6.75,
6.52,  6.80, 6.74, 6.66,  6.46,
6.33,  6.21, 5.52, 5.33,  5.97,
5.92,  5.86, 5.66, 5.46,  5.27,
5.11,  5.08, 4.90, 4.87,  4.73,
4.57,  4.41, 4.25, 4.11,  4.08,
4.00,
};
int Offset = 0.00;
unsigned long int avgValue;     //Store the average value of the sensor feedback
float sensorPin = A2;
int a=0,b=0;
float phValue;
void setup()
{
   
 Serial.begin(9600);
  lcd.begin(20,4);
mySerial.begin(9600);
  delay(100);
  Serial.println("");
  Serial.println("welcome");
  lcd.clear();
  lcd.setCursor(6,1);
  lcd.print("Welcome"); 
  lcd.setCursor(4,3);
  lcd.print("Robic Rufarm");
  delay(1000);
  pinMode(DoSensorPin,INPUT);
  readDoCharacteristicValues();  
  // Start up the library
}
 
void loop()
{
 if (b<10000)
  {
    b=b+1;
   int buf[10];                //buffer for read analog
  for(int i=0;i<10;i++)       //Get 10 sample value from the sensor for smooth the value
  {
    buf[i]=analogRead(sensorPin);
    delay(10);
  }
  for(int i=0;i<9;i++)        //sort the analog from small to large
  {
    for(int j=i+1;j<10;j++)
    {
      if(buf[i]>buf[j])
      {
        int temp=buf[i];
        buf[i]=buf[j];
        buf[j]=temp;
      }
    }
  }
  for(int i=2;i<8;i++)  
  avgValue=buf[i];
  avgValue++;
   phValue=(avgValue*5.0)/1023.0; //convert the analog into millivolt
 phValue= 3.5*phValue + 2;
  static unsigned long analogSampleTimepoint = millis();
   if(millis()-analogSampleTimepoint > 30U)     //every 30 milliseconds,read the analog value from the ADC
   {
     analogSampleTimepoint = millis();
     analogBuffer[analogBufferIndex] = analogRead(DoSensorPin);    //read the analog value and store into the buffer
     analogBufferIndex++;
     if(analogBufferIndex == SCOUNT) 
         analogBufferIndex = 0;
   }
   
   static unsigned long tempSampleTimepoint = millis();
   if(millis()-tempSampleTimepoint > 500U)  // every 500 milliseconds, read the temperature
   {
      tempSampleTimepoint = millis();
      //temperature = readTemperature();  // add your temperature codes here to read the temperature, unit:^C
   }
   
   static unsigned long printTimepoint = millis();
   if(millis()-printTimepoint > 1000U)
   {
      printTimepoint = millis();
      for(copyIndex=0;copyIndex<SCOUNT;copyIndex++)
      {
        analogBufferTemp[copyIndex]= analogBuffer[copyIndex];
      }
      averageVoltage = getMedianNum(analogBufferTemp,SCOUNT) * (float)VREF / 1024.0; // read the value more stable by the median filtering algorith
     
  
    doValue = pgm_read_float_near( &SaturationValueTab[0] + (int)(SaturationDoTemperature+0.5) ) * averageVoltage / SaturationDoVoltage;
   
   }
   
   if(serialDataAvailable() > 0)
   {
      byte modeIndex = uartParse();  //parse the uart command received
      doCalibration(modeIndex);    // If the correct calibration command is received, the calibration function should be called.
   }
   
 
 lcd.setCursor(1,0);
 lcd.print("PH VALUE: ");
 lcd.print(phValue); 
 lcd.setCursor(1,1);
 lcd.print(F("DO Value:"));
 lcd.print(doValue,2);
 lcd.print(F("mg/L"));
 delay(2000);
 lcd.clear();
 delay(1000);
 if(phValue > 8.7 && phValue<=14 && doValue>=0 && doValue<5){
    Serial.print("PH VALUE: ");
    Serial.print(phValue); 
     Serial.println("");
    Serial.print(F("DO Value:"));
    Serial.print(doValue,2);
     Serial.println(F("mg/L"));
  
    Serial.println("Both are not in specified range, your pond is in danger,take action");
    
    delay(500);}
     else if(phValue >= 7.5 && phValue <= 8.7 && doValue>=0 && doValue<5){
    
   Serial.println("");
   Serial.print("PH VALUE: ");
    Serial.print(phValue);
    Serial.println(""); 
    Serial.print(F("DO Value:"));
    Serial.print(doValue,2);
   Serial.println(F("mg/L"));
    
    Serial.println("DO is not in required range,your pond is in danger,take action" );
    delay(500);
     }
     else if(phValue < 7.5 && phValue >=0 && doValue>=0 && doValue<5){
    
    Serial.print("PH VALUE: ");
    Serial.print(phValue); 
    Serial.println("");
    Serial.print(F("DO Value:"));
    Serial.print(doValue,2);
    Serial.println(F("mg/L"));
    Serial.println("Both are not in specified range, your pond is in danger,take action");
    Serial.println("");
    delay(500);
 }

 
else if (phValue > 8.7 && phValue<=14 && doValue>=5 && doValue<=7){
   Serial.print("PH VALUE: ");
    Serial.print(phValue); 
     Serial.println("");
    Serial.print(F("DO Value:"));
      Serial.print(doValue,2);
      Serial.println(F("mg/L"));
  
    Serial.println("pH is not in specified range, your pond is in danger, take action");
   delay(500); 
}

else if (phValue >= 7.5 && phValue <= 8.7 && doValue>=5 && doValue<=7){
   Serial.print("PH VALUE: ");
    Serial.print(phValue); 
     Serial.println("");
    Serial.print(F("DO Value:"));
     Serial.print(doValue,2);
      Serial.println(F("mg/L"));
  
    Serial.println("Both are in specified range, your pond is safe.");
    delay(500);
}

else if (phValue>= 0 && phValue< 7.5 && doValue>=5 && doValue<=7){
   Serial.print("PH VALUE: ");
    Serial.print(phValue); 
     Serial.println("");
    Serial.print(F("DO Value:"));
      Serial.print(doValue,2);
      Serial.println(F("mg/L"));
  
    Serial.println("pH is not in specified range, your pond is in danger, take action.");
     delay(500);
}



else if (phValue > 8.7 && phValue<=14 && doValue>7 && doValue<=15){
   Serial.print("PH VALUE: ");
    Serial.print(phValue); 
     Serial.println("");
    Serial.print(F("DO Value:"));
      Serial.print(doValue,2);
      Serial.println(F("mg/L"));
  
    Serial.println("Both are not in specified range, your pond is in danger, take action.");
     delay(500);
     }

else if (phValue >=7.5 && phValue <= 8.7 && doValue>7 && doValue<=15){
   Serial.print("PH VALUE: ");
    Serial.print(phValue); 
     Serial.println("");
    Serial.print(F("DO Value:"));
     Serial.print(doValue,2);
      Serial.println(F("mg/L"));
   
    Serial.println("DO is not in specified range, your pond is in danger, take action.");
 delay(500);
 }

else if (phValue >=0 && phValue< 7.5 && doValue>7 && doValue<=15){
   Serial.print("PH VALUE: ");
    Serial.print(phValue); 
     Serial.println("");
    Serial.print(F("DO Value:"));
      Serial.print(doValue,2);
      Serial.println(F("mg/L"));
  
    Serial.println("Both are not in specified range, your pond is in danger, take action.");
 delay(500);
 }  
    if (a<100)
  {
     mySerial.println("AT+CMGF=1");    //Sets the GSM Module in Text Mode
     delay(500);  // Delay of 1 second
     mySerial.println("AT+CMGS=\"+918208592978\"\r"); // Replace x with mobile number
     delay(500);
     if(phValue > 8.7 && phValue<=14 && doValue>=0 && doValue<5){
    mySerial.print("PH VALUE: ");
    mySerial.print(phValue); 
     mySerial.println("");
    mySerial.print(F("DO Value:"));
      mySerial.print(doValue,2);
      mySerial.println(F("mg/L"));
  
    mySerial.println("Both are not in specified range, your pond is in danger,take action");
    mySerial.println((char)26);
    delay(500);}
     else if(phValue >= 7.5 && phValue <= 8.7 && doValue>=0 && doValue<5){
    
   mySerial.println("");
   mySerial.print("PH VALUE: ");
    mySerial.print(phValue);
    mySerial.println(""); 
    mySerial.print(F("DO Value:"));
    mySerial.print(doValue,2);
   mySerial.println(F("mg/L"));
    
    mySerial.println("DO is not in required range,your pond is in danger,take action" );
    mySerial.println((char)26);// ASCII code of CTRL+Z for saying the end of sms to  the module 
     }
     else if(phValue < 7.5 && phValue >=0 && doValue>=0 && doValue<5){
    
    mySerial.print("PH VALUE: ");
    mySerial.print(phValue); 
    mySerial.println("");
    mySerial.print(F("DO Value:"));
    mySerial.print(doValue,2);
    mySerial.println(F("mg/L"));
    mySerial.println("Both are not in specified range, your pond is in danger,take action");
    mySerial.println("");
    mySerial.println((char)26);
 }

 
else if (phValue > 8.7 && phValue<=14 && doValue>=5 && doValue<=7){
   mySerial.print("PH VALUE: ");
    mySerial.print(phValue); 
     mySerial.println("");
    mySerial.print(F("DO Value:"));
      mySerial.print(doValue,2);
      mySerial.println(F("mg/L"));
  
    mySerial.println("pH is not in specified range, your pond is in danger, take action");
    
}

else if (phValue >= 7.5 && phValue <= 8.7 && doValue>=5 && doValue<=7){
   mySerial.print("PH VALUE: ");
    mySerial.print(phValue); 
     mySerial.println("");
    mySerial.print(F("DO Value:"));
      mySerial.print(doValue,2);
      mySerial.println(F("mg/L"));
  
    mySerial.println("Both are in specified range, your pond is safe.");
}

else if (phValue>= 0 && phValue< 7.5 && doValue>=5 && doValue<=7){
   mySerial.print("PH VALUE: ");
    mySerial.print(phValue); 
     mySerial.println("");
    mySerial.print(F("DO Value:"));
      mySerial.print(doValue,2);
      mySerial.println(F("mg/L"));
  
    mySerial.println("pH is not in specified range, your pond is in danger, take action.");
}



else if (phValue > 8.7 && phValue<=14 && doValue>7 && doValue<=15){
   mySerial.print("PH VALUE: ");
    mySerial.print(phValue); 
     mySerial.println("");
    mySerial.print(F("DO Value:"));
      mySerial.print(doValue,2);
      mySerial.println(F("mg/L"));
  
    mySerial.println("Both are not in specified range, your pond is in danger, take action.");
}

else if (phValue >=7.5 && phValue <= 8.7 && doValue>7 && doValue<=15){
   mySerial.print("PH VALUE: ");
    mySerial.print(phValue); 
     mySerial.println("");
    mySerial.print(F("DO Value:"));
      mySerial.print(doValue,2);
      mySerial.println(F("mg/L"));
   
    mySerial.println("DO is not in specified range, your pond is in danger, take action.");
}

else if (phValue >=0 && phValue< 7.5 && doValue>7 && doValue<=15){
   mySerial.print("PH VALUE: ");
    mySerial.print(phValue); 
     mySerial.println("");
    mySerial.print(F("DO Value:"));
      mySerial.print(doValue,2);
      mySerial.println(F("mg/L"));
  
    mySerial.println("Both are not in specified range, your pond is in danger, take action.");
}

  delay(900000);
   
a=a+1;
  }
}
  
}
     

boolean serialDataAvailable(void)
{
  char receivedChar;
  static unsigned long receivedTimeOut = millis();
  while ( Serial.available() > 0 ) 
  {   
    if (millis() - receivedTimeOut > 500U) 
    {
      receivedBufferIndex = 0;
      memset(receivedBuffer,0,(ReceivedBufferLength+1));
    }
    receivedTimeOut = millis();
    receivedChar = Serial.read();
    if (receivedChar == '\n' || receivedBufferIndex == ReceivedBufferLength)
    {
  receivedBufferIndex = 0;
  strupr(receivedBuffer);
  return true;
    }else{
        receivedBuffer[receivedBufferIndex] = receivedChar;
        receivedBufferIndex++;
    }
  }
  return false;
}

byte uartParse()
{
    byte modeIndex = 0;
    if(strstr(receivedBuffer, "CALIBRATION") != NULL) 
        modeIndex = 1;
    else if(strstr(receivedBuffer, "EXIT") != NULL) 
        modeIndex = 3;
    else if(strstr(receivedBuffer, "SATCAL") != NULL)   
        modeIndex = 2;
    return modeIndex;
}

void doCalibration(byte mode)
{
    char *receivedBufferPtr;
    static boolean doCalibrationFinishFlag = 0,enterCalibrationFlag = 0;
    float voltageValueStore;
    switch(mode)
    {
      case 0:
      if(enterCalibrationFlag)
         Serial.println(F("Command Error"));
      break;
      
      case 1:
      enterCalibrationFlag = 1;
      doCalibrationFinishFlag = 0;
      Serial.println();
      Serial.println(F(">>>Enter Calibration Mode<<<"));
      Serial.println(F(">>>Please put the probe into the saturation oxygen water! <<<"));
      Serial.println();
      break;
     
     case 2:
      if(enterCalibrationFlag)
      {
         Serial.println();
         Serial.println(F(">>>Saturation Calibration Finish!<<<"));
         Serial.println();
         EEPROM_write(SaturationDoVoltageAddress, averageVoltage);
         EEPROM_write(SaturationDoTemperatureAddress, temperature);
         SaturationDoVoltage = averageVoltage;
         SaturationDoTemperature = temperature;
         doCalibrationFinishFlag = 1;
      }
      break;

        case 3:
        if(enterCalibrationFlag)
        {
            Serial.println();
            if(doCalibrationFinishFlag)      
               Serial.print(F(">>>Calibration Successful"));
            else 
              Serial.print(F(">>>Calibration Failed"));       
            Serial.println(F(",Exit Calibration Mode<<<"));
            Serial.println();
            doCalibrationFinishFlag = 0;
            enterCalibrationFlag = 0;
        }
        break;
    }
}

int getMedianNum(int bArray[], int iFilterLen) 
{
      int bTab[iFilterLen];
      for (byte i = 0; i<iFilterLen; i++)
      {
    bTab[i] = bArray[i];
      }
      int i, j, bTemp;
      for (j = 0; j < iFilterLen - 1; j++) 
      {
    for (i = 0; i < iFilterLen - j - 1; i++) 
          {
      if (bTab[i] > bTab[i + 1]) 
            {
    bTemp = bTab[i];
          bTab[i] = bTab[i + 1];
    bTab[i + 1] = bTemp;
       }
    }
      }
      if ((iFilterLen & 1) > 0)
  bTemp = bTab[(iFilterLen - 1) / 2];
      else
  bTemp = (bTab[iFilterLen / 2] + bTab[iFilterLen / 2 - 1]) / 2;
      return bTemp;
}

void readDoCharacteristicValues(void)
{
    EEPROM_read(SaturationDoVoltageAddress, SaturationDoVoltage);  
    EEPROM_read(SaturationDoTemperatureAddress, SaturationDoTemperature);
    if(EEPROM.read(SaturationDoVoltageAddress)==0xFF && EEPROM.read(SaturationDoVoltageAddress+1)==0xFF && EEPROM.read(SaturationDoVoltageAddress+2)==0xFF && EEPROM.read(SaturationDoVoltageAddress+3)==0xFF)
    {
      SaturationDoVoltage = 1127.6;   //default voltage:1127.6mv
      EEPROM_write(SaturationDoVoltageAddress, SaturationDoVoltage);
    }
    if(EEPROM.read(SaturationDoTemperatureAddress)==0xFF && EEPROM.read(SaturationDoTemperatureAddress+1)==0xFF && EEPROM.read(SaturationDoTemperatureAddress+2)==0xFF && EEPROM.read(SaturationDoTemperatureAddress+3)==0xFF)
    {
      SaturationDoTemperature = 20.0;   //default temperature is 24^C
      EEPROM_write(SaturationDoTemperatureAddress, SaturationDoTemperature);
    }    

}


 






