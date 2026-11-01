# Directory.Build.targets (CopyVcpkgRuntimeBinaries): copies only the vcpkg DLLs that a built binary actually loads.
# Starting from -Binary, it follows each file's import table (dumpbin /dependents). A dependency found in -VcpkgBin is
# copied to -OutDir and followed. One already in -OutDir (Engine.dll, Client.dll, copied by the post-build steps) is
# only followed. System DLLs match neither and are skipped. DLLs loaded at run time only (LoadLibrary) are not found;
# no project relies on one from vcpkg (PhysX GPU is not used).
param(
    [Parameter(Mandatory)][string]$Binary,
    [Parameter(Mandatory)][string]$OutDir,
    [Parameter(Mandatory)][string]$VcpkgBin,
    [Parameter(Mandatory)][string]$Dumpbin
)
$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $Dumpbin)) { Write-Error "dumpbin not found: $Dumpbin"; exit 1 }
if (-not (Test-Path -LiteralPath $Binary)) { Write-Error "binary not found: $Binary"; exit 1 }

$visited = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
$queue = New-Object 'System.Collections.Generic.Queue[string]'
$queue.Enqueue((Resolve-Path -LiteralPath $Binary).Path)
$copied = @()

while ($queue.Count -gt 0) {
    $file = $queue.Dequeue()
    if (-not $visited.Add([IO.Path]::GetFileName($file))) { continue }

    $output = & $Dumpbin /nologo /dependents $file
    if ($LASTEXITCODE -ne 0) { Write-Error "dumpbin failed on $file"; exit 1 }
    foreach ($line in $output) {
        $name = $line.Trim()
        if ($name -notmatch '^[\w.+-]+\.dll$' -or $visited.Contains($name)) { continue }

        $source = Join-Path $VcpkgBin $name
        $target = Join-Path $OutDir $name
        if (Test-Path -LiteralPath $source) {
            $src = Get-Item -LiteralPath $source
            $dst = Get-Item -LiteralPath $target -ErrorAction SilentlyContinue
            if (-not $dst -or $dst.Length -ne $src.Length -or $dst.LastWriteTimeUtc -ne $src.LastWriteTimeUtc) {
                Copy-Item -LiteralPath $source -Destination $target -Force
                $copied += $name
            }
            $queue.Enqueue($source)
        }
        elseif (Test-Path -LiteralPath $target) {
            $queue.Enqueue($target)
        }
    }
}

if ($copied.Count -gt 0) { Write-Host "[vcpkg runtime] $([IO.Path]::GetFileName($Binary)): copied $($copied -join ', ')" }
exit 0
