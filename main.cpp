/**
 * @file main.cpp
 * @author Matthew Yu (matthewjkyu@gmail.com)
 * @brief This program samples the TSL2591 irradiance sensor at 10 Hz on the
 * Blackbody B PCB. The result is either piped to CAN or serial UART through USB
 * to a host PCB for analysis.
 * @version 0.1
 * @date 2021-10-20
 * @copyright Copyright (c) 2021
 */
#include "mbed.h"
#include "TSL2591.hpp"
#include <cstdint>

#define __CAN__    0
#define __SERIAL__ 1
#define SENSOR_ID  0x630

#define I2C_SDA D0
#define I2C_SCL D1

static I2C i2c1(I2C_SDA, I2C_SCL);
static TSL2591 sensor(&i2c1, TSL2591_ADDR);
static DigitalOut led(LED1);
static DigitalOut led2(D3);
static CAN can(D10, D2);
static Ticker ticker;
static bool sampleFlag = false;

void tick(void) {
    led = !led;
    led2 = !led;
    sampleFlag = true;
}

int main() {
    sensor.init();
    sensor.enable();
    ticker.attach(&tick, 500ms);

    while (1) {
        if (sampleFlag) {
            // Sample sensor and normalize.
            sensor.getALS();
            sensor.calcLux();
            uint16_t ch0counts = sensor.full;
            uint16_t ch1counts = sensor.ir;

            // This particular metric comes from Re, Irradiance responsivity
            // from Figure TSL2591 – 9. 
            double ch0irradiance = (double) ch0counts / 6024; // uW/cm^2
            double ch1irradiance = (double) ch1counts / 1003; // uW/cm^2
            double avgIrradiance = (ch0irradiance + ch1irradiance) / 2; // uW/cm^2
            avgIrradiance *= 1000.0; // 1000 uW/cm^2
            avgIrradiance /= 100.0;  // 1000 W/m^2
            float irradiance = avgIrradiance;

            // Output result to external device.
#if __CAN__
            char buf[8] = { '\0' };
            sprintf(buf, "%.1f", sensor.lux*2.379);
            can.write(CANMessage(SENSOR_ID, buf, 8));
            // can.write(CANMessage(SENSOR_ID, (char *)&irradiance, 4));
#endif

#if __SERIAL__
            // Lux derived W/m^2 was sourced from Peter Michael, September 20,
            // 2019, "A Conversion Guide: Solar Irradiance and Lux Illuminance
            // ", IEEE Dataport, doi: https://dx.doi.org/10.21227/mxr7-p365. 
            printf(
                "W/m^2: %f\tLux: %f\tLux derived W/m^2: %f\n", 
                irradiance,
                sensor.lux * 2.379,
                sensor.lux * 2.379 * 0.0083333);
#endif

            sampleFlag = false;
        }

        ThisThread::sleep_for(50ms);
    }
}
