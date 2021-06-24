#include <SPI.h>
#include <SD.h>
#include <Arduino_LSM9DS1.h>
#include <PDM.h>

#include <mbed.h>
#include <rtos.h>
#include <mbed_wait_api.h>
#include <platform/CircularBuffer.h>
#include <platform/Callback.h>

using namespace rtos;

// FINISH THREADDING!!!!!!!!!!!!!!!!!!!!!!!!!!!!

/*************************Global Variables *************************/
// IMU data variables
float ax, ay, az, wx, wy, wz, mx, my, mz;
float imu_data[] = {ax, ay, az, wx, wy, wz, mx, my, mz};
int imu_counter = 0;

// microphone variables
PDMClass PDM(D8, D7, D6);
static const char channels = 2;
static const int frequency = 16000;
short sampleBuffer[512];
volatile int samplesRead;
int mic_init = 0;
int mic_counter = 0;

// SD card variables
File imu_file, mic_file;
const int chipSelect = D10;
int resetOpenLog = D2;

// threadding variables
Thread mic_thread;
/********************************************************************/

void setup_IMU() {
  if (!IMU.begin()) {
    Serial.println("Failed to initialize IMU!");
    while (1);
  }
//  imu_file = SD.open("imu_data.csv", FILE_WRITE);
//  imu_file.println("ax, ay, az, wx, wy, wz, mx, my, mz");
//  imu_file.seek(EOF);

  Serial.println("IMU initialized");
}

void update_IMU() {
  int start = millis();

  bool acc_avail, omg_avail, mag_avail;
  if (acc_avail = IMU.accelerationAvailable()) {
    IMU.readAcceleration(ax, ay, az);
  }

  if (omg_avail = IMU.gyroscopeAvailable()) {
    IMU.readGyroscope(wx, wy, wz);
  }

  if (mag_avail = IMU.magneticFieldAvailable()) {
    IMU.readMagneticField(mx, my, mz);
  }

  Serial.println("UIMU: " + (millis() - start));

//  if (acc_avail | omg_avail | mag_avail) {
//    float data_arr[] = {ax, ay, az, wx, wy, wz, mx, my, mz};
//    for (int i = 0; i < 9; i++) {
//      imu_file.print(String(data_arr[i]) + ",");
//    }
//    imu_file.println("");
//    imu_counter++;
//    if(imu_counter > 50){
//      imu_file.flush();
//      imu_counter = 0;
//    }
//  }
}

void print_IMU() {
  // Serial.println("Printing IMU data");
//  noInterrupts();
  int start = millis();
  Serial.print("acc:\t" + String(ax) + "\t" + String(ay) + "\t" + String(az) + "\t\t");
  Serial.print("ang:\t" + String(wx) + "\t" + String(wy) + "\t" + String(wz) + "\t\t");
  Serial.println("mag:\t" + String(mx) + "\t" + String(my) + "\t" + String(mz) + "\t\t");

  if(appendFile("mic_data.csv")){
    Serial1.print(String(ax) + ", " + String(ay) + ", " + String(az));
    Serial1.print(String(wx) + ", " + String(wy) + ", " + String(wz));
    Serial1.println(String(mx) + ", " + String(my) + ", " + String(mz));
    delay(5);
  }
  Serial.println("PIMU: " + (millis() - start));
//  interrupts();
}

void setup_mic() {
  // Configure the data receive callback
  PDM.onReceive(onPDMdata);

  // Optionally set the gain
  // Defaults to 20 on the BLE Sense and -10 on the Portenta Vision Shield
  // PDM.setGain(30);

  // Initialize PDM with:
  // - one channel (mono mode)
  // - a 16 kHz sample rate for the Arduino Nano 33 BLE Sense
  // - a 32 kHz or 64 kHz sample rate for the Arduino Portenta Vision Shield
  if (!PDM.begin(channels, frequency)) {
    Serial.println("Failed to start PDM!");
    while (1);
  }


//  mic_file = SD.open("mic_data.csv", O_WRITE | O_CREAT);
//  mic_file.println("L, R");
//  mic_file.seek(EOF);
  
  Serial.println("mic initialized");
  mic_init = 1;
  //  mic_thread.start(mbed::callback(print_mic));
}

/**
   Callback function to process the data from the PDM microphone.
   NOTE: This callback is executed as part of an ISR.
   Therefore using `Serial1` to print messages inside this function isn't supported.
 * */
void onPDMdata() {
  // Query the number of available bytes
  int bytesAvailable = PDM.available();

  // Read into the sample buffer
  PDM.read(sampleBuffer, bytesAvailable);

  // 16-bit, 2 bytes per sample
  samplesRead = bytesAvailable / 2;
}

void print_mic() {
  // Wait for samples to be read
  // Serial.println("Printing microphone data");
  int start = millis();
  if (samplesRead) {
    // Print samples to the Serial1 monitor or plotter
    for (int i = 0; i < samplesRead; i++) {
      if (channels == 2) {
        Serial.print("L:");
        Serial.print(sampleBuffer[i]);
        Serial.print(" R:");
        i++;
      }
      Serial.println(sampleBuffer[i]);
    }

    if(appendFile("mic_data.csv")){
      for (int i = 0; i < samplesRead; i++) {
        if (channels == 2) {
          Serial1.print(sampleBuffer[i] + ", ");
          i++;
        }
        Serial1.println(sampleBuffer[i]);
      }
      delay(5);
    }
//    save_mic_data();
    // Clear the read count
    samplesRead = 0;
  }
  Serial.println("MIC: " + (millis() - start));
}

void save_mic_data() {
  if (mic_init) {
    for (int i = 0; i < samplesRead; i++) {
      mic_file.print(String(sampleBuffer[i++]) + ", " + String(sampleBuffer[i]) + "\n");
      //          Serial1.println("mic printed");
    }
    mic_counter++;
    if (mic_counter > 100) {
      mic_file.flush();
      mic_counter = 0;
    }
  }
}

void setup_SD_card() {
  if (!SD.begin(chipSelect)) {
    Serial.println("SD card initialization failed!");
    while (1);
  }
  Serial.println("SD card initialization done.");
}

void setup() {
  Serial.begin(115200);
  Serial1.begin(115200);
  while (!Serial1 || !Serial);
  Serial.println("Started");

//  setup_SD_card();
  setupOpenLog();
  setup_mic();
  setup_IMU();

  gotoCommandMode();
  runCommand("rm LOG*", '>');
  runCommand("rm imu_data.csv", '>');
  runCommand("rm mic_data.csv", '>');
  createFile("imu_data.csv");
  Serial1.println("ax, ay, az, wx, wy, wz, mx, my, mz");
  delay(10);
  createFile("mic_data.csv");
  Serial1.println("L, R");
  delay(10);
}

void loop() {
  update_IMU();
  print_IMU();
  print_mic();
//  openlog_terminal();
}
