set sfd [file dir [info script]]

## Make sure the input timing annotation file is found 
set input_dir_path .
set base_name my_testbench
if { "[file normalize $sfd]" ne "[file normalize [pwd]]" } {
  if { ![file exists $input_dir_path/$base_name.input.json] } {
    set input_dir_path "$sfd"
  }
}

options defaults

options set /Input/CppStandard c++11
options set /Input/CompilerFlags {-DHLS_CATAPULT -DCONNECTIONS_ACCURATE_SIM -DCONNECTIONS_NAMING_ORIGINAL}

project new

flow package require /SCVerify
solution options set Flows/SCVerify/INVOKE_ARGS [list $base_name $input_dir_path]
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
