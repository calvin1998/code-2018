/*
 * MC_temperatures_1.cpp - CAN message parser: RMS Motor Controller temperatures 1 message
 * Created by Nathan Cheek, November 22, 2016.
 */

#include "HyTech17.h"

MC_temperatures_1::MC_temperatures_1() {
    message = {};
}

MC_temperatures_1::MC_temperatures_1(uint8_t buf[8]) {
    load(buf);
}

void MC_temperatures_1::load(uint8_t buf[8]) {
    message = {};
    memcpy(&(message.module_a_temperature), &buf[0], sizeof(int16_t));
    memcpy(&(message.module_b_temperature), &buf[2], sizeof(int16_t));
    memcpy(&(message.module_c_temperature), &buf[4], sizeof(int16_t));
    memcpy(&(message.gate_driver_board_temperature), &buf[6], sizeof(int16_t));
}
//testing purposes
void MC_temperatures_1::write(uint8_t buf[8]) {
    message = {};
    memcpy(&buf[0], &(message.module_a_temperature), sizeof(int16_t));
    memcpy(&buf[2], &(message.module_b_temperature), sizeof(int16_t));
    memcpy(&buf[4], &(message.module_c_temperature), sizeof(int16_t));
    memcpy(&buf[6], &(message.gate_driver_board_temperature), sizeof(int16_t));
}
//end of testing purposes

int16_t MC_temperatures_1::get_module_a_temperature() {
    return message.module_a_temperature;
}

int16_t MC_temperatures_1::get_module_b_temperature() {
    return message.module_b_temperature;
}

int16_t MC_temperatures_1::get_module_c_temperature() {
    return message.module_c_temperature;
}

int16_t MC_temperatures_1::get_gate_driver_board_temperature() {
    return message.gate_driver_board_temperature;
}

//Testing purposes only

void MC_temperatures_1::set_module_a_temperature(int16_t temp) {
    message.module_a_temperature = temp;
}

void MC_temperatures_1::set_module_b_temperature(int16_t temp) {
    message.module_b_temperature = temp;
}

void MC_temperatures_1::set_module_c_temperature(int16_t temp) {
    message.module_c_temperature = temp;
}

void MC_temperatures_1::set_gate_driver_board_temperature(int16_t temp) {
    message.gate_driver_board_temperature = temp;
}
