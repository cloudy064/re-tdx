param(
    [Parameter(Mandatory = $true)][string]$Exe,
    [Parameter(Mandatory = $true)][string]$OutDir,
    [int]$Port = 18771
)

# Capture the two registry-derived HTTP documents by running `serve` on a scratch
# port. Port 8765 (the default) is deliberately left alone.
New-Item -ItemType Directory -Force $OutDir | Out-Null

$proc = Start-Process -FilePath $Exe -ArgumentList @('serve', '--port', "$Port") `
    -PassThru -WindowStyle Hidden `
    -RedirectStandardOutput "$OutDir\serve_stdout.txt" `
    -RedirectStandardError  "$OutDir\serve_stderr.txt"

try {
    $ready = $false
    foreach ($attempt in 1..40) {
        Start-Sleep -Milliseconds 250
        try {
            Invoke-WebRequest -Uri "http://127.0.0.1:$Port/api/v1/health" -TimeoutSec 3 `
                -UseBasicParsing | Out-Null
            $ready = $true
            break
        } catch { }
    }
    if (-not $ready) {
        Write-Output "SERVER DID NOT COME UP on port $Port"
        exit 1
    }

    foreach ($pair in @(
            @('features', "/api/v1/features"),
            @('openapi',  "/api/v1/openapi.json"))) {
        $name = $pair[0]
        $path = $pair[1]
        $resp = Invoke-WebRequest -Uri "http://127.0.0.1:$Port$path" -TimeoutSec 20 `
            -UseBasicParsing
        [IO.File]::WriteAllText("$OutDir\$name.json", $resp.Content)
        Write-Output ("{0,-9} {1,7} bytes" -f $name, $resp.Content.Length)
    }
} finally {
    if ($proc -and -not $proc.HasExited) {
        Stop-Process -Id $proc.Id -Force
        $proc.WaitForExit(5000) | Out-Null
    }
}
