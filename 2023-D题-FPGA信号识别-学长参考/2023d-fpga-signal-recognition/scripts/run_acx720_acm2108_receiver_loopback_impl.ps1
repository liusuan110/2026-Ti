$ErrorActionPreference = "Stop"

$vivado = "D:\Xilinx\Vivado\2023.1\bin\vivado.bat"
& $vivado -mode batch -nolog -nojournal -notrace -source .\scripts\vivado_acx720_acm2108_receiver_loopback_impl.tcl
