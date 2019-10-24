#include <Arduino.h>

//Below line imports the header file that defines the channel class
#include "channel.h"

//Library for IR light gate
#include <IRremote.h>

#define PIN_IR 3
#define PIN_DETECT 4
// state values for FSM
#define LFL 0
#define LOWF 1
#define LFH 2
#define HIGHF 3


//Pins for voltage sensor
const int voltagePin = A0;

//Pins for ultrasonic sensor
const int trigPin = 10;
const int echoPin = 9;

//Declarations for light gate
const int INTERVALARRAYSIZE = 10;
unsigned long intervalArray[INTERVALARRAYSIZE] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
const char *stateStr[4] = {"LFL", "LOWF", "LFH", "HIGHF"};
const long debounceInterval = 10L; //bounce in ms
int state, intervalIndex = 0;
//times at which a HIGH to LOW transition has been detected in ms
unsigned long previousHighLow, intervalHighLow, startTime;
IRsend irsend;


const float NO_DATA = -1;
const int BAUD = 9600;
const int MAX_CHANNELS = 3;
const char *versionStr = "v1.2";
const int maxTempStrLen = 16;
const int maxUnitStrLen = 10; //Includes null terminator as a char
const char *calUnitSettings[] = {"V", "mm", "ms"};
const char *rawUnitSettings[] = {"counts", "μs", "ms"};
const char *modeStr[] = {"CAL", "RAW"};

int currentChan; //Currently selected channel number
char request[30]; //Buffer for incoming requests
int requestIndex; //Index for request buffer
int currentCollectMode = 0;
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
  irsend.enableIROut(38);
  irsend.mark(0);
  state = LOWF;
  startTime = 0;
  //End of setup for light gate

  // initialize the serial communication:
  Serial.begin(BAUD);
  delay(500); // to let the serial port initialise correctly
#ifdef XYZ
  Serial.println("Channel Class Demo ");
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
    if (c == '\n') {
      request[requestIndex] = '\0'; //Replacing the \n with \0 so strtok knows when to stop reading as it has reached the end of the string
      //Every c string is an array of characters, terminated with the \0
#ifdef XYZ
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
  char *token;
  char cmd[30];
  char response[50];
  char units[maxUnitStrLen];
  char temp[maxTempStrLen];
  bool recognised;
  recognised = false;
  strcpy(cmd, request); //Copies request into a new character array called cmd
  token = strtok(request, " ");
  if (token != NULL && strcmp(token, "SET") == 0) {
    token = strtok(NULL, " ");
    if (token != NULL && strcmp(token, "CHAN") == 0) {
      token = strtok(NULL, " ");
      currentChan = atoi(token);
      Serial.print("ACK "); Serial.println(cmd);
      recognised = true;
    }
    if (token != NULL && strcmp(token, "INTERVAL") == 0) {
      token = strtok(NULL, " ");
      channelSet[currentChan].setSampleInterval(atoi(token));
      Serial.print("ACK "); Serial.println(cmd);
      recognised = true;
    }
    if (token != NULL && strcmp(token, "SAMPLES") == 0) {
      token = strtok (NULL, " ");
      channelSet[currentChan].setSamplesToTake(atoi(token));
      Serial.print("ACK "); Serial.println(cmd);
      recognised = true;
    }
    if (token != NULL && strcmp(token, "MODE") == 0) {
      token = strtok (NULL, " ");
      int arraySize = (sizeof(modeStr) / sizeof(char*));
      for (int i = 0; i < arraySize; i++) {
        if (strcmp(token, modeStr[i]) == 0) {
          channelSet[currentChan].setMode(i);
          Serial.print("ACK "); Serial.println(cmd);
        }
      }
      recognised = true;
    }
  }
  if (token != NULL && strcmp(token, "GET") == 0) {
    token = strtok(NULL, " ");
    if (token != NULL && strcmp(token, "CHAN") == 0) {
      Serial.print("ACK "); Serial.print(cmd); Serial.print(" "); Serial.println(currentChan);
      recognised = true;
    }
    if (token != NULL && strcmp(token, "INTERVAL") == 0) {
      Serial.print("ACK "); Serial.print(cmd); Serial.print(" "); Serial.println(channelSet[currentChan].getSampleInterval());
      recognised = true;
    }
    if (token != NULL && strcmp(token, "BOARD") == 0) {
      strcpy(response, "ACK ");
      strcat(response, cmd);
      strcat(response, " ");
      strcat(response, versionStr);
      strcat(response, " ");
      itoa(MAX_CHANNELS, temp, 10);
      strcat(response, temp);
      Serial.println(response);
      recognised = true;
    }
    if (token != NULL && strcmp(token, "UNIT") == 0) {
      strcpy(units, channelSet[currentChan].getUnits());
      strcpy(response, "ACK ");
      strcat(response, cmd);
      strcat(response, " ");
      strcat(response, units);
      Serial.println(response);
      recognised = true;
    }
    if (token != NULL && strcmp(token, "SAMPLES") == 0) {
      strcpy(response, "ACK ");
      strcat(response, cmd);
      strcat(response, " ");
      itoa(channelSet[currentChan].getSamplesToTake(), temp, 10);
      strcat(response, temp);
      Serial.println(response);
      recognised = true;
    }
    if (token != NULL && strcmp(token, "MODE") == 0) {
      strcpy(response, "ACK ");
      strcat(response, cmd);
      strcat(response, " ");
      int modeNum = channelSet[currentChan].getMode();
      strcat(response, modeStr[modeNum]);
      Serial.println(response);
      recognised = true;
    }
    if (token != NULL && strcmp(token, "ALL") == 0) {
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
      Serial.println(response);
      recognised = true;
    }
  }
  if (token != NULL && strcmp(token, "START") == 0) {
    Serial.println("ACK START");
    indexValue = 0;
    outputEnabled = true;
    recognised = true;
  }
  if (token != NULL && strcmp(token, "STOP") == 0) {
    Serial.println("ACK STOP");
    outputEnabled = false;
    recognised = true;
  }
  if (not recognised) {
    Serial.print("NAK BAD REQUEST "); Serial.println(cmd);
  }
}

// a function to test the methods used in the class

float readLightGate(unsigned long interval) {
  char response[30];
  char temp[maxTempStrLen];
  float value = NO_DATA;
  float voltageValue;
  int currentCollectMode;
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    float reading = tenPtAverage();
    currentCollectMode = channelSet[currentChan].getMode();
    if (currentCollectMode == 0) {      
      value = reading;
    }
    if (currentCollectMode == 1) {
      value = reading;
    }
  }
  return (value);
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
#ifdef XYZ
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


float voltageSensor(unsigned long interval) {  //Function to collect data, checks if it is time to collect the next data point
  // check to see if it's time to blink the LED; that is, if the difference
  // between the current time and last time you blinked the LED is bigger than
  // the interval at which you want to blink the LED.
  char response[30];
  char temp[maxTempStrLen];
  float value = NO_DATA;
  float voltageValue;
  int currentCollectMode;
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    float reading = analogRead(voltagePin);
    currentCollectMode = channelSet[currentChan].getMode();
    if (currentCollectMode == 0) {
      voltageValue = (5 * ((reading + 1) / 1024));
      value = voltageValue;
    }
    if (currentCollectMode == 1) {
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
    if (currentCollectMode == 0) {
      distance = duration * 0.1715;
    }
    if (currentCollectMode == 1) {
      distance = duration;
    }
  }
  return (distance);
}


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
          if (intervalIndex > INTERVALARRAYSIZE - 1) {
            intervalIndex = 0;
          }
          digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));  // toggle LED state for every interval measurement
          #ifdef XYZ
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
    #ifdef XYZ
      Serial.println("STATE ERROR");
    #endif
      break;
  }
  if (state != startState) {
    #ifdef XYZ
    Serial.print(millis());
    Serial.print(" ");
    Serial.print(stateStr[startState]);
    Serial.print(" -> ");
    Serial.print(stateStr[state]);
    Serial.print(" ");
    #endif


  }
}


long tenPtAverage () {
  int i = 0, count = 0;
  long sum = 0;
  for (i = 0 ; i < INTERVALARRAYSIZE - 1 ; i++) {
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
