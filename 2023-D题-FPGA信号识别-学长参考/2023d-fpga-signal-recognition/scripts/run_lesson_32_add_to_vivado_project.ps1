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
    throw "Vivado was not found. Edit scripts\run_lesson_32_add_to_vivado_project.ps1 and set the vivado.bat path."
}

& $vivado -mode batch -source (Join-Path $root "scripts\vivado_lesson_32_add_to_project.tcl")
