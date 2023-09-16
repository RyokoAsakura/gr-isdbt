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
/* BINDTOOL_HEADER_FILE(bit_deinterleaver.h)                                        */
/* BINDTOOL_HEADER_FILE_HASH(06ee8266c9fad6207e3de6946848a007)                     */
/***********************************************************************************/

#include <pybind11/complex.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

#include <gnuradio/isdbt/bit_deinterleaver.h>
// pydoc.h is automatically generated in the build directory
#include <bit_deinterleaver_pydoc.h>

void bind_bit_deinterleaver(py::module& m)
{

    using bit_deinterleaver    = ::gr::isdbt::bit_deinterleaver;


    py::class_<bit_deinterleaver, gr::sync_interpolator,
        std::shared_ptr<bit_deinterleaver>>(m, "bit_deinterleaver", D(bit_deinterleaver))

        .def(py::init(&bit_deinterleaver::make),
           py::arg("mode"),
           py::arg("segments"),
           py::arg("constellation_size"),
           D(bit_deinterleaver,make)
        )
        



        ;




}








