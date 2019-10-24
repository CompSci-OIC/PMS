//
// PMS Collector
// Collects data from a set of sensors
// Services requests for data and configuration changes from the PMS supervisor
// One channel per sensor, only one channel is in use at any one time
//
// 29/06/19 NM created v1.3 from ChannelDemo v1.2
// added default values for attributes in Channel() constructor
// replaced strcmp() with stricmp()
// added consts for MODE_CAL and MODE_RAW
// revised request parsing
// 01/07/19
// testing use of tone() to replace IR library (conditional compilation)


#include <Arduino.h>

//Below line imports the header file that defines the channel class
#include "channel.h"

const char *versionStr = "v1.3";

//#define USE_IR_LIB 1

//Library for IR light gate
#ifdef USE_IR_LIB
#include <IRremote.h>
#endif

// pins for IR sensor
#define PIN_IR 3
#define PIN_DETECT 4
// state values for FSM for IR sensor
#define LFL 0
#define LOWF 1
#define LFH 2
#define HIGHF 3


// #defines for conditional compilation
// see also #define USE_IR_LIB 1 earlier in this file 
//#define PARSE_DBG 1
//#define TESTING_DBG 1


//Pins for voltage sensor
const int voltagePin = A0;

//Pins for ultrasonic sensor
const int trigPin = 10;
const int echoPin = 9;

//Declarations for light gate
const int INTERVAL_ARRAY_SIZE = 10;
unsigned long intervalArray[INTERVAL_ARRAY_SIZE] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
const char *stateStr[4] = {"LFL", "LOWF", "LFH", "HIGHF"};
const long debounceInterval = 10L; //bounce in ms
int state, intervalIndex = 0;
//times at which a HIGH to LOW transition has been detected in ms
unsigned long previousHighLow, intervalHighLow, startTime;

#ifdef USE_IR_LIB
IRsend irsend;
#endif


const float NO_DATA = -1;
const int BAUD = 9600;
const int MAX_CHANNELS = 3;

const int maxTempStrLen = 16;
const int maxUnitStrLen = 10; //Includes null terminator as a char
const int maxReqStrLen = 30;

const char *calUnitSettings[] = {"V", "mm", "ms"};
const char *rawUnitSettings[] = {"counts", "μs", "ms"};
const char *modeStr[] = {"CAL", "RAW"};

int currentChan; //Currently selected channel number
char request[maxReqStrLen]; //Buffer for incoming requests
int requestIndex; //Index for request buffer
int currentCollectMode = MODE_CAL;
int indexValue; //Index of the current data value being given out
unsigned long previousMillis = 0;        // will store last time LED was updated
bool outputEnabled; //Flag to show if output is sent to laptop or not

float voltageSensor(unsigned long interval);//Prototype, telling the compiler what getInput does
float ultrasonicSensor(unsigned long interval);
void ultrasonicSetup(void);

Channel channelSet[MAX_CHANNELS] = {Channel(), Channel(), Channel()}; // Creates channel object

void setup() {
  requestIndex = 0;
  currentChan = 0;
  outputEnabled = false;
  //Set up sensors for each channel
  voltageSensorSetup();
  ultrasonicSetup();
  pinMode(LED_BUILTIN, OUTPUT);  // use on-board LED

  //Start of setup for IR light gate
  pinMode(PIN_DETECT, INPUT);
  // IR LED output is modulated at 38kHz to reduce interference from ambient IR radiation
#ifdef USE_IR_LIB
  irsend.enableIROut(38); // this is a 30% duty cycle
  irsend.mark(0);
#else
  tone(PIN_IR, 38000); // this is a 50% duty cycle
#endif
  state = LOWF;
  startTime = 0;
  //End of setup for light gate

  // initialize the serial communication:
  Serial.begin(BAUD);
  delay(500); // to let the serial port initialise correctly
#ifdef TESTING_DBG
  Serial.println("PMS Collector Class Demo ");
#endif
  for (int chan = 0; chan < MAX_CHANNELS; chan++) {
    channelSet[chan].setUnits(calUnitSettings[chan]);
  }
}

void voltageSensorSetup() {
  pinMode(voltageSensor, INPUT);
}

void  ultrasonicSetup() {
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
}

void loop() {
  // 1) Read sensors
  readSensors(currentChan);
  // 2) Service serial port
  char c;
  if (Serial.available() > 0) {
    c = Serial.read();
    if ( c == '\n' ) { // some systems will terminate with LF ('\n'), others with CR ('\r'), others with CR-LF (\r\n)
      request[requestIndex] = '\0'; //Replacing the terminator with \0 so strtok knows when to stop reading as it has reached the end of the string
      //Every C string is an array of characters, terminated with the \0
#ifdef TESTING_DBG
      Serial.print("Received request "); Serial.println(request);
#endif
      processRequest(request);
      requestIndex = 0;
    }
    else {
      request[requestIndex] = c;
      requestIndex++;
    }
  }
  // 3) service IR Light gate
  stateTransition();
}

void processRequest(char *request) {
  char cmd[maxReqStrLen]; // copy of the original request used in the ACK
  char response[maxReqStrLen + 20];
  char units[maxUnitStrLen];
  char temp[maxTempStrLen];
  bool recognised = false;
  const char *delimiterList = " \r\n";
  const int MAX_TOKENS = 10;
  char *token[MAX_TOKENS];
  int numTokens = 0;

  strcpy(cmd, request); //Copies request into a new character array called cmd for use in the ack

#ifdef PARSE_DBG
  Serial.print("request is >"); Serial.print(request); Serial.println("<");
#endif

  // split the command into delimited tokens
  token[0] = strtok( request, delimiterList);
#ifdef PARSE_DBG
  Serial.print(numTokens); Serial.print(" >"); Serial.print(token[numTokens]); Serial.println("<  ");
#endif
  while ( (token[numTokens] != NULL) && (numTokens < MAX_TOKENS - 1) )
  {
    token[++numTokens] = strtok( NULL, delimiterList );
#ifdef PARSE_DBG
    Serial.print(numTokens); Serial.print(" >"); Serial.print(token[numTokens]); Serial.println("<  ");
#endif
  }

#ifdef PARSE_DBG
  Serial.print("num tokens "); Serial.println(numTokens);
#endif
  if (numTokens != 0) {

    if (stricmp(token[0], "SET") == 0) {
      if (stricmp(token[1], "CHAN") == 0) {
        currentChan = atoi(token[2]);
        strcpy(response, "ACK ");
        strcat(response, cmd);
        recognised = true;
      } else if (stricmp(token[1], "INTERVAL") == 0) {
        channelSet[currentChan].setSampleInterval(atoi(token[2]));
        strcpy(response, "ACK ");
        strcat(response, cmd);
        recognised = true;
      } else if (stricmp(token[1], "SAMPLES") == 0) {
        channelSet[currentChan].setSamplesToTake(atoi(token[2]));
        strcpy(response, "ACK ");
        strcat(response, cmd);
        recognised = true;
      } else if (stricmp(token[1], "MODE") == 0) {
        int arraySize = (sizeof(modeStr) / sizeof(char*));
        for (int i = 0; i < arraySize; i++) {
          if (stricmp(token[2], modeStr[i]) == 0) {
            channelSet[currentChan].setMode(i);
            strcpy(response, "ACK ");
            strcat(response, cmd);
          }
        }
        recognised = true;
      } else {
        //  token[1] unknown
#ifdef PARSE_DBG
        Serial.println("SET: Token 1 not recognised");
#endif
        recognised = false;
      }
    } else if (stricmp(token[0], "GET") == 0) {
      if (stricmp(token[1], "CHAN") == 0) {
        strcpy(response, "ACK ");
        strcat(response, cmd);
        strcat(response, " ");
        itoa(currentChan, temp, 10);
        strcat(response, temp);
        recognised = true;
      } else if (stricmp(token[1], "INTERVAL") == 0) {
        strcpy(response, "ACK ");
        strcat(response, cmd);
        strcat(response, " ");
        itoa(channelSet[currentChan].getSampleInterval(), temp, 10);
        strcat(response, temp);
        recognised = true;
      } else if (stricmp(token[1], "BOARD") == 0) {
        strcpy(response, "ACK ");
        strcat(response, cmd);
        strcat(response, " ");
        strcat(response, versionStr);
        strcat(response, " ");
        itoa(MAX_CHANNELS, temp, 10);
        strcat(response, temp);
        recognised = true;
      } else if (stricmp(token[1], "UNIT") == 0) {
        strcpy(units, channelSet[currentChan].getUnits());
        strcpy(response, "ACK ");
        strcat(response, cmd);
        strcat(response, " ");
        strcat(response, units);
        recognised = true;
      } else if (stricmp(token[1], "SAMPLES") == 0) {
        strcpy(response, "ACK ");
        strcat(response, cmd);
        strcat(response, " ");
        itoa(channelSet[currentChan].getSamplesToTake(), temp, 10);
        strcat(response, temp);
        recognised = true;
      } else if (stricmp(token[1], "MODE") == 0) {
        strcpy(response, "ACK ");
        strcat(response, cmd);
        strcat(response, " ");
        int modeNum = channelSet[currentChan].getMode();
        strcat(response, modeStr[modeNum]);
        recognised = true;
      } else if (stricmp(token[1], "ALL") == 0) {
        //Insert code to do all here.
        strcpy(response, "ACK ");
        strcat(response, cmd);
        strcat(response, " ");
        itoa(currentChan, temp, 10);
        strcat(response, temp);
        strcat(response, " ");
        itoa(channelSet[currentChan].getSampleInterval(), temp, 10);
        strcat(response, temp);
        strcat(response, " ");
        itoa(channelSet[currentChan].getSamplesToTake(), temp, 10);
        strcat(response, temp);
        strcat(response, " ");
        strcat(response, channelSet[currentChan].getUnits());
        recognised = true;
      } else {
#ifdef PARSE_DBG
        Serial.println("GET: Token 1 not recognised");
#endif
        recognised = false;
      }
    } else if (stricmp(token[0], "START") == 0) {
      strcpy(response, "ACK START");
      indexValue = 0;
      outputEnabled = true;
      recognised = true;
    } else if (stricmp(token[0], "STOP") == 0) {
      strcpy(response, "ACK STOP");
      outputEnabled = false;
      recognised = true;
    } else {
      // token[0] not known
      recognised = false;
#ifdef PARSE_DBG
      Serial.println("Token 0 not recognised");
#endif
    }
  } else {
    // no tokens
    recognised = false;
#ifdef PARSE_DBG
    Serial.println("no tokens recognised");
#endif
  }
  // send the response
  if (recognised == true) {
    Serial.println(response);
  } else {
    Serial.print("NAK BAD REQUEST "); Serial.println(cmd);
  }
}


void readSensors(int currentChan) {
  float value;
  char temp[maxTempStrLen];
  char response[60];
  switch (currentChan) {
    case 0:
      value = voltageSensor(channelSet[currentChan].getSampleInterval());
      break;
    case 1:
      value = ultrasonicSensor(channelSet[currentChan].getSampleInterval());
      break;
    case 2:
      value = readLightGate(channelSet[currentChan].getSampleInterval());
      break;
    default:
#ifdef TESTING_DBG
      Serial.println("Unexpected Channel");
#endif
      break;
  }
  if (outputEnabled and value != NO_DATA) {
    strcpy(response, "DATA ");
    strcat(response, itoa(indexValue, temp, 10));
    strcat(response, " ");
    ftoa(value, temp, 3); //Returns value as a string to 3d.p.
    strcat(response, temp);
    Serial.println(response);
    indexValue++;
    if (indexValue == channelSet[currentChan].getSamplesToTake()) {
      outputEnabled = false;
    }
  }
}


float voltageSensor(unsigned long interval) {  //Function to collect data from the voltage sensor, checks if it is time to collect the next data point

  float value = NO_DATA;
  float voltageValue;
  int currentCollectMode;
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    float reading = analogRead(voltagePin);
    currentCollectMode = channelSet[currentChan].getMode();
    if (currentCollectMode == MODE_CAL) {
      voltageValue = (5 * ((reading + 1) / 1024));
      value = voltageValue;
    }
    if (currentCollectMode == MODE_RAW) {
      value = reading;
    }
  }
  return (value);
}

//Reads a distance value from ultrasonic sensor, if it is time to do so
//This function should be called from loop
float ultrasonicSensor(unsigned long interval) {
  long duration;
  float distance = NO_DATA; //The distance is converted from microsec to mm
  unsigned long currentMillis = millis();
  int currentCollectMode;
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(30);
    digitalWrite(trigPin, LOW);
    // Reads the echoPin, returns the sound wave travel time in microseconds
    duration = pulseIn(echoPin, HIGH);
    currentCollectMode = channelSet[currentChan].getMode();
    if (currentCollectMode == MODE_CAL) {
      distance = duration * 0.1715;
    }
    if (currentCollectMode == MODE_RAW) {
      distance = duration;
    }
  }
  return (distance);
}

// read the most recent interval value from the light gate sensor
// this is based on a 10 point moving average of sensor reading
float readLightGate(unsigned long interval) {
  float value = NO_DATA;
  int currentCollectMode;
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    float reading = tenPtAverage();
    currentCollectMode = channelSet[currentChan].getMode();
    if (currentCollectMode == MODE_CAL) {
      value = reading;
    }
    if (currentCollectMode == MODE_RAW) {
      value = reading;
    }
  }
  return (value);
}

// Finite stae machine to determine the interval between high to low transitions
// on the light gate IR sensor.
// The interval is added to intervalArray[] from which a separate function calculates
// a 10 point moving average.
void stateTransition() {  // calculate time period of the pendulum
  int startState = state, reading;
  switch (state) {
    case LFL: //look for LOW
      reading = digitalRead(PIN_DETECT);
      if (reading == LOW) {
        state = LOWF;
        startTime = millis(); //time at which LOW is first detected
      }
      break;

    case LOWF: //LOW found
      reading = digitalRead(PIN_DETECT);
      if (reading == LOW) {
        if ( (millis() - startTime) > debounceInterval) {  //no longer in bounce
          //          Serial.print("current milli: ");
          //          Serial.println(millis());
          //          Serial.print("startTime: ");
          //          Serial.println(startTime);
          state = LFH;

          intervalHighLow = millis() - previousHighLow; //time at which LOW is first detected
          previousHighLow = millis();

          intervalArray[intervalIndex] = intervalHighLow;
          intervalIndex++;
          if (intervalIndex > INTERVAL_ARRAY_SIZE - 1) {
            intervalIndex = 0;
          }
          digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));  // toggle LED state for every interval measurement
#ifdef TESTING_DBG
          Serial.print(" ");
          Serial.print("intervalHighLow  ");
          Serial.print(intervalHighLow);
          Serial.print(" average is ");
          Serial.println(tenPtAverage());
          //          Serial.print("previousHighLow ");
          //          Serial.println(previousHighLow);

#endif
        }
      } else {  //bounce exists
        state = LFL;
      }

      break;

    case LFH: //look for HIGH
      reading = digitalRead(PIN_DETECT);
      if (reading == HIGH) {
        state = HIGHF;
        startTime = millis(); //time at which HIGH is first detected
      }
      break;

    case HIGHF: //HIGH found
      reading = digitalRead(PIN_DETECT);
      if (reading == HIGH) {
        if ( (millis() - startTime) > debounceInterval) { //no longer in bounce
          state = LFL;
          //          Serial.print("current milli: ");
          //          Serial.println(millis());
          //          Serial.print("startTime: ");
          //          Serial.println(startTime);
        }
      } else { //bounce exists
        state = LFH;
      }
      break;

    default:
#ifdef TESTING_DBG
      Serial.println("STATE ERROR");
#endif
      break;
  }
  if (state != startState) {
#ifdef TESTING_DBG
    Serial.print(millis());
    Serial.print(" ");
    Serial.print(stateStr[startState]);
    Serial.print(" -> ");
    Serial.print(stateStr[state]);
    Serial.print(" ");
#endif


  }
}


// calculate a moving ten point average
// from the non zero data values in intervaArray[]
long tenPtAverage () {
  int count = 0;
  long sum = 0;
  for (int i = 0 ; i < INTERVAL_ARRAY_SIZE; i++) {
    if (intervalArray[i] != 0) {
      sum += intervalArray[i];
      count++;
    }
  }
  if (sum == 0) {
    return (0);
  } else {
    return (sum / count);
  }
}





//Functions to change float to string
void reverse(char *str, int len)
{
  int i = 0, j = len - 1, temp;
  while (i < j)
  {
    temp = str[i];
    str[i] = str[j];
    str[j] = temp;
    i++; j--;
  }
}

int intToStr(int x, char str[], int d)
{
  int i = 0;
  while (x)
  {
    str[i++] = (x % 10) + '0';
    x = x / 10;
  }

  // If number of digits required is more, then
  // add 0s at the beginning
  while (i < d)
    str[i++] = '0';

  reverse(str, i);
  str[i] = '\0';
  return i;
}

void ftoa(float n, char *res, int afterpoint)
{
  // Extract integer part
  int ipart = (int)n;

  // Extract floating part
  float fpart = n - (float)ipart;

  // convert integer part to string
  int i = intToStr(ipart, res, 0);

  // check for display option after point
  if (afterpoint != 0)
  {
    res[i] = '.';  // add dot

    // Get the value of fraction part upto given no.
    // of points after dot. The third parameter is needed
    // to handle cases like 233.007
    fpart = fpart * pow(10, afterpoint);

    intToStr((int)fpart, res + i + 1, afterpoint);
  }
}

// a replacement for strcmp() which is not case sensitive
int stricmp (const char *s1, const char *s2) {
  while (*s1 != 0 && *s2 != 0) {
    if (*s1 != *s2 && ::toupper (*s1) != ::toupper(*s2)) {
      return -1;
    }
    s1++;
    s2++;
  }
  return (*s1 == 0 && *s2 == 0) ? 0 : -1;
}
