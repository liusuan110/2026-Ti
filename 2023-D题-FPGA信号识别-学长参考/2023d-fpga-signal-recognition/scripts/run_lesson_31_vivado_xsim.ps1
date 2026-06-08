$ErrorActionPreference = "Stop"

$root = Resolve-Path (Join-Path $PSScriptRoot "..")
$vivadoCandidates = @(
    "D:\Xilinx\Vivado\2023.1\bin\vivado.bat",
    "C:\Xilinx\Vivado\2023.1\bin\vivado.bat"
)

$vivado = $null
foreach ($candidate in $vivadoCandidates) {
    if (Test-Path -LiteralPath $candidate) {
        $vivado = $candidate
        break
    }
}

if (-not $vivado) {
    $cmd = Get-Command vivado -ErrorAction SilentlyContinue
    if ($cmd) {
        $vivado = $cmd.Source
    }
}

if (-not $vivado) {
    throw "Vivado was not found. Edit scripts\run_lesson_31_vivado_xsim.ps1 and set the vivado.bat path."
}

python (Join-Path $root "scripts\lesson_31_prepare_and_check_pipeline_phase_core.py")
& $vivado -mode batch -source (Join-Path $root "scripts\vivado_lesson_31_pipeline_phase_core.tcl")
if (-not (Test-Path (Join-Path $root "outputs\lesson_31\ddc_phase_core_pipeline_sim.csv"))) {
    throw "Vivado/XSim did not create outputs\lesson_31\ddc_phase_core_pipeline_sim.csv"
}
python (Join-Path $root "scripts\lesson_31_prepare_and_check_pipeline_phase_core.py")
