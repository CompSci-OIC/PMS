//Header file used to define the class (in this case called channel)

#ifndef HEADER_CHANNEL
  #define HEADER_CHANNEL
// Class for a Channel object

//Class consists of attributes and methods


class Channel
{  
  // attributes
  private: //Private variables --> Not visible outside the class
    unsigned long sampleInterval; // time between samples (ms)
    int samplesToTake; //Holds the number of samples to be taken in a single measurement 
    char unitStr[4]; //Holds the units in a character array
    int collectMode; //Mode for collecting data
  // public:
  // define any public attributes here
   
  // methods
  public:
    Channel(void); //Constructor method --> Is called when objects are created within the class
    void setSampleInterval(int value);
    unsigned long getSampleInterval(void); 
    void setSamplesToTake(int value);
    int getSamplesToTake(void);
    void setUnits(char *units);
    char *getUnits(void);
    void setMode(int value);
    int getMode(void);
    void getAll(void);
  //private:
    // define any private methods here 
};

#endif
