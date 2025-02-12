#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/sync.h"


#define MIN_INTERVAL 2000 // Minimum interval value (if DHT-11 is read faster than the MIN_INTERVAL it will return the previous value it read instead of requesting new values)
#define TIMEOUT UINT32_MAX

bool readDHT11();
uint32_t expectPulse(bool level);
uint32_t getHumidity();

uint8_t DHT11_PIN = 0;  // The GPIO (General Purpose Input/Output) pin on the Raspberry Pi Pico that is connected to the DHT-11 sensor pin

/* calculating max cycles
*  longest pulse from DHT-11 is 70us
*  Pico runs at 125mhz, meaning each cycle is 8 nanoseconds
*  (70 * 10^6 ns) / 8 ns = 8,750 cycles
*  set maxCycles to 10k to allow for some overhead (can be adjusted later)
*/
uint32_t maxCycles = 10000;

uint8_t data[5];    // creates an array of 5 8bit unsigned integers to store the raw humidity, temperature, and checkSum values
uint32_t lastReadTime = 0;
bool lastResult = 0;

int main() 
{
    stdio_init_all(); // initializes stdio system allowing for communication between the pico and the computer
    gpio_init(DHT11_PIN); // initializes the pin connected to the DHT-11 sensor

    sleep_ms(5000);
    printf("\nDHT Result: %d\n", readDHT11());

    while(1) {

    }
}

bool readDHT11()
{
    // check if sensor was read less than two seconds ago, if it has then return early to use the last reading
    uint32_t currentTime = to_ms_since_boot(get_absolute_time());   // stores the current time in milliseconds

    if(currentTime - lastReadTime < MIN_INTERVAL) { // checks if sensor was read less than MIN_INTERVAL ms ago 
        return lastResult;                          // if previous statement is true then return the last correct measurement
    }
    lastReadTime = currentTime; // updates lastReadTime to represent the most recent sensor read

    for(int i = 0; i < sizeof(data) / sizeof(data[0]); i++) { // resets every value in data to 0
        data[i] = 0;
    }

    gpio_set_dir(DHT11_PIN, GPIO_IN);   // sets the DHT-11 GPIO pin to behave as an input
    gpio_pull_up(DHT11_PIN);    // sets the default value of the DHT-11 GPIO pin to HIGH(1)
    sleep_ms(1);

    // First set the data line to low(0) for 20 ms (data sheet says at least 18ms so using 20ms to be safe)
    gpio_set_dir(DHT11_PIN, GPIO_OUT);
    gpio_put(DHT11_PIN, 0);
    sleep_ms(20);

    uint32_t cycles[80];

    gpio_set_dir(DHT11_PIN, GPIO_IN);   // sets the DHT-11 GPIO pin to behave as an input
    gpio_pull_up(DHT11_PIN);    // sets the default value of the DHT-11 GPIO pin to HIGH(1)
    while(gpio_get(DHT11_PIN) == 1); // waits until receiving a low signal from dht-11

    // temporarily turn off interrrupts because the next section of code is timing critical
    uint32_t irq_status = save_and_disable_interrupts();    // Disables interrupts and saves the current interrupt state

    // expect a low signal for about 54us
    if (expectPulse(0) == TIMEOUT) {
        printf("ERROR: Timeout waiting for start signal low pulse");
        lastResult = false;
        return lastResult;
    }
    // expect a high signal for about 80us
    if (expectPulse(1) == TIMEOUT) {
        printf("ERROR: Timeout waiting for start signal high pulse");
        lastResult = false;
        return lastResult;
    }

    /*  Read the 40 bits sent by the sensor. Each bit is sent by a ~54us low pulse followed by a variable length high pulse.
    *   If the high pulse is ~24us then it's a 0 and if it's ~70us then it is a 1. 
    *   By measuring the cycle count of the initial 50us low pulse and use that to compare to the cycle count of the high pulse
    *   we can determine if the bit is a 0 (high state cycle count < low state cycle count), or a 1 (high state cycle count > low state cycle count).
    */
    for(int i = 0; i < 80; i += 2) {
        cycles[i] = expectPulse(0);
        cycles[i+1] = expectPulse(1);
    }

    restore_interrupts(irq_status); // timing critical code is complete so restore the interrupt state

    printf("low cycles: ");
    for(int i = 0; i < 40; i++) {
        printf("[%d]", cycles[2 * i]);
    }
    printf("\nHigh cycles: ");
    for(int i = 0; i < 40; i++) {
        printf("[%d]", cycles[2 * i] + 1);
    }

    // inspect the pulses and determine which ones are 0 or 1
    for(int i = 0; i < 40; i++) {
        uint32_t lowCycles = cycles[2 * i];
        uint32_t highCycles = cycles[2 * i + 1];
        if((lowCycles == TIMEOUT) || (highCycles == TIMEOUT)) {
            printf("ERROR: Timeout waiting for pulse");
            lastResult = false;
            return lastResult;
        }
        data[i/8] <<= 1; // perform a left bitshift on data[i/8]
        
        // compare the low and high cycle times to see if the bit is a 0 or 1
        if(highCycles > lowCycles) {
            // High cycles are greater than 50us low cycle count, the bit must be a 1
            data[i/8] |= 1;
        }

        // check that 40 bits were read and that the checksum matches
        if (data[4] == ((data[0] + data[1] + data[2] + data[3]) & 0xFF)) {
            lastResult = true;
            return lastResult;
        }
        else {
            printf("ERROR: DHT checksum failure");
            lastResult = false;
            return lastResult;
        }
    }
}

uint32_t expectPulse(bool level) {  // returns the number of cycles it takes
    uint32_t count = 0;

    while(gpio_get(DHT11_PIN) == level) {
        if(count++ >= maxCycles) {
            return TIMEOUT;
        }
    }

    return count;
}

uint32_t getHumidity() {
    printf("Humidity int: %d\n",data[0]);
    printf("Humidity fraction: %d\n",data[1]);
    return 0;
}