/**
 * This code demonstrates the use of an MPU6050 gyroscope and accelerometer sensor to control the pitch of a buzzer.
 * 
 * The code includes the following functionality:
 * - Initializes the MPU6050 sensor and configures its settings
 * - Defines a 2D array of musical notes that correspond to different octaves and notes
 * - Implements a `pitch()` function that calculates the current octave and note based on the gyroscope readings
 * - Plays the calculated pitch on the buzzer in the `loop()` function
 * - Prints the accelerometer, gyroscope, and temperature data to the serial monitor
 */
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include "pitches.h"

// ESP32 pin GPIO18 connected to piezo buzzer
#define BUZZZER_PIN_1  25
#define BUZZZER_PIN_2  26

#define LED_PIN_BASS 33
#define LED_LEN_BASS 38

#define LED_PIN_MELODY 27 //D27
#define LED_LEN_MELODY 44

struct note
{
  int pitch;
  int octave;
  int duration;
  bool is_playing;
};


// defines  Bb Major scale with 6 octaves
int bb_scale[6][8] = {{NOTE_AS1, NOTE_C1, NOTE_D1, NOTE_DS1, NOTE_F1, NOTE_G1, NOTE_A1, SILENCE}, 
                      {NOTE_AS2, NOTE_C2, NOTE_D2, NOTE_DS2, NOTE_F2, NOTE_G2, NOTE_A2, SILENCE}, 
                      {NOTE_AS3, NOTE_C3, NOTE_D3, NOTE_DS3, NOTE_F3, NOTE_G3, NOTE_A3, SILENCE}, 
                      {NOTE_AS4, NOTE_C4, NOTE_D4, NOTE_DS4, NOTE_F4, NOTE_G4, NOTE_A4, SILENCE},
                      {NOTE_AS5, NOTE_C5, NOTE_D5, NOTE_DS5, NOTE_F5, NOTE_G5, NOTE_A5, SILENCE},
                      {NOTE_AS6, NOTE_C6, NOTE_D6, NOTE_DS6, NOTE_F6, NOTE_G6, NOTE_A6, SILENCE}};

int noteDuration[] = {125, 125, 125, 125, 125, 125, 125, 125, 250, 250, 250, 250, 500, 500, 500, 500, 1000, 1000, 1000, 1500};

unsigned long previousMillisMelody = 0, previousMillisBass = 0;

struct note melodyCurrentNote = {0, 3, 0, false};
struct note bassCurrentNote = {0, 0, 0, false};

// MPU6050 sensor objects
Adafruit_MPU6050 mpu1;
Adafruit_MPU6050 mpu2;

// LED strip objects
Adafruit_NeoPixel NeoPixel_B(LED_LEN_BASS, LED_PIN_BASS, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel NeoPixel_M(LED_LEN_MELODY, LED_PIN_MELODY, NEO_GRB + NEO_KHZ800);
int pixelMelody = 0, pixelBass = 0;


/**
 * @brief Determines the duration of a note based on the total acceleration.
 *
 * This function takes the total acceleration as input and returns a note duration
 * based on predefined ranges of acceleration values. The note duration is selected
 * randomly from specific ranges within the `noteDuration` array.
 *
 * @param totalAcc The total acceleration value.
 * @return The duration of the note, selected randomly from predefined ranges.
 */
int defineNoteDuration (float totalAcc){
  if (totalAcc > 0.5 and totalAcc < 0.75) return noteDuration[random(18, 19)];
  if (totalAcc > 0.75 and totalAcc < 3) return noteDuration[random(10, 18)];
  return noteDuration[random(0, 10)];
}

/**
 * @brief Adjusts the melody note based on accelerometer and gyroscope readings.
 *
 * This function modifies the current melody note's octave and pitch based on the 
 * provided total acceleration and total spin values. The adjustments are made 
 * according to specific thresholds and random variations.
 *
 * @param totalAcc The total acceleration value from the accelerometer.
 * @param totalSpin The total spin value from the gyroscope.
 */
void defineMelodyNote(float totalAcc, float totalSpin){
  int octave = melodyCurrentNote.octave;
  int pitch = melodyCurrentNote.pitch;

  if (totalAcc < 3) octave -= 1;
  if (totalAcc >= 3) octave += 1;

  if (octave < 0) octave = 5;
  if (octave > 5) octave = 2;
  
  if (totalSpin < 3) pitch -= random(0, 6);
  if (totalSpin > 4) pitch += random(0, 6);

  if (pitch < 0) pitch = abs(pitch);
  while (pitch > 6) pitch -= 3;

  melodyCurrentNote.pitch = pitch;
  melodyCurrentNote.octave = octave;
  melodyCurrentNote.duration = defineNoteDuration(totalAcc);

  if(totalAcc < 0.5 || totalSpin < 0.5) {
    melodyCurrentNote.pitch = 7;
    melodyCurrentNote.duration = 50;
  }
}

/**
 * @brief Defines the bass note based on the total acceleration and total spin.
 *
 * This function adjusts the octave and pitch of the bass note according to the 
 * provided total acceleration and total spin values. The pitch is selected from 
 * a predefined set of harmonics based on the current melody note's pitch. The 
 * octave is adjusted based on the total spin value, and the duration of the note 
 * is determined by the total acceleration.
 *
 * @param totalAcc The total acceleration value used to define the note duration.
 * @param totalSpin The total spin value used to adjust the octave.
 */
void defineBassNote(float totalAcc, float totalSpin){
  int harmonics[8][3] = {{2, 4, 6}, {3, 5, 0}, {4, 6, 1}, 
                        {5, 0, 2}, {6, 1, 3}, {0, 2, 4}, 
                        {1, 3, 5}, {0, 2, 4}};
  int octave = bassCurrentNote.octave;
  int pitch = bassCurrentNote.pitch;

  if (totalSpin < 3) octave -= 1;
  if (totalSpin >= 3) octave += 1;

  if (octave < 0) octave = 2;
  if (octave > 5) octave = 0;
  
  bassCurrentNote.pitch = harmonics[melodyCurrentNote.pitch][random(0, 2)];
  bassCurrentNote.octave = octave;
  bassCurrentNote.duration = defineNoteDuration(totalAcc) * 2;

  if(totalAcc < 0.5 || totalSpin < 0.5) {
    bassCurrentNote.pitch = 7;
    bassCurrentNote.duration = 50;
  }
}

void playMelodyLEDs(){
  if(pixelMelody == LED_LEN_MELODY){
      pixelMelody = 0;
  }

  if (melodyCurrentNote.pitch < 7){
    switch (melodyCurrentNote.duration){
    case 250:
      NeoPixel_M.rainbow(pixelMelody, -1 , 255, 200, 1);
      break;
    case 500:
      NeoPixel_M.rainbow(pixelMelody, 1 , 255, 200, 1);
      break;
    case 1000:
      NeoPixel_M.rainbow(pixelMelody, 2 , 255, 200, 1);
      break;    
    default:
      NeoPixel_M.rainbow(pixelMelody, 3 , 255, 200, 1);
      break;
    }
    NeoPixel_M.show();
    pixelMelody++;
  } else {
    for(int pixel = 0; pixel<LED_LEN_MELODY; pixel++){
        NeoPixel_M.setPixelColor(pixel, NeoPixel_M.Color(255, 255, 255));
    }
    NeoPixel_M.show();
  }
}

void playBassLEDs(){
  if (pixelBass == LED_LEN_BASS){
    pixelBass = 0;
  }

  if (bassCurrentNote.pitch < 7){
    switch (bassCurrentNote.duration){
    case 1000:
      NeoPixel_B.fill(100, 0, LED_LEN_BASS);
      break;
    case 2000:
      NeoPixel_B.fill(200, 0, LED_LEN_BASS);
      break;
    case 4000:
      NeoPixel_B.fill(50, 0, LED_LEN_BASS);
      break;    
    default:
      NeoPixel_B.fill(150, 0, LED_LEN_BASS);
      break;
    }
    NeoPixel_B.show();
    pixelBass++;
  } else {
    for(int pixel = 0; pixel<LED_LEN_BASS; pixel++){
        NeoPixel_B.setPixelColor(pixel, NeoPixel_B.Color(255, 0, 0));
    }
    NeoPixel_B.show();
  }
}


void defineColorBass(float octave, float pitch, float duration){
  //dependig on sensor 2 speed, change the color of the leds in strips BASS
  if(octave <= 1){ 
    //if first octave, colors in blue
    if(pitch <=3){
      //if low pitch, dark colors
      for (int blue = 0; blue < 255; blue++) {
        if(duration > 600){
          //if duration is long, spark slowly -> acende todos e muda o brilho
          for (int pixel = 0; pixel < LED_LEN_BASS; pixel++){
            NeoPixel_B.setPixelColor(pixel, NeoPixel_B.Color(0, 0, blue));
            NeoPixel_B.show();
            delay(200);
          }
          for (int pixel =  LED_LEN_BASS; pixel >0; pixel--){
            NeoPixel_B.setPixelColor(pixel, NeoPixel_B.Color(0, 0, blue));
            NeoPixel_B.show();
            delay(200);
          }
        } else {
          //if duration is short, spark fast -> acende todos e muda o brilho
           for (int pixel = 0; pixel < LED_LEN_BASS; pixel++){
            NeoPixel_B.setPixelColor(pixel, NeoPixel_B.Color(0, 0, blue));
            NeoPixel_B.show();
            delay(50);
          }
          for (int pixel = LED_LEN_BASS; pixel > 0; pixel--){
            NeoPixel_B.setPixelColor(pixel, NeoPixel_B.Color(0, 0, blue));
            NeoPixel_B.show();
            delay(50);
          }
        }
      }
    } else if (pitch > 3 && pitch < 7){
      //if high pitch, light colors
      for (int blue = 0; blue < 255; blue++) {
        if(duration > 600){
          //if duration is long, spark slowly -> acende todos e muda o brilho
          for (int pixel = 0; pixel < LED_LEN_BASS; pixel++){
            NeoPixel_B.setPixelColor(pixel, NeoPixel_B.Color(0, blue, blue));
            NeoPixel_B.show();
            delay(200);
          }
          for (int pixel = LED_LEN_BASS; pixel > 0; pixel--){
            NeoPixel_B.setPixelColor(pixel, NeoPixel_B.Color(0, blue, blue));
            NeoPixel_B.show();
            delay(200);
          }
        } else {
          //if duration is short, spark fast -> acende todos e muda o brilho
           for (int pixel = 0; pixel < LED_LEN_BASS; pixel++){
            NeoPixel_B.setPixelColor(pixel, NeoPixel_B.Color(0, blue, blue));
            NeoPixel_B.show();
            delay(50);
          }
          for (int pixel = LED_LEN_BASS; pixel > 0; pixel++){
            NeoPixel_B.setPixelColor(pixel, NeoPixel_B.Color(0, blue, blue));
            NeoPixel_B.show();
            delay(50);
          }
        }
      }
    } else {
        NeoPixel_B.clear();
        NeoPixel_B.show();
    }
  } else if (octave == 2){  
  //if second octave, colors in purple
    if(pitch <=3){
      //if low pitch, dark colors
      for (int green = 0; green < 255; green++) {
        if(duration > 600){
          //if duration is long, spark slowly -> acende todos e muda o brilho
          for (int pixel = 0; pixel < LED_LEN_BASS; pixel++){
            NeoPixel_B.setPixelColor(pixel, NeoPixel_B.Color(0, green, 0));
            NeoPixel_B.show();
            delay(200);
          }
          for (int pixel = LED_LEN_BASS; pixel > 0; pixel--){
            NeoPixel_B.setPixelColor(pixel, NeoPixel_B.Color(0, green, 0));
            NeoPixel_B.show();
            delay(200);
          }
        } else {
          //if duration is short, spark fast -> acende todos e muda o brilho
           for (int pixel = 0; pixel < LED_LEN_BASS; pixel++){
            NeoPixel_B.setPixelColor(pixel, NeoPixel_B.Color(0, green, 0));
            NeoPixel_B.show();
            delay(50);
          }
          for (int pixel = LED_LEN_BASS; pixel > 0; pixel--){
            NeoPixel_B.setPixelColor(pixel, NeoPixel_B.Color(0, green, 0));
            NeoPixel_B.show();
            delay(50);
          }
        }
      }
    } else if (pitch > 3 && pitch < 7){
      //if high pitch, light colors
      for (int green = 0; green < 255; green++) {
        if(duration > 600){
          //if duration is long, spark slowly -> acende todos e muda o brilho
          for (int pixel = 0; pixel < LED_LEN_BASS; pixel++){
            NeoPixel_B.setPixelColor(pixel, NeoPixel_B.Color(173, green, 47));
            NeoPixel_B.show();
            delay(200);
          }
          for (int pixel = LED_LEN_BASS; pixel > 0; pixel--){
            NeoPixel_B.setPixelColor(pixel, NeoPixel_B.Color(173, green, 47));
            NeoPixel_B.show();
            delay(200);
          }
        } else {
          //if duration is short, spark fast -> acende todos e muda o brilho
           for (int pixel = 0; pixel < LED_LEN_BASS; pixel++){
            NeoPixel_B.setPixelColor(pixel, NeoPixel_B.Color(173, green, 47));
            NeoPixel_B.show();
            delay(50);
          }
          for (int pixel = LED_LEN_BASS; pixel > 0; pixel--){
            NeoPixel_B.setPixelColor(pixel, NeoPixel_B.Color(173, green, 47));
            NeoPixel_B.show();
            delay(50);
          }
        }
      }
    } else {
      //acende tudo ao mesmo tempo e deixa aceso em branco
      for(int pixel = 0; pixel<LED_LEN_BASS; pixel++){
        NeoPixel_B.setPixelColor(pixel, NeoPixel_B.Color(255, 255, 255));
        delay(100);
      }
      NeoPixel_B.show();
    }
  }
}

void playBassNote(sensors_event_t a2, sensors_event_t g2){
  float totalAcc2 = sqrt(a2.acceleration.x * a2.acceleration.x + a2.acceleration.y * a2.acceleration.y);
  float totalSpin2 = sqrt(g2.gyro.x * g2.gyro.x + g2.gyro.y * g2.gyro.y);

  defineBassNote(totalAcc2, totalSpin2);
  tone(BUZZZER_PIN_2, bb_scale[bassCurrentNote.octave][bassCurrentNote.pitch]);
  bassCurrentNote.is_playing = true;
  playBassLEDs();
}

void playMelodyNote(sensors_event_t a1, sensors_event_t g1){
  float totalAcc1 = sqrt(a1.acceleration.x * a1.acceleration.x + a1.acceleration.y * a1.acceleration.y);
  float totalSpin1 = sqrt(g1.gyro.x * g1.gyro.x + g1.gyro.y * g1.gyro.y);

  defineMelodyNote(totalAcc1, totalSpin1);
  tone(BUZZZER_PIN_1, bb_scale[melodyCurrentNote.octave][melodyCurrentNote.pitch]);
  melodyCurrentNote.is_playing = true;
  playMelodyLEDs();
}

/**
 * Configures the MPU6050 sensor with the following settings:
 * - Accelerometer range: ±8g
 * - Gyroscope range: ±500 deg/s
 * - Filter bandwidth: 5 Hz
 * 
 * This function initializes the MPU6050 sensor and checks if it is found. If the sensor is not found, the function will enter an infinite loop.
 */
void setMPUConfigurations(){
  Serial.println("Searching MPU6050 chip 1");
  if (!mpu1.begin(0x68)) {
        Serial.println("Failed to find MPU6050 chip 1");
        while (1) {
            delay(10);
        }
    }
    Serial.println("MPU6050 1 Found!");

    // Initialize the second MPU6050
    Serial.println("Searching MPU6050 chip 2");
    if (!mpu2.begin(0x69)) {
        Serial.println("Failed to find MPU6050 chip 2");
        while (1) {
            delay(10);
        }
    }
    Serial.println("MPU6050 2 Found!");

    // Set up the sensors
    mpu1.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu1.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu1.setFilterBandwidth(MPU6050_BAND_21_HZ);

    mpu2.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu2.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu2.setFilterBandwidth(MPU6050_BAND_21_HZ);

    delay(100);
}

/**
 * Prints the data from the MPU6050 sensor, including the accelerometer, gyroscope, and temperature readings.
 *
 * @param a The accelerometer event data.
 * @param g The gyroscope event data.
 * @param temp The temperature event data.
 */
void printMPUData(sensors_event_t a, sensors_event_t g, sensors_event_t temp){
  Serial.print("AccX:");
  Serial.print(a.acceleration.x);
  Serial.print(",AccY:");
  Serial.print(a.acceleration.y);
  Serial.print(",AccZ:");
  Serial.print(a.acceleration.z);
  Serial.print(",RotX:");
  Serial.print(g.gyro.x * (180 / 3.1415));
  Serial.print(",RotY:");
  Serial.print(g.gyro.y * (180 / 3.1415));
  Serial.print(",RotZ:");
  Serial.print(g.gyro.z * (180 / 3.1415));
  Serial.print(",Temp:");
  Serial.println(temp.temperature);
}

void setup() {
  Serial.begin(115200);
  while (!Serial)
    delay(10);

  setMPUConfigurations();
  NeoPixel_B.begin();
  NeoPixel_M.begin();

  delay(100);
}

unsigned long currentMillis = millis();
void loop() {
  currentMillis = millis();
  if (currentMillis - previousMillisMelody >= melodyCurrentNote.duration) {
    Serial.println("mel");
    previousMillisMelody = currentMillis;
    if (melodyCurrentNote.is_playing)
    {
    Serial.println("mel notone");
      noTone(BUZZZER_PIN_1);
      melodyCurrentNote.is_playing = false;
    }
    sensors_event_t a1, g1, temp1;
    mpu1.getEvent(&a1, &g1, &temp1);
    //printMPUData(a1, g1, temp1);
    playMelodyNote(a1, g1);
  }

  if (currentMillis - previousMillisBass >= bassCurrentNote.duration) {
    Serial.println("bass");
    previousMillisBass = currentMillis;
    if (bassCurrentNote.is_playing)
    {
    Serial.println("bass notone");
      noTone(BUZZZER_PIN_2);
      bassCurrentNote.is_playing = false;
    }
    sensors_event_t a2, g2, temp2;
    mpu2.getEvent(&a2, &g2, &temp2);
    //printMPUData(a2, g2, temp2);
    playBassNote(a2, g2);
  }

  delay(50);
}