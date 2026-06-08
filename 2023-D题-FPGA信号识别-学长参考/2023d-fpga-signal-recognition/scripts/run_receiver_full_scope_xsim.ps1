$ErrorActionPreference = "Stop"

python .\scripts\prepare_receiver_full_scope_vectors.py

$vivado = "D:\Xilinx\Vivado\2023.1\bin\vivado.bat"
& $vivado -mode batch -source .\scripts\vivado_receiver_full_scope_pipeline.tcl
