param(
    [Parameter(Mandatory = $true)]
    [string]$ExePath,

    [int]$Samples = 5
)

$ErrorActionPreference = "Stop"
$exe = (Resolve-Path $ExePath).Path
$results = @()

for ($i = 1; $i -le $Samples; $i++) {
    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    $process = Start-Process -FilePath $exe -PassThru
    try {
        try { $null = $process.WaitForInputIdle(5000) } catch { }
        $stopwatch.Stop()
        Start-Sleep -Milliseconds 300
        $process.Refresh()
        $results += [pscustomobject]@{
            Sample = $i
            StartupMs = [math]::Round($stopwatch.Elapsed.TotalMilliseconds, 2)
            WorkingSetMB = [math]::Round($process.WorkingSet64 / 1MB, 2)
            PrivateMemoryMB = [math]::Round($process.PrivateMemorySize64 / 1MB, 2)
        }
    }
    finally {
        if (!$process.HasExited) {
            Stop-Process -Id $process.Id -Force
            $process.WaitForExit()
        }
    }
}

$results | Format-Table -AutoSize
[pscustomobject]@{
    Samples = $Samples
    StartupMsAverage = [math]::Round(($results | Measure-Object StartupMs -Average).Average, 2)
    WorkingSetMBAverage = [math]::Round(($results | Measure-Object WorkingSetMB -Average).Average, 2)
    PrivateMemoryMBAverage = [math]::Round(($results | Measure-Object PrivateMemoryMB -Average).Average, 2)
    ExeBytes = (Get-Item $exe).Length
} | Format-List
