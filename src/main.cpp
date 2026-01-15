
#include "mbed.h"
#include <stdio.h>

// Global variables for sensor data
float global_lux = 0;
int global_temp = 0;
int global_hum = 0;

// Task 1: Bare-Metal ADC Photoresistor Read
void task_photoresistor() {
    // ADC Init: PA0
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
    GPIOA->MODER |= (3U << 0); // Analog mode
    ADC1->CR2 |= ADC_CR2_ADON;

    while (true) {
        ADC1->CR2 |= ADC_CR2_SWSTART;
        while (!(ADC1->SR & ADC_SR_EOC));
        uint16_t raw = ADC1->DR;
        
        float voltage = (raw * 3.3f) / 4095.0f;
        if (voltage > 0) {
            float r_ldr = (10000.0f * (3.3f - voltage)) / voltage;
            global_lux = 500.0f / (r_ldr / 1000.0f);
        }
        ThisThread::sleep_for(1000ms);
    }
}

// Helper to set PA1 to Output
void PA1_Output() {
    GPIOA->MODER &= ~(3U << (1 * 2)); // Clear bits
    GPIOA->MODER |=  (1U << (1 * 2)); // Set as Output
}

// Helper to set PA1 to Input
void PA1_Input() {
    GPIOA->MODER &= ~(3U << (1 * 2)); // Set as Input (00)
}

void task_dht11() {
    Timer t;
    uint8_t bits[5];

    while (true) {
        uint8_t checksum = 0;
        memset(bits, 0, 5);

        // 1. Start Signal
        PA1_Output();
        GPIOA->ODR &= ~(1U << 1); // Pull Low
        ThisThread::sleep_for(18ms);  
        GPIOA->ODR |= (1U << 1);  // Pull High
        wait_us(30);
        PA1_Input();

        // 2. Wait for DHT11 Response (Low 80us, then High 80us)
        t.reset(); t.start();
        while(!(GPIOA->IDR & (1U << 1))) { if(t.elapsed_time().count() > 100) break; } // Wait for high
        t.reset();
        while( (GPIOA->IDR & (1U << 1))) { if(t.elapsed_time().count() > 100) break; } // Wait for low

        // 3. Read 40 Bits
        for (int i = 0; i < 40; i++) {
            // Wait for pin to go high (start of bit)
            while(!(GPIOA->IDR & (1U << 1)));
            t.reset();
            
            // Wait for pin to go low (end of bit)
            while( (GPIOA->IDR & (1U << 1)));
            
            // If pulse duration > 40us, it's a '1', else '0'
            if (t.elapsed_time().count() > 40) {
                bits[i / 8] |= (1 << (7 - (i % 8)));
            }
        }

        // 4. Verify Checksum and Update Globals
        checksum = bits[0] + bits[1] + bits[2] + bits[3];
        if (checksum == bits[4] && checksum != 0) {
            global_hum = bits[0];
            global_temp = bits[2];
        }

        // DHT11 requires at least 2 seconds between samples
        ThisThread::sleep_for(2000ms);
    }
}

// Task 3: Serial Output (PuTTY)
void task_serial_output() {
    static BufferedSerial pc(USBTX, USBRX, 115200);
    char buf[100];

    while (true) {
        int len = snprintf(buf, sizeof(buf), 
            "LDR: %.2f Lux | Temp: %d C | Hum: %d %%\r\n", 
            global_lux, global_temp, global_hum);
        pc.write(buf, len);
        ThisThread::sleep_for(1000ms);
    }
}
using namespace std::chrono_literals;
int main() {
    Thread t1, t2, t3;
    t1.start(task_photoresistor);
    t2.start(task_dht11);
    t3.start(task_serial_output);
    
    while (true) { ThisThread::sleep_for(1s); }
}
