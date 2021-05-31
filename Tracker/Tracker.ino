#include <SoftwareSerial.h>
#include <TinyGPS++.h>

#define gpsTX 4
#define gpsRX 5
#define smsTX 2
#define smsRX 0

//const int ALARM = 6;
//const int POWER = 7;

SoftwareSerial sms( smsTX, smsRX );
SoftwareSerial  ss( gpsTX, gpsRX );
TinyGPSPlus gps;
String gpsStat;


String senderNumber;  // the container for reciever's number
String senderMessage; // the container for reciever's message content
int lastReceivedSMSId = 0; // id of the message
char buffer[80]; // length of gsm module message
byte pos = 0; 

enum _parseState
{
  PS_DETECT_MSG_TYPE,

  PS_IGNORING_COMMAND_ECHO,

  PS_READ_CMTI_STORAGE_TYPE,
  PS_READ_CMTI_ID,

  PS_READ_CMGR_STATUS,
  PS_READ_CMGR_NUMBER,
  PS_READ_CMGR_SOMETHING,
  PS_READ_CMGR_DATE,
  PS_READ_CMGR_CONTENT
};


byte state = PS_DETECT_MSG_TYPE;

void resetBuffer()
{
  memset(buffer, 0, sizeof(buffer));
  pos = 0;
}

void setup() {
  //pinMode( ALARM, OUTPUT );
  //pinMode( POWER, OUTPUT );

  Serial.begin(9600);
  sms.begin(9600);
  ss.begin(9600);
}

void loop() 
{
  if(sms.available())
  {
    parseATText(sms.read());
  }
  else
  { 
    if ( ss.available() > 0 )
    {
      if ( gps.encode(Serial.read()) )
      {
        if ( gps.location.isValid() ) // If GPS signal is available to area
        {
          gpsStat = "GPS_AVAILABLE";
          long gpsLatt = gps.location.lat() * 10000; // get your current lattitude location and multiply it my 10k
          long gpsLong = gps.location.lng() * 10000; // get your current longitude location and multiply it my 10k

          // this is your exact and current location
          Serial.print("YOUR LOCATION: ");
          Serial.print(gpsLatt); // your current lattitude location
          Serial.print(", ");
          Serial.print(gpsLong); // your current longitude location
          Serial.println();
        }
        else
        {
          // if no GPS signal available this will be appear
          // green led is OFF, red led is ON, the door unlock
          gpsStat = "GPS_NOT_AVAILABLE";
          Serial.println("GPS NOT AVAILABLE");
        }
      }
    }
    else
    {
      // GPS Wire Connection Problem
      gpsStat = "GPS_PLUG_OUT";
      Serial.println("GPS PLUG OUT");
      delay(500);
    }
  }
}

void parseATText(byte b) 
{
  buffer[pos++] = b;
  if ( pos >= sizeof(buffer) )
    resetBuffer(); // reseting buff just to be safe
    
  switch (state)
  {
    case PS_DETECT_MSG_TYPE:
      {
        if ( b == '\n' )
          resetBuffer();
        else
        {
          if ( pos == 3 && strcmp(buffer, "AT+") == 0 )
          {
            state = PS_IGNORING_COMMAND_ECHO;
          }
          else if ( pos == 6 )
          {
            if ( strcmp(buffer, "+CMTI:") == 0 )
            {
              state = PS_READ_CMTI_STORAGE_TYPE;
            }
            else if ( strcmp(buffer, "+CMGR:") == 0 )
            {
              state = PS_READ_CMGR_STATUS;
            }
            resetBuffer();
          }
        }
      }
    break;

    case PS_IGNORING_COMMAND_ECHO:
      {
        if ( b == '\n' )
        {
          state = PS_DETECT_MSG_TYPE;
          resetBuffer();
        }
      }
    break;

    case PS_READ_CMTI_STORAGE_TYPE:
      {
        if ( b == ',' )
        {
          state = PS_READ_CMTI_ID;
          resetBuffer();
        }
      }
    break;

    case PS_READ_CMTI_ID:
      {
        if ( b == '\n' )
        {
          lastReceivedSMSId = atoi(buffer);
          Serial.print("SMS ID: ");
          Serial.println(lastReceivedSMSId);

          Serial.print("AT+CMGR=");
          Serial.println(lastReceivedSMSId);
          //delay(500); don't do this!

          state = PS_DETECT_MSG_TYPE;
          resetBuffer();
        }
      }
    break;
      
    case PS_READ_CMGR_STATUS:
      {
        if ( b == ',' )
        {
          state = PS_READ_CMGR_NUMBER;
          resetBuffer();
        }
      }
    break;

    case PS_READ_CMGR_NUMBER:
      {
        if ( b == ',' )
        {
          senderNumber = String(buffer); 
          senderNumber = senderNumber.substring(1, senderNumber.length() - 2);

          state = PS_READ_CMGR_SOMETHING;
          resetBuffer();
        }
      }
    break;

    case PS_READ_CMGR_SOMETHING:
      {
        if ( b == ',' )
        {
          state = PS_READ_CMGR_DATE;
          resetBuffer();
        }
      }
    break;
    
    case PS_READ_CMGR_DATE:
      {
        if ( b == '\n' )
        {
          state = PS_READ_CMGR_CONTENT;
          resetBuffer();
        }
      }
    break;
    
    case PS_READ_CMGR_CONTENT:
      {
        if ( b == '\n' )
        {
          senderMessage = String(buffer);
          senderMessage = senderMessage.substring(0, senderMessage.length() - 1 );
          
          Serial.print("From: ");
          Serial.println(senderNumber);

          Serial.print("Message: ");
          Serial.println(senderMessage);
          
          if ( senderMessage == "rqloc" ) // Request Location
          {
            if( gpsStat == "GPS_AVAILABLE" )
            {
              Serial.println("REQUEST RECIEVED");
              sendCurrentLocation( senderNumber );
            }
            else
            {
              Serial.println("REQUEST RECIEVED");
              sendGPSstat( senderNumber );
            }
          }

          if ( senderMessage == "rqloc" ) // Request Location
          {
            if( gpsStat == "GPS_AVAILABLE" )
            {
              Serial.println("REQUEST RECIEVED");
              sendCurrentLocation( senderNumber );
            }
            else
            {
              Serial.println("REQUEST RECIEVED");
              sendGPSstat( senderNumber );
            }
          }
          
          else
          {
            Serial.print("INVALID REQUEST: ");
            sendQueryError( senderNumber );
          }

          Serial.print("AT+CMGD=");
          Serial.println(lastReceivedSMSId);
          //delay(500); don't do this!

          state = PS_DETECT_MSG_TYPE;
          resetBuffer();
        }
      }
    break;
  }
}

void sendQueryError( String senderNum )
{
  Serial.println("AT+CMGF=1"); // set gsm to text mode
  delay(1000);

  Serial.println("AT+CMGS=\"" + senderNum + "\"");
  delay(500);

  Serial.println("QUERY ERROR: INVALID REQUEST");
  delay(100);

  Serial.println((char)26); // ASCII code of CTRL+Z
  delay(1000);

  Serial.println();
  delay(100);
}

void sendCurrentLocation( String senderNum )
{
  Serial.println("AT+CMGF=1"); // set gsm to text mode
  delay(1000);

  Serial.println("AT+CMGS=\"" + senderNum + "\"");
  delay(500);

  Serial.println("LOCATION: " + String(gps.location.lat() * 10000) +", " + String(gps.location.lat() * 10000));
  delay(100);

  Serial.println((char)26); // ASCII code of CTRL+Z
  delay(1000);

  Serial.println();
  delay(100);
}

void sendGPSstat( String senderNum ){
  Serial.println("AT+CMGF=1"); // set gsm to text mode
  delay(1000);

  Serial.println("AT+CMGS=\"" + senderNum + "\"");
  delay(500);

  Serial.println("GPS ERROR: " + gpsStat);
  delay(100);

  Serial.println((char)26); // ASCII code of CTRL+Z
  delay(1000);

  Serial.println();
  delay(100);
}
