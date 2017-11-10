#include <chrono>
#include <ctime>
#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <signal.h>
#include <sys/time.h>

#include "can_lib.h"
#include "bt_lib.h"
#include "../../../Libraries/HyTech17_Library/HyTech17.h"

#define RED_CONSOLE "\033[31;1m"
#define GREEN_CONSOLE "\033[32;1m"
#define YELLOW_CONSOLE "\033[33;1m"
#define BLUE_CONSOLE "\033[34;1m"
#define NO_COLOR_CONSOLE "\033[0m"

#define TIME_LEFT_HIST_LEN 100

const std::string CONSOLE_COLORS[] = {RED_CONSOLE, GREEN_CONSOLE,
                                      YELLOW_CONSOLE, BLUE_CONSOLE};

// variables
struct timeval prev_time;
struct timeval curr_time;
uint16_t amp_history[TIME_LEFT_HIST_LEN];
uint8_t amp_insert_pos = 0;
int running_amp_sum = 1;

// function prototypes
int process_data_for_sending(uint8_t* bt_data, canframe_t* frame);
void print(canframe_t* frame);
void log_to_file(canframe_t* frame);

// Define output file streams and time variables
std::ofstream log_mc;
std::ofstream log_bms;
std::ofstream log_main_ard;
std::ofstream log_evdc;
std::ofstream log_rear_ard;
struct timeval log_mc_prev_time;
struct timeval log_bms_prev_time;
struct timeval log_main_ard_prev_time;
struct timeval log_evdc_prev_time;
struct timeval log_rear_ard_prev_time;
char timestr[20];

std::string base_log_dir = "/home/pi/logs/";

int main() {
    // Open the output file streams
    log_mc.open(base_log_dir + "logs_mc.txt", std::ios::out | std::ios::app);
    log_bms.open(base_log_dir + "logs_bms.txt", std::ios::out | std::ios::app);
    log_main_ard.open(base_log_dir + "logs_pcu.txt",
            std::ios::out | std::ios::app);
    log_evdc.open(base_log_dir + "logs_tcu.txt",
            std::ios::out | std::ios::app);
    log_rear_ard.open(base_log_dir + "logs_dashboard.txt",
            std::ios::out | std::ios::app);

    // Initialize previous time values to current time
    gettimeofday(&log_mc_prev_time, NULL);
    gettimeofday(&log_bms_prev_time, NULL);
    gettimeofday(&log_main_ard_prev_time, NULL);
    gettimeofday(&log_evdc_prev_time, NULL);
    gettimeofday(&log_rear_ard_prev_time, NULL);

    // Initialize CAN and Bluetooth libraries
    CAN can;
    BT bt(10); // use port 10
    bt.connect();

    // Zero out amp history, initialize current/previous time variables
    bzero(amp_history, TIME_LEFT_HIST_LEN * sizeof(uint16_t));
    gettimeofday(&prev_time, NULL);
    curr_time = prev_time;

    // Initialize CAN frame and Bluetooth data buffer
    // canframe_t struct:
    //      uint32_t can_id
    //      uint8_t  can_dlc -- length of data (0..8)
    //      uint8_t  data[8] -- up to 8 bytes of data
    canframe_t *frame = (canframe_t*) malloc(sizeof(canframe_t));
    uint8_t bt_buffer[BT::DATA_LENGTH];

    while (1) {
        // continually read messages off the CAN bus
        if (can.read(frame) > 0) {
            std::cout << "Error reading message or no message to read" << std::endl;
        } else {
            frame->can_id = frame->can_id & 0xFF;
            if (0 == process_data_for_sending(bt_buffer, frame)) {
                print(frame);
                if (bt.send(bt_buffer) == -1) {
                    std::cout << "Attempting to reconnect" << std::endl;
                    bt.connect();
                }
            }
            log_to_file(frame);
        }
    }

    // cleanup
    free(frame);
    bt.disconnect();
    log_mc.close();
    log_bms.close();
    log_main_ard.close();
    log_evdc.close();
    log_rear_ard.close();
    return 0;
}

/**
 * Processes CAN frame to extract useful information and load into Bluetooth
 * data frame.
 * params:
 *  uint8_t *bt_data: empty bluetooth data frame
 *  canframe_t *frame: CAN frame containing data we want to extract
 */
int process_data_for_sending(uint8_t* bt_data, canframe_t* frame) {
    uint16_t value;
    bzero(bt_data, BT::DATA_LENGTH);
    switch(frame->can_id) {
        case ID_BMS_TEMPERATURE:
        {
            // High and Avg Battery Temp (0xDA)
            bt_data[0] = 1;
            BMS_temperatures bms_temperatures(frame->data);
            value = bms_temperatures.getHighTemp();
            memcpy(&bt_data[1], &value, sizeof(uint16_t));
            value = bms_temperatures.getAvgTemp();
            memcpy(&bt_data[3], &value, sizeof(uint16_t));
            break;
        }
        case ID_PCU_STATUS:
        {
            // PCU Status (0xD0, 0)
            bt_data[0] = 2;
            PCU_status pcu_status(frame->data);
            bt_data[1] = pcu_status.get_state();
            bt_data[2] = (pcu_status.get_bms_fault() << 3) | (pcu_status.get_imd_fault() << 2)
                | (pcu_status.get_okhs_value() << 1) | pcu_status.get_discharge_ok_value();
            break;
        }
        case ID_PCU_VOLTAGES:
        {
            // Rear Low Voltage Voltage (0xD1, 2)
            bt_data[0] = 6;
            PCU_voltages pcu_voltages(frame->data);
            value = pcu_voltages.get_GLV_battery_voltage();
            memcpy(&bt_data[1], &value, sizeof(uint16_t));
            break;
        }
        case ID_TCU_STATUS:
        {
            // TCU Status (0xD2)
            bt_data[0] = 8;
            TCU_status tcu_status(frame->data);
            bt_data[1] = tcu_status.get_state();
            bt_data[2] = (tcu_status.get_throttle_implausibility() << 3) | (tcu_status.get_throttle_curve() << 2)
                | (tcu_status.get_brake_implausibility() << 1) | tcu_status.get_brake_pedal_active();
            break;
        }
        case ID_MC_TEMPERATURES_3:
        {
            // Motor Temp (0xA2)
            MC_temperatures_3 mc_temps(frame->data);
            value = mc_temps.get_motor_temperature() / 10;
            bt_data[0] = 3;
            memcpy(&bt_data[1], &value, sizeof(value));
            break;
        }
        case ID_MC_MOTOR_POSITION_INFORMATION:
        {
            // Speed (0xA5)
            // RPM * (16/35) * 5.2 = feet / min
            // * 60/5280 = mph
            // Multiply by 10 to send
            // Circumference = 5.2 ft
            MC_motor_position_information position_info(frame->data);
            value = position_info.get_motor_speed();
            value = (uint16_t) (value * (16.0/35) * 5.2 * (60.0/5280));
            bt_data[0] = 4;
            memcpy(&bt_data[1], &value, sizeof(value));
            break;
        }
        case ID_MC_CURRENT_INFORMATION:
        {
            // Current Draw (0xA6)
            MC_current_information current_info(frame->data);
            value = current_info.get_dc_bus_current();
            bt_data[0] = 5;
            memcpy(&bt_data[1], &value, sizeof(value));
            break;
        }
        case ID_TCU_READINGS:
        {
            // Pedal Values (0xD3)
            bt_data[0] = 7;
            TCU_readings tcu_readings(frame->data);
            value = tcu_readings.get_throttle_value_avg();
            memcpy(&bt_data[1], &value, sizeof(uint16_t));
            value = tcu_readings.get_brake_value();
            memcpy(&bt_data[3], &value, sizeof(uint16_t));
            break;
        }
        default:
            return -1;
    }
    return 0;
}

/**
 * Prints CAN ID and data to the terminal.
 * params:
 *  canframe_t *frame: the CAN frame to print
 */
void print(canframe_t* frame) {
    if (frame && frame->can_dlc > 0) {
        // frame is not null and has positive length
        printf("%x: ", frame->can_id);
        for (uint8_t i = 0; i < frame->can_dlc; ++i) {
            //std::cout << CONSOLE_COLORS[rand() % 4];
            std::cout << (int) frame->data[i] << " ";
        }
        std::cout << NO_COLOR_CONSOLE << std::endl;
    } else {
        std::cout << "Can frame is NULL or has an empty message" << std::endl;
    }
}

/**
 * Logs CAN data to file with timestamp. Chooses appropriate file based on
 * CAN ID.
 * This function could use a complete rewrite.
 * params:
 *  canframe_t *frame: the frame to be written to file
 */
void log_to_file(canframe_t* frame) {
    std::ofstream* out_file;
    gettimeofday(&curr_time, NULL);
    if (frame->can_id < 0x10
            && (curr_time.tv_sec - log_bms_prev_time.tv_sec) > 0) {
        out_file = &log_bms;
        log_bms_prev_time = curr_time;
    } else if (frame->can_id < 0x30
            && (curr_time.tv_sec - log_rear_ard_prev_time.tv_sec) > 0) {
        out_file = &log_rear_ard;
        log_rear_ard_prev_time = curr_time;
    } else if (frame->can_id < 0x90
            && (curr_time.tv_sec - log_main_ard_prev_time.tv_sec) > 0) {
        out_file = &log_main_ard;
        log_main_ard_prev_time = curr_time;
    } else if (frame->can_id < 0xB0
            && (curr_time.tv_sec - log_mc_prev_time.tv_sec) > 0) {
        out_file = &log_mc;
        log_mc_prev_time = curr_time;
    } else if (frame->can_id < 0xD0
            && (curr_time.tv_sec - log_evdc_prev_time.tv_sec) > 0) {
        out_file = &log_evdc;
        log_evdc_prev_time = curr_time;
    } else {
        return;
    }

    std::time_t timestamp = std::chrono::system_clock
        ::to_time_t(std::chrono::system_clock::now());
    if (std::strftime(timestr, sizeof(timestr), "%T",
                std::localtime(&timestamp))) {
        *out_file << timestr << ": ";
    }
    *out_file << frame->can_id << ": ";
    for (uint8_t i = 0; i < 8; ++i) {
        uint8_t data = frame->data[i];
        *out_file << (int) data << " ";
    }
    *out_file << std::endl;
}
