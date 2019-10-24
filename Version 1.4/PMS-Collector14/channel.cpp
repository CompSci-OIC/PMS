
//File used to define the methods for the channel class

#include <Arduino.h>

#include "channel.h"

// function definitions for the methods in the Channel class

// Constructor method:
// Args:

Channel::Channel(void) {
  // set attributes to default values
  samplesToTake = 20;
  sampleInterval = 250; // msec
  collectMode = MODE_CAL;
}

// return the current value of the sample interval
unsigned long Channel::getSampleInterval(void) {
  return (sampleInterval);
}

// set the current value of the sample interval
void Channel::setSampleInterval(int value) {
  sampleInterval = value;
}

//Sets the number of samples to be taken
void Channel::setSamplesToTake(int value) {
  samplesToTake = value;
}

int Channel::getSamplesToTake(void) {
  return (samplesToTake);
}

void Channel::setUnits(char *units) {
  strcpy(&unitStr[0], units);
}

char *Channel::getUnits(void) {
  return (unitStr);
}

void Channel::setMode(int value){
  collectMode = value;
}

int Channel::getMode(void){
  return(collectMode);
}

void Channel::getAll(void) {
  Serial.print("DBG SampleInterval = "); Serial.println(sampleInterval);
}
