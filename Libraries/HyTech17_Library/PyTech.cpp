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
// #include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(PyTech, m)
{
    // BMS_status
    py::class_<BMS_status>(m, "BMS_status")
        .def(py::init<>())
        // .def(py::init<std::uint8_t[]>())
        .def("load", &BMS_status::load)
        .def("write", &BMS_status::write)
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
