/*
 * Copyright 2022 Free Software Foundation, Inc.
 *
 * This file is part of GNU Radio
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

/***********************************************************************************/
/* This file is automatically generated using bindtool and can be manually edited  */
/* The following lines can be configured to regenerate this file during cmake      */
/* If manual edits are made, the following tags should be modified accordingly.    */
/* BINDTOOL_GEN_AUTOMATIC(0)                                                       */
/* BINDTOOL_USE_PYGCCXML(0)                                                        */
/* BINDTOOL_HEADER_FILE(ofdm_synchronization.h)                                        */
/* BINDTOOL_HEADER_FILE_HASH(868e570369c1b442bf6f731a0a028891)                     */
/***********************************************************************************/

#include <pybind11/complex.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

#include <gnuradio/isdbt/ofdm_synchronization.h>
// pydoc.h is automatically generated in the build directory
#include <ofdm_synchronization_pydoc.h>

void bind_ofdm_synchronization(py::module& m)
{

    using ofdm_synchronization    = ::gr::isdbt::ofdm_synchronization;


    py::class_<ofdm_synchronization, gr::block, gr::basic_block,
        std::shared_ptr<ofdm_synchronization>>(m, "ofdm_synchronization", D(ofdm_synchronization))

        .def(py::init(&ofdm_synchronization::make),
           py::arg("mode"),
           py::arg("cp_length"),
           py::arg("interpolate"),
           D(ofdm_synchronization,make)
        )
        



        ;




}








