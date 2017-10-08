/*
 * Python wrapper for the BMS part of the HyTech library.
 *
<%
pybind11_setup(cfg)
%>
*/

#include "HyTech17.h"
#include "BMS_status.cpp"
// #include <BMS_currents.cpp>

#include <pybind11/pybind11.h>

namespace py = pybind11;

PYBIND11_MODULE(PyBMS, m)
{
    // BMS Status
    py::class_<BMS_status>(m, "BMS_status")
        .def(py::init<>());
    // status.def(py::init<uint8_t[]>());
    // status.def("load", &BMS_status::load);
    // status.def("write", &BMS_status::write);
    // status.def("getOvervoltage", &BMS_status::getOvervoltage);
    // status.def("getUndervoltage", &BMS_status::getUndervoltage);
    // status.def("getTotalVoltageHigh", &BMS_status::getTotalVoltageHigh);
    // status.def("getDischargeOvercurrent", &BMS_status::getDischargeOvercurrent);
    // status.def("getChargeOvercurrent", &BMS_status::getChargeOvercurrent);
    // status.def("getDischargeOvertemp", &BMS_status::getDischargeOvertemp);
    // status.def("getChargeOvertemp", &BMS_status::getChargeOvertemp);
    // status.def("getBMSStatusOK", &BMS_status::getBMSStatusOK);
    // status.def("setOvervoltage", &BMS_status::setOvervoltage);
    // status.def("setUnderVoltage", &BMS_status::setUnderVoltage);
    // status.def("setTotalVoltageHigh", &BMS_status::setTotalVoltageHigh);
    // status.def("setDischargeOvercurrent", &BMS_status::setDischargeOvercurrent);
    // status.def("setChargeOvercurrent", &BMS_status::setChargeOvercurrent);
    // status.def("setDischargeOvertemp", &BMS_status::setDischargeOvertemp);
    // status.def("setChargeOvertemp", &BMS_status::setChargeOvertemp);

    // // BMS Current
    // py::class_<BMS_currents> currents(m, "BMS_currents");
    // currents.def(py::init<>());
    // currents.def(py::init<uint8_t[]>());
    // currents.def(py::init<float, uint8_t);
    // currents.def("load", &BMS_currents::load);
    // currents.def("write", &BMS_currents::write);
    // currents.def("getCurrent", &BMS_currents::getCurrent);
    // currents.def("getChargingState", &BMS_currents::getChargingState);
    // currents.def("setCurrent", &BMS_currents::setCurrent);
    // currents.def("setChargingState", &BMS_currents::setChargingState);
}
