#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

//setup screen dimensions
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);


#include <MIDI.h>
#if defined(ARDUINO_SAM_DUE) || defined(SAMD_SERIES) || defined(_VARIANT_ARDUINO_ZERO_)
   /* example not relevant for this hardware (SoftwareSerial not supported) */
   MIDI_CREATE_DEFAULT_INSTANCE();
#else
   #include <SoftwareSerial.h>
   using Transport = MIDI_NAMESPACE::SerialMIDI<SoftwareSerial>;
   int rxPin = 8;
   int txPin = 9;
   SoftwareSerial mySerial = SoftwareSerial(rxPin, txPin);
   Transport serialMIDI(mySerial);
   MIDI_NAMESPACE::MidiInterface<Transport> MIDI((Transport&)serialMIDI);
#endif


//button setup
int buttonScale = 14;
int buttonRoot  = 15;
  
//setup scales

//if you want to add a scale make sure you edit both the names and the scale itself
//scales[<number of scales>][<number of notes in each scale>]
const int scales[7][7] = { //Array of scale arrays containing the steps between each note
  {0, 2, 4, 5, 7, 9, 11}, // Ionian, Major
  {0, 3, 4, 6, 8, 9, 10}, // Dorian
  {0, 1, 3, 5, 7, 8, 9}, //Phrgian 
  {0, 2, 4, 6, 7, 9, 10}, //lydian
  {0, 2, 4, 5, 7, 9, 10}, //mixolydian
  {0, 2, 3, 5, 7, 8, 10}, // Natural Minor
  {0, 1, 3, 5, 6, 8, 10} //locarian
};

const String scaleNames[7] ={
  "Major",
  "Dorian",
  "Phrygian",
  "Lydian",
  "Mixolyd",
  "Minor",
  "Locarian"
};

 
const String noteNames[12]={
  "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"};

/*
// if you prefer sharps like a werido
const String noteNames[12]={
  "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
*/

//setting the current scale to Minor because having it be C Major will be boring and seem to do nothing except break the black keys
int currentScale=5; 
int rootNote=0;

// i set this assuming it would come in handy, never needed it. leaving it here cause its nice to know
//int middleC=60;


//I'm not sure if this is a good idea but this how I write pico 8 games in lua 
//the state changes how our note on/off handlers work. current states are 
//0=play //play the notes after transposing them
//1=scale //use the next note to set the scale
//2=root  //set the next note played to the root of our scale

//initalize in state 0 so we can play right away
int state=0;


//read the buttons and if one is pessed change the screen and update the state. 
void readButtons(){
  if (digitalRead(buttonScale) == LOW ){
    state=1;
    updateScreen("Scale?");
  }
  
  if (digitalRead(buttonRoot) == LOW){
    state=2; 
     updateScreen("Root?");
  }
  
}


byte getTransPitch(byte oldPitch){
  //this where the magic happens and notes are transposed within the current scale/key
  //accepts a pitch value from a midi event 
  //each midi note is a number stored in a single byte. 
  
  //initalize degree which we'll need later to figure out which of the 7 notes in our scale to play
  int degree=0; 
  
  //get our octave and save it to an int (12 notes per octave and our remainder is discarded middle C is 60, so its octave 5)
  int octave=oldPitch/12;
  
  //grab the remainder we discarded up above, which tells us our note, since C is conviently 0 here
  
  byte newPitch=oldPitch%12;

 //Print Debug info in case we need it later
  Serial.print("Found octave: ");
  Serial.print(octave);
  Serial.print(" Found note: ");
  Serial.println(newPitch);
  

  
  //get the new midi note number conforming to the current scale
  //assume user is playing cmajor which contains these notes {0, 2, 4, 5, 7, 9, 11}
  //we iterate keeping each note that isn't higher than the one we're looking for 
  //this way if the user hit a black key (even though the whole point is not to hit black keys)
  //we'll just keep the last note below it
    for (int i = 0; i < 7; ++i) {
      if (scales[0][i] <= newPitch){
        degree=i;
      }
    }

  //so our new pitch is our octave*12 (12 notes per octave) + (the degree of our scale, [think of it as the nth note]) + our root note
  newPitch=(12*octave)+scales[currentScale][degree]+rootNote;

  //print what we transposed to and return that number
  Serial.print(" transposed to note: ");
  Serial.println(newPitch);
  return newPitch;
}


//helpful function to clear the screen and display some text
void updateScreen(String text){
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println(text);
  display.display(); 
  }



//sets default sceen text of scale followed by note on newline
void defaultScreenText(){
  updateScreen(scaleNames[currentScale]+"\n"+noteNames[rootNote%12]);
  }

void handleNoteOn(byte channel, byte pitch, byte velocity) {
  //if state == 0 transpose the pitch and play the note as normal, print stuff for debug
  switch(state){
    case 0: //play
      pitch=getTransPitch(pitch);
      MIDI.sendNoteOn(pitch, velocity, channel);
      Serial.print("Note On: ");
      Serial.print(pitch);
      Serial.print(" Velocity: ");
      Serial.print(velocity);
      Serial.print(" Channel: ");
      Serial.println(channel);
      break;

    //state == 1 enter scale mode. Next note sets the scale. (white keys only, I only have 7 scales at the moment)
    case 1: //scale
      for (int i = 0; i < 7; ++i) {
        if (scales[0][i] <= pitch%12){
          currentScale=i;
        }
      }
      Serial.print("Set scale to: ");
      Serial.println(scaleNames[currentScale]);
      break;

    //next note sets the root note
    case 2: //root
      rootNote=pitch%12;
      Serial.print("Set root to: ");
      Serial.println(noteNames[rootNote]);
      break;
    }
  }

  
void handleNoteOff(byte channel, byte pitch, byte velocity) {
  if (state == 0){
    pitch=getTransPitch(pitch);
    MIDI.sendNoteOff(pitch, 0, channel);
    Serial.print("Note Off: ");
    Serial.print(pitch);
    Serial.print(" Velocity: ");
    Serial.print(velocity);
    Serial.print(" Channel: ");
    Serial.println(channel);
    }
  //if  the state isn't zero, set it to zero, if we're getting a note off event and we were in a non play state, then the user must have changed a setting
  //all the work was handled by handleNoteOn. Honestly we could have done this part there as well but then the user would be releasing the key in the play state
  //and we send this errant note off out into the world and who knows what that could do to some weird poorly design synth
  else {
    state=0;
    defaultScreenText();
    }
}



void setup()
{
  
  //set our pins to use the input pullup so we can just wire the buttons to ground instead of soldering resistors like some kind of chump
  pinMode(buttonScale, INPUT_PULLUP);
  pinMode(buttonRoot, INPUT_PULLUP);
  //turn on that led cause why not
  pinMode(LED_BUILTIN, OUTPUT);

  //listen on all channels
  MIDI.begin(MIDI_CHANNEL_OMNI);

  //no midi through we're mucking with the midi we don't want to pass on stuff
  MIDI.turnThruOff();

  //set note handler functions that are up above
  MIDI.setHandleNoteOn(handleNoteOn); // Set the handler for note on messages
  MIDI.setHandleNoteOff(handleNoteOff); // Set the handler for note off messages

  //setup debugging serial
  Serial.begin(9600);


  
  // default pin setup on raspberry pico for our screen are
  //sda 4, scl 5. and weirdly hard to change cause adafruit libraries are very friendly and a tad over engineered
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
      Serial.println(F("SSD1306 allocation failed"));
      for(;;);
    }
  //sleep for one second cause the screen was failing when I didn't sleep and why is the whole world in such a damn hurry anyway?
  delay(1000);

  //turn on the screen and display some text
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  defaultScreenText();
  
  }


//every loop we read the buttons, and then read the midi which will be passed to our handler functions
// we also do delay 1 cause it was the code i stole from an example 
void loop() {
  readButtons();
  MIDI.read(); 
  delay(1); // Add a small delay to avoid busy-waiting
}
