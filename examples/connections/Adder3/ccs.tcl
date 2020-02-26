set sfd [file dir [info script]]

## Make sure the timing annotation file is local
if { "[file normalize $sfd]" ne "[file normalize [pwd]]" } {
  if { ![file exists ./my_testbench.input.json] } {
    file copy "$sfd/my_testbench.input.json" .
  }
}

solution new -state initial
solution options defaults
solution options set /Input/CompilerFlags {-DHLS_CATAPULT -DCONNECTIONS_ACCURATE_SIM}

flow package require /SCVerify
solution file add "$sfd/Adder3.h" -type CHEADER
solution file add "$sfd/testbench.cpp" -type C++ -exclude true

go analyze
directive set -DESIGN_HIERARCHY Adder3

go compile
solution library add nangate-45nm_beh -- -rtlsyntool DesignCompiler -vendor Nangate -technology 045nm

go libraries
directive set -CLOCKS {clk {-CLOCK_PERIOD 2.0}}

go assembly
directive set /Adder3/run/while -PIPELINE_INIT_INTERVAL 1
directive set /Adder3/run/while -PIPELINE_STALL_MODE flush

go architect
go extract
