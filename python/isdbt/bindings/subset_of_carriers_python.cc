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
/* BINDTOOL_HEADER_FILE(subset_of_carriers.h)                                        */
/* BINDTOOL_HEADER_FILE_HASH(673fa4f7dbe839ccbeef1498bd9c1cb6)                     */
/***********************************************************************************/

#include <pybind11/complex.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

#include <gnuradio/isdbt/subset_of_carriers.h>
// pydoc.h is automatically generated in the build directory
#include <subset_of_carriers_pydoc.h>

void bind_subset_of_carriers(py::module& m)
{

    using subset_of_carriers    = ::gr::isdbt::subset_of_carriers;


    py::class_<subset_of_carriers, gr::sync_block, gr::block, gr::basic_block,
        std::shared_ptr<subset_of_carriers>>(m, "subset_of_carriers", D(subset_of_carriers))

        .def(py::init(&subset_of_carriers::make),
           py::arg("total"),
           py::arg("first"),
           py::arg("last"),
           D(subset_of_carriers,make)
        )
        



        ;




}








