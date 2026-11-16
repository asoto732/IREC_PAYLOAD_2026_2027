#include <Adafruit_BMP3XX.h>
#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <EEPROM.h>
#include <string.h>
 
namespace {
constexpr uint8_t kBmpAddress = 0x77;
constexpr uint8_t kAdxlAddress = 0x53;
constexpr uint32_t kI2cClockHz = 400000;
constexpr uint32_t kBarometerPeriodUs = 40000;// 25 Hz
constexpr uint32_t kFifoPollPeriodUs = 5000; //FIFO holds 32 samples which a 5 ms would not surpas
constexpr uint32_t kBypassPollPeriodUs = 300;//Check if data is ready this often
constexpr uint32_t kReportPeriodUs = 1000000;//report every second
constexpr uint16_t kGroundSamples = 50;//Takes this many samples to get a good ground reading
constexpr uint32_t kPrintEveryNthAccel = 8;//100 printed lines/s at 800 Hz
constexpr float kGPerLsb = 0.049f; //To get the acceleration in Gs, multiply the raw value by this constant
 
constexpr uint8_t kBmpChipIdReg = 0x00;
constexpr uint8_t kBmpErrorReg = 0x02;
constexpr uint8_t kAdxlDeviceIdReg = 0x00;
constexpr uint8_t kAdxlBandwidthReg = 0x2C;
constexpr uint8_t kAdxlPowerReg = 0x2D;
constexpr uint8_t kAdxlIntSourceReg = 0x30;
constexpr uint8_t kAdxlDataFormatReg = 0x31;
constexpr uint8_t kAdxlDataReg = 0x32;
constexpr uint8_t kAdxlFifoControlReg = 0x38;
constexpr uint8_t kAdxlFifoStatusReg = 0x39;
//The errors of what went wrong.
constexpr uint8_t kErrorBmpUnavailable = 1U << 0;
constexpr uint8_t kErrorAdxlUnavailable = 1U << 1;
constexpr uint8_t kErrorBmpInternal = 1U << 2;
constexpr uint8_t kErrorAccelOverrun = 1U << 3;
constexpr uint8_t kErrorBarometerLate = 1U << 4;

constexpr uint32_t kI2cTimeoutUs = 5000;
 
inline unsigned long ul(uint32_t v) { return static_cast<unsigned long>(v); }
 
constexpr int kRefEepromAddr = 0;
constexpr uint32_t kRefMagic = 0x52454631;
constexpr bool kTrustOnlyFrozenReference = true;

struct Timing { //This is for calculating bus budget 
  uint32_t count = 0, totalUs = 0, maxUs = 0; //amount of transactions, total time, and worst case 
  void record(uint32_t us) {
    ++count;
    totalUs += us;
    if (us > maxUs) maxUs = us;
  }
  void report(const char* name) { //Prints a line outputting the data
    if (count == 0) {
      Serial.printf("I2C,%s,no_samples\n", name);
      return;
    }
    Serial.printf("I2C,%s,count=%lu,avg_us=%lu,max_us=%lu\n", name, ul(count), ul(totalUs / count), ul(maxUs));
    *this = Timing();
  }
};
 
bool readRegister(TwoWire& bus, uint8_t address, uint8_t reg, uint8_t* data, uint8_t length, Timing* timing = nullptr) {
  const uint32_t started = micros();
  bus.beginTransmission(address);
  bus.write(reg); //setup of the register and address to read from
  bool ok = bus.endTransmission(false) == 0 && bus.requestFrom(address, length) == length; //checks if the transmission was successful and if the amount of bytes requested is equal to the amount of bytes received
  for (uint8_t i = 0; ok && i < length; ++i) data[i] = static_cast<uint8_t>(bus.read());
  while (bus.available()) bus.read();
  const uint32_t elapsed = micros() - started;
  if (timing) timing->record(elapsed);
  return ok && elapsed < kI2cTimeoutUs;
}
 
bool writeRegister(TwoWire& bus, uint8_t address, uint8_t reg, uint8_t value) {
  bus.beginTransmission(address);
  bus.write(reg);
  bus.write(value);
  const uint32_t started = micros();
  bool ok = bus.endTransmission() == 0;
  const uint32_t elapsed = micros() - started;
  return ok && elapsed < kI2cTimeoutUs;
}

void restartBus(TwoWire& bus) {
  bus.end();
  bus.begin();
  bus.setClock(kI2cClockHz);
}
 
class Barometer {
 public:
 explicit Barometer(TwoWire& bus) : bus_(bus) {}
  bool begin() {
    uint8_t chipId = 0;
    ready_ = readRegister(bus_, kBmpAddress, kBmpChipIdReg, &chipId, 1) && chipId == 0x60 &&
             bmp_.begin_I2C(kBmpAddress, &bus_);
    if (!ready_) return false;
    bus_.setClock(kI2cClockHz); //restore bus speed just in case
    bmp_.setTemperatureOversampling(BMP3_OVERSAMPLING_2X);
    bmp_.setPressureOversampling(BMP3_OVERSAMPLING_4X);//oversampling more since this gives altitude
    bmp_.setIIRFilterCoeff(BMP3_IIR_FILTER_COEFF_3); //I think this should be fine for the rocket but could potentially be decreased
    return true;
  }
 
  bool read(float& pressurePa, float& temperatureC, uint8_t& fault) {
    const uint32_t started = micros();
    const bool readOk = bmp_.performReading();
    dataTiming_.record(micros() - started);
    fault = 0xFF; //reading the sensor's errors
    const bool faultReadOk = readRegister(bus_, kBmpAddress, kBmpErrorReg, &fault, 1, &errorTiming_);
    if (!readOk || !faultReadOk || fault != 0) { //if anything went wrong
      ready_ = false;
      return false;
    }
    pressurePa = bmp_.pressure;
    temperatureC = bmp_.temperature;
    return true;
  }
 
  bool ready() const { return ready_; }
  void reportTiming() {
    dataTiming_.report("i2c0_bmp_data");
    errorTiming_.report("i2c0_bmp_error");
  }
 
 private:
  TwoWire& bus_;
  Adafruit_BMP3XX bmp_;
  Timing dataTiming_, errorTiming_;
  bool ready_ = false;
};
 
struct Acceleration {
  int16_t x, y, z;
};
 
class Accelerometer {
 public:
  explicit Accelerometer(TwoWire& bus) : bus_(bus) {}
  bool begin() {
    uint8_t id = 0;
    ready_ = readRegister(bus_, kAdxlAddress, kAdxlDeviceIdReg, &id, 1) && id == 0xE5 && //check if the device is present and has the correct ID
             writeRegister(bus_, kAdxlAddress, kAdxlBandwidthReg, 0x0D) && //Bandwith set to 800 Hz
             writeRegister(bus_, kAdxlAddress, kAdxlDataFormatReg, 0x0B) && //+/- 200g range
             writeRegister(bus_, kAdxlAddress, kAdxlFifoControlReg, 0x00) && //clears the old data in the FIFO
             writeRegister(bus_, kAdxlAddress, kAdxlFifoControlReg, fifo_ ? 0x80 : 0x00) && //sets the FIFO mode to stream mode
             writeRegister(bus_, kAdxlAddress, kAdxlPowerReg, 0x08); //puts the device into measurement mode
    return ready_; //did everything succeed
  }
 
  void setFifo(bool enable) {
    fifo_ = enable;
    if (ready_) {
      ready_ = writeRegister(bus_, kAdxlAddress, kAdxlFifoControlReg, 0x00) &&
               writeRegister(bus_, kAdxlAddress, kAdxlFifoControlReg, enable ? 0x80 : 0x00);
    }
  }
 
  bool pending(uint8_t& count, bool& overrun) { //decides how many samples to get and if any samples were overwritten
    uint8_t value = 0;
    if (!readRegister(bus_, kAdxlAddress, fifo_ ? kAdxlFifoStatusReg : kAdxlIntSourceReg, &value, 1, &statusTiming_)) {
      ready_ = false; //The above read failed, so the device is not ready to be read from
      return false;
    }
    if (fifo_) { //stream mode
      count = value & 0x3F; //FIFO count is the lower 6 bits of the FIFO status register
      uint8_t src = 0;  //sample was overwritten
      if (!readRegister(bus_, kAdxlAddress, kAdxlIntSourceReg, &src, 1, &statusTiming_)) {
        ready_ = false;
        return false;
      }
      overrun = (src & 0x01) != 0;
    } else { //bypass mode
      count = (value & 0x80) ? 1 : 0; //if the data ready bit is set, then there is one sample to read
      overrun = (value & 0x01) != 0; //if the overrun bit is set, then a sample was overwritten
    }
    return true;
  }
 
  bool read(Acceleration& sample) {
    uint8_t d[6];
    if (!readRegister(bus_, kAdxlAddress, kAdxlDataReg, d, 6, &dataTiming_)) { //if the read failed, then the device is not ready to be read from
      ready_ = false;
      return false;
    }
    sample.x = static_cast<int16_t>((d[1] << 8) | d[0]); //combine the two bytes into a 16-bit signed integer and same for below
    sample.y = static_cast<int16_t>((d[3] << 8) | d[2]);
    sample.z = static_cast<int16_t>((d[5] << 8) | d[4]);
    return true;
  }
 
  bool ready() const { return ready_; }
  uint32_t pollPeriodUs() const { return fifo_ ? kFifoPollPeriodUs : kBypassPollPeriodUs; }
  void reportTiming() {
    dataTiming_.report("i2c1_adxl_data");
    statusTiming_.report("i2c1_adxl_status");
  }
 
 private:
  TwoWire& bus_;
  Timing dataTiming_, statusTiming_;
  bool ready_ = false;
  bool fifo_ = true;
};
 
Barometer barometer(Wire);
Accelerometer accelerometer(Wire1);
 
uint8_t errorFlags = 0;
uint32_t nextAccelUs = 0, nextBarometerUs = 0, nextReportUs = 0;
uint32_t accelCount = 0, barometerCount = 0;
uint32_t lastAccelCount = 0, lastBarometerCount = 0;
uint32_t missedAccel = 0, missedBarometer = 0;
 
double groundSumPa = 0;
uint16_t groundCount = 0;
float groundPa = 0;
bool referenceFrozen = false;
 
bool due(uint32_t now, uint32_t when) { return static_cast<int32_t>(now - when) >= 0; }
 
struct StoredReference {
  uint32_t magic;
  float pressurePa;
  uint32_t frozen;
  uint32_t check;
};

uint32_t referenceCheck(const StoredReference& ref) {
  uint32_t bits;
  memcpy(&bits, &ref.pressurePa, sizeof(bits));
  return ref.magic ^ bits ^ ref.frozen ^ 0xA5A5A5A5; //This is a simple checksum to verify the integrity of the reference data
}

void saveReference() {
  StoredReference ref{kRefMagic, groundPa, referenceFrozen ? 1u : 0u, 0}; //This is the reference data that will be saved to EEPROM
  ref.check = referenceCheck(ref);
  EEPROM.put(kRefEepromAddr, ref);
}

bool loadReference() {
  StoredReference ref;
  EEPROM.get(kRefEepromAddr, ref);
  if (ref.magic != kRefMagic || ref.check != referenceCheck(ref)) return false; //if the magic number or checksum is invalid, then the reference data is invalid
  if (kTrustOnlyFrozenReference && !ref.frozen) return false;
  groundPa = ref.pressurePa;
  groundCount=kGroundSamples; //if the reference data is valid, then set the ground count to the number of samples used to calculate the reference pressure
  referenceFrozen = ref.frozen != 0;
  return true;
}

void setReferenceFrozen(bool frozen) {
  if (groundCount < kGroundSamples) return; //if the ground count is less than the number of samples used to calculate the reference pressure, then do not allow the reference to be frozen
  referenceFrozen = frozen;
  saveReference();
}

void clearReference() {
  StoredReference ref{};
  EEPROM.put(kRefEepromAddr, ref);
  groundSumPa = 0;
  groundCount = 0;
  groundPa = 0;
  referenceFrozen = false;
}


uint32_t nextBmpRetryUs = 0;
uint32_t nextAdxlRetryUs = 0;
void Recovery(uint32_t now) { //This function attempts to recover the barometer and accelerometer if they are not ready
  if (barometer.ready()) {
    nextBmpRetryUs = now + 1000000; //retry every second
  }
  else {
    errorFlags |= kErrorBmpUnavailable; //if the barometer is not ready, then set the error flag
    if (due(now, nextBmpRetryUs)) {
      nextBmpRetryUs = now + 1000000; //retry every second
      restartBus(Wire); //restart the I2C bus to try to recover the barometer
      if (barometer.begin()) nextBarometerUs = micros();
    }
  }
  if (accelerometer.ready()) {
    nextAdxlRetryUs = now + 1000000; //retry every second
  }
  else {
    errorFlags |= kErrorAdxlUnavailable; //if the accelerometer is not ready, then set the error flag
    if (due(now, nextAdxlRetryUs)) {
      nextAdxlRetryUs = now + 1000000; //retry every second
      restartBus(Wire1); //restart the I2C bus to try to recover the accelerometer
      if (accelerometer.begin()) nextAccelUs = micros();
    }
  }
}



void serviceAccelerometer(uint32_t now) { //This function services the accelerometer, reading data from it and printing it to the serial port
  if (!accelerometer.ready() || !due(now, nextAccelUs)) return;
  nextAccelUs = now + accelerometer.pollPeriodUs(); //schedule the next time to read from the accelerometer
  uint8_t pending = 0;
  bool overrun = false;
  if (!accelerometer.pending(pending, overrun)) return; //if the read failed, then the device is not ready to be read from
  if (overrun) {
    ++missedAccel;
    errorFlags |= kErrorAccelOverrun; //if a sample was overwritten, then increment the missed sample count and set the error flag
  }
  for (uint8_t i = 0; i < pending; ++i) { //for each sample that is pending, read it and print it to the serial port
    Acceleration a;
    if (!accelerometer.read(a)) return;
    if (++accelCount % kPrintEveryNthAccel == 0) {
      Serial.printf("A,%lu,%lu,%.3f,%.3f,%.3f\n", ul(accelCount), ul(micros()), a.x * kGPerLsb, a.y * kGPerLsb,
                    a.z * kGPerLsb); //print the sample count, timestamp, and acceleration in Gs
    }
  }
}
 
void serviceBarometer(uint32_t now) { //Same thing as the accelerometer, but for the barometer. It reads the pressure and temperature from the barometer and prints it to the serial port
  if (!barometer.ready() || !due(now, nextBarometerUs)) return;
 
  const uint32_t late = now - nextBarometerUs;
  if (late >= kBarometerPeriodUs) {
    missedBarometer += late / kBarometerPeriodUs;
    errorFlags |= kErrorBarometerLate;
  }
  nextBarometerUs += (late / kBarometerPeriodUs + 1) * kBarometerPeriodUs;
 
  float pressurePa = 0, temperatureC = 0;
  uint8_t fault = 0;
  if (!barometer.read(pressurePa, temperatureC, fault)) {
    if (fault != 0 && fault != 0xFF) errorFlags |= kErrorBmpInternal;
    Serial.printf("BMP_ERROR,reg=0x%02X\n", fault);
    return;
  }
  ++barometerCount;
  if (groundCount < kGroundSamples) { //Take samples to get a good ground reading
    groundSumPa += pressurePa;
    if (++groundCount == kGroundSamples) {
      groundPa = static_cast<float>(groundSumPa / kGroundSamples);
      saveReference();
      Serial.printf("GROUND,pressure_pa=%.2f\n", groundPa);
    }
  }
  const float altitudeM = //The altitude is calculated using the barometric formula. Not sure if this is good for the rocket.
      groundCount >= kGroundSamples ? 44330.0f * (1.0f - powf(pressurePa / groundPa, 0.19029495f)) : NAN;
  Serial.printf("B,%lu,%.2f,%.2f,%.2f\n", ul(now), pressurePa / 100.0f, temperatureC, altitudeM);
}
 
void serviceCommands() { //This function services the serial commands, reading from the serial port and executing the appropriate command
  while (Serial.available() > 0) {
    switch (Serial.read()) {
      case 'f': case 'F':
        accelerometer.setFifo(true);
        Serial.println("FIFO,stream_on");
        break;
      case 'b': case 'B':
        accelerometer.setFifo(false);
        Serial.println("FIFO,bypass_on");
        break;
      case 'r': case 'R':
        clearReference();
        Serial.println("REFERENCE,cleared");
        break;
    }
  }
}
 
void serviceReport(uint32_t now) { //This function services the report, printing the timing information for the barometer and accelerometer to the serial port
  if (!due(now, nextReportUs)) return;
  nextReportUs = now + kReportPeriodUs;
  barometer.reportTiming();
  accelerometer.reportTiming();
  Serial.printf("RATE,missed_accel=%lu,missed_baro=%lu,accel_hz=%lu,baro_hz=%lu,errors=0x%02X\n", ul(missedAccel),
                ul(missedBarometer), ul(accelCount - lastAccelCount), ul(barometerCount - lastBarometerCount),
                errorFlags);
  lastAccelCount = accelCount;
  lastBarometerCount = barometerCount;
  errorFlags = 0;
  if (!barometer.ready()) errorFlags |= kErrorBmpUnavailable;
  if (!accelerometer.ready()) errorFlags |= kErrorAdxlUnavailable;
}
}
 
void setup() { //Initialize
  Serial.begin(115200);
  if (loadReference()){
    Serial.printf("REFERENCE,loaded,pressure_pa=%.2f,frozen=%d\n", groundPa, static_cast<int>(referenceFrozen));
  }
  Wire.begin();
  Wire.setClock(kI2cClockHz);
  Wire1.begin();
  Wire1.setClock(kI2cClockHz);
  barometer.begin();
  accelerometer.begin();
  const uint32_t now = micros();
  nextAccelUs = nextBarometerUs = now;
  nextReportUs = now + kReportPeriodUs;
}
 
void loop() { //Main loop of the program
  const uint32_t now = micros();
  serviceCommands();
  serviceAccelerometer(now);
  serviceBarometer(now);
  Recovery(now);
  serviceReport(now);
}
