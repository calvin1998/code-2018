/*
 * BMS_config.cpp
 * Allows remote setting of config variables.
 * Ryan Gallaway - Sep 12, 2017
 */
#include <HyTech17.h>

BMS_config::BMS_config() {
    bmsConfigMessage = {};
}

BMS_config::BMS_config(uint8_t buf[]) {
    load(buf);
}

BMS_config::BMS_config(uint8_t addr, short val) {
    setAddress(addr);
    setValue(val);
}

void BMS_config::load(uint8_t buf[]) {
    bmsConfigMessage = {};
    memcpy(&(bmsConfigMessage.address), &buf[0], sizeof(uint8_t));
    memcpy(&(bmsConfigMessage.value), &buf[1], sizeof(short));
}

void BMS_config::write(uint8_t buf[]) {
    memcpy(&buf[0], &(bmsConfigMessage.address), sizeof(uint8_t));
    memcpy(&buf[1], &(bmsConfigMessage.value), sizeof(short));
}

uint8_t BMS_config::getAddress() {
    return bmsConfigMessage.address;
}

short BMS_config::getValue() {
    return bmsConfigMessage.value;
}

void BMS_config::setAddress(uint8_t addr) {
    bmsConfigMessage.address = addr;
}

void BMS_config::setValue(short val) {
    bmsConfigMessage.value = val;
}