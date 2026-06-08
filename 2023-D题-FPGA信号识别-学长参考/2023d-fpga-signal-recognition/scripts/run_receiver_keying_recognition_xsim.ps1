$ErrorActionPreference = "Stop"

python .\scripts\prepare_receiver_keying_recognition_vectors.py

$vivado = "D:\Xilinx\Vivado\2023.1\bin\vivado.bat"
& $vivado -mode batch -source .\scripts\vivado_receiver_keying_recognition_pipeline.tcl
