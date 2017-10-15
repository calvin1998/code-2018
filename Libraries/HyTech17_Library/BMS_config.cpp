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

BMS_config::BMS_config(uint8_t addr, uint8_t wflag, short val) {
    setAddress(addr);
    setValue(val);
    setWriteFlag(wflag);
}

void BMS_config::load(uint8_t buf[]) {
    bmsConfigMessage = {};
    memcpy(&(bmsConfigMessage.address), &buf[0], sizeof(uint8_t));
    memcpy(&(bmsConfigMessage.writeFlag), &buf[1], sizeof(uint8_t));
    memcpy(&(bmsConfigMessage.value), &buf[2], sizeof(short));
}

void BMS_config::write(uint8_t buf[]) {
    memcpy(&buf[0], &(bmsConfigMessage.address), sizeof(uint8_t));
    memcpy(&buf[1], &(bmsConfigMessage.writeFlag), sizeof(uint8_t));
    memcpy(&buf[2], &(bmsConfigMessage.value), sizeof(short));
}

uint8_t BMS_config::getAddress() {
    return bmsConfigMessage.address;
}

short BMS_config::getValue() {
    return bmsConfigMessage.value;
}

uint8_t BMS_config::isWrite() {
    return bmsConfigMessage.writeFlag;
}

void BMS_config::setAddress(uint8_t addr) {
    bmsConfigMessage.address = addr;
}

void BMS_config::setValue(short val) {
    bmsConfigMessage.value = val;
}

void BMS_config::setWriteFlag(uint8_t wflag) {
    bmsConfigMessage.writeFlag = wflag;
}