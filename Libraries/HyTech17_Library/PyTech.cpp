/*
 * Python wrapper for the BMS part of the HyTech library.
 *
<%
pybind11_setup(cfg)
%>
*/

#include "HyTech17.h"
#include "BMS_status.cpp"
#include "BMS_currents.cpp"

#include <pybind11/pybind11.h>
// #include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(PyTech, m)
{
    // NOTE: uint8_t array constructor doesn't compile, and write and load functionality is ...misunderstood(?) - look at https://trello.com/c/IumyUtGR

    // BMS_status
    py::class_<BMS_status>(m, "BMS_status")
        .def(py::init<>())
        // .def(py::init<std::uint8_t[]>())
        // .def("load", &BMS_status::load)
        // .def("write", &BMS_status::write)
        .def("getOvervoltage", &BMS_status::getOvervoltage)
        .def("getUndervoltage", &BMS_status::getUndervoltage)
        .def("getTotalVoltageHigh", &BMS_status::getTotalVoltageHigh)
        .def("getDischargeOvercurrent", &BMS_status::getDischargeOvercurrent)
        .def("getChargeOvercurrent", &BMS_status::getChargeOvercurrent)
        .def("getDischargeOvertemp", &BMS_status::getDischargeOvertemp)
        .def("getChargeOvertemp", &BMS_status::getChargeOvertemp)
        .def("getBMSStatusOK", &BMS_status::getBMSStatusOK)
        .def("setOvervoltage", &BMS_status::setOvervoltage)
        .def("setUndervoltage", &BMS_status::setUndervoltage)
        .def("setTotalVoltageHigh", &BMS_status::setTotalVoltageHigh)
        .def("setDischargeOvercurrent", &BMS_status::setDischargeOvercurrent)
        .def("setChargeOvercurrent", &BMS_status::setChargeOvercurrent)
        .def("setDischargeOvertemp", &BMS_status::setDischargeOvertemp)
        .def("setChargeOvertemp", &BMS_status::setChargeOvertemp)
        .def("setBMSStatusOK", &BMS_status::setBMSStatusOK);

    // BMS_currents
    py::class_<BMS_currents>(m, "BMS_currents")
        .def(py::init<>())
        // .def(py::init<uint8_t[]>())
        // .def(py::init<float, uint8_t)
        // .def("load", &BMS_currents::load)
        // .def("write", &BMS_currents::write)
        .def("getCurrent", &BMS_currents::getCurrent)
        .def("getChargingState", &BMS_currents::getChargingState)
        .def("setCurrent", &BMS_currents::setCurrent)
        .def("setChargingState", &BMS_currents::setChargingState);
    
    // BMS_temperatures
    // BMS_voltages
    
    // TODO: MC messages
    // TODO: PCU messages

}
