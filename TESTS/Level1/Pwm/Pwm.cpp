/*
 * Copyright (c) 2016 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#if !DEVICE_PWMOUT
  #error [NOT_SUPPORTED] PWMOUT not supported on this platform, add 'DEVICE_PWMOUT' definition to your platform.
#endif

#include "cmsis.h"
#include "pinmap.h"
#include "PeripheralPins.h"

#include "mbed.h"
#include "greentea-client/test_env.h"
#include "unity.h"
#include "utest.h"
#include "ci_test_config.h"

#include "TestFramework.hpp"
#include <vector>

using namespace utest::v1;

// Static variables for managing the dynamic list of pins
std::vector< vector <PinMap> > TestFramework::pinout(TS_NC);
std::vector<int> TestFramework::pin_iterators(TS_NC);
Timer timer;

// Initialize a test framework object
TestFramework test_framework;

int rise_count;
int fall_count;
int last_rise_time;
int duty_count;

utest::v1::status_t test_setup(const size_t number_of_cases) {
    GREENTEA_SETUP(30, "default_auto");
    return verbose_test_setup_handler(number_of_cases);
}

utest::v1::status_t greentea_failure_handler(const Case *const source, const failure_t reason) {
    greentea_case_failure_abort_handler(source, reason);
    return STATUS_ABORT;
}

void callback_pwm_rise(void) {
    rise_count++;
    last_rise_time = timer.read_ms();
}

void callback_pwm_fall(void) {
    fall_count++;
    if (last_rise_time != 0)
        duty_count = duty_count + (timer.read_ms() - last_rise_time);
}

void test_pwm_execute(PinName pin, float dutycycle, int period) {
	DEBUG_PRINTF("Running pwm test on pin %d\n", pin);
    TEST_ASSERT_MESSAGE(pin != NC, "Pin is NC");

    float tolerance = 0.05;
    int iterations = 20;
    float calculated_percent = iterations * tolerance;
    if (calculated_percent < 1) calculated_percent = 1.0f;

    // Initialize PWM, InterruptIn, Timer, and Rising / Falling edge counts
    fall_count = 0;
    rise_count = 0;
    last_rise_time = 0;
    duty_count = 0;
    PwmOut pwm(pin);

    timer.reset();
    InterruptIn iin(TestFramework::find_pin_pair(pin));
    iin.rise(callback_pwm_rise);
    iin.fall(callback_pwm_fall);

    DEBUG_PRINTF("Period set to: %f, duty cycle set to: %f\n", (float)period/1000, dutycycle);
    pwm.period((float)period/1000);
    
    //Start Testing
    pwm.write(0);
    DEBUG_PRINTF("Waiting for %dms\n", iterations * period);
    timer.start();
    pwm.write(dutycycle); // 50% duty cycle
    wait_ms(iterations * period); // wait for pwm to run and counts to add up
    pwm.write(0);
    iin.disable_irq(); // This is here because otherwise it fails on some platforms
    timer.stop();
    int rc = rise_count; // grab the numbers to work with as the pwm may continue going
    int fc = fall_count;
    float avgTime = (float)duty_count / iterations;
    float expectedTime = (float)period * dutycycle;
    DEBUG_PRINTF("Expected time: %f, Avg Time: %f\n", expectedTime, avgTime);
    DEBUG_PRINTF("rise count = %d, fall count = %d, expected count = %d\n", rc, fc, iterations);
    TEST_ASSERT_MESSAGE( std::abs(rc-fc) <= calculated_percent, "There was more than a specific variance in number of rise vs fall cycles");
    TEST_ASSERT_MESSAGE( std::abs(iterations - rc) <= calculated_percent, "There was more than a specific variance in number of rise cycles seen and number expected.");
    TEST_ASSERT_MESSAGE( std::abs(iterations - fc) <= calculated_percent, "There was more than a specific variance in number of fall cycles seen and number expected.");
    // TEST_ASSERT_MESSAGE( std::abs(expectedTime - avgTime) <= calculated_percent,"Greater than a specific variance between expected and measured duty cycle");
}

template <int dutycycle, int period> 
utest::v1::control_t test_level1_pwm(const size_t call_count) {
	return TestFramework::test_level1_framework(TestFramework::PWM, TestFramework::CITS_PWM, &test_pwm_execute, dutycycle*0.01f, period);
}

Case cases[] = {
	Case("Level 1 - PWM Frequency test (single pin) - 10ms", test_level1_pwm<50, 10>, greentea_failure_handler),
	Case("Level 1 - PWM Frequency test (single pin) - 30ms", test_level1_pwm<50, 30>, greentea_failure_handler),
	// Case("Level 1 - PWM Frequency test (single pin) - 100ms", test_level1_pwm<50, 100>, greentea_failure_handler),
	Case("Level 1 - PWM Duty cycle test (single pin) - 10%", test_level1_pwm<10, 10>, greentea_failure_handler),
	Case("Level 1 - PWM Duty cycle test (single pin) - 50%", test_level1_pwm<50, 10>, greentea_failure_handler),
	Case("Level 1 - PWM Duty cycle test (single pin) - 90%", test_level1_pwm<90, 10>, greentea_failure_handler),
};

int main() {
	// Formulate a specification and run the tests based on the Case array
	Specification specification(test_setup, cases);
    return !Harness::run(specification);
}