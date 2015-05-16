#/******************************************************************************
#*
#* Copyright (C) 2015 Xilinx, Inc.  All rights reserved.
#*
#* Permission is hereby granted, free of charge, to any person obtaining a copy
#* of this software and associated documentation files (the "Software"), to deal
#* in the Software without restriction, including without limitation the rights
#* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
#* copies of the Software, and to permit persons to whom the Software is
#* furnished to do so, subject to the following conditions:
#*
#* The above copyright notice and this permission notice shall be included in
#* all copies or substantial portions of the Software.
#*
#* Use of the Software is limited solely to applications:
#* (a) running on a Xilinx device, or
#* (b) that interact with a Xilinx device through a bus or interconnect.
#*
#* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
#* XILINX  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
#* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
#* OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
#* SOFTWARE.
#*
#* Except as contained in this notice, the name of the Xilinx shall not be used
#* in advertising or otherwise to promote the sale, use or other dealings in
#* this Software without prior written authorization from Xilinx.
#*
#******************************************************************************/

proc swapp_get_name {} {
	return "ZynqMP PMU Firmware";
}

proc swapp_get_description {} {
	return "Platform Management Unit Firmware for ZynqMP.";
}

proc check_standalone_os {} {
	set oslist [get_os];

	if { [llength $oslist] != 1 } {
		return 0;
	}
	set os [lindex $oslist 0];

	if { $os != "standalone" } {
		error "This application is supported only on the Standalone Board Support Package.";
	}
}

proc swapp_is_supported_sw {} {
	return 1;
}

proc swapp_is_supported_hw {} {
	# check processor type
	set proc_instance [get_sw_processor];
	set hw_processor [get_property HW_INSTANCE $proc_instance]
	set proc_type [get_property IP_NAME [get_cells $hw_processor]];

	if {($proc_type != "psu_microblaze")} {
		error "This application is supported only for PMU Microblaze processor (psu_microblaze).";
	}

	return 1;
}

proc get_stdout {} {
	set os [get_os];
	set stdout [get_property CONFIG.STDOUT $os];
	return $stdout;
}

proc swapp_generate {} {
	# PMU Firmware uses its own startup file. so set the -nostartfiles flag
	set_property  -name APP_LINKER_FLAGS -value {-nostartfiles} -objects [current_sw_design]
	# Set PMU Microblaze HW related compiler flags
	set_property  -name APP_COMPILER_FLAGS -value {-mlittle-endian -mxl-barrel-shift -mxl-pattern-compare -mcpu=v9.2 -mxl-soft-mul} -objects [current_sw_design]
}

proc swapp_get_linker_constraints {} {
	# don't generate a linker script. PMU Firmware has its own linker script
	return "lscript no";
}
