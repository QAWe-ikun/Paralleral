# MPI Matrix Multiplication - Benchmark Script
# Runs all process counts and matrix sizes combinations

$MPIEXEC = "D:\Microsoft MPI\Bin\mpiexec.exe"
$EXE = "D:\experiment\Parallel\task1\bin\mpi_matrix_mul.exe"

$proc_counts = @(2, 4, 8, 16)
$matrix_sizes = @(128, 256, 512, 1024, 2048)

Write-Host "`n============================================" -ForegroundColor Cyan
Write-Host "  MPI Matrix Multiplication - Performance Test" -ForegroundColor Cyan
Write-Host "============================================`n" -ForegroundColor Cyan

$results = @{}

foreach ($procs in $proc_counts) {
    $results[$procs] = @{}
    foreach ($size in $matrix_sizes) {
        Write-Host "Running: $procs procs, size=$size ..." -ForegroundColor Gray
        $raw = & $MPIEXEC -n $procs $EXE $size 2>&1
        $output = $raw | Out-String
        Write-Host $output

        $serial_time = 0.0
        $compute_time = 0.0

        foreach ($line in $raw) {
            if ($line -match 'Serial Time:\s*([\d.]+)') {
                $serial_time = [double]::Parse($Matches[1], [System.Globalization.CultureInfo]::InvariantCulture)
            }
            if ($line -match 'Compute Time:\s*([\d.]+)') {
                $compute_time = [double]::Parse($Matches[1], [System.Globalization.CultureInfo]::InvariantCulture)
            }
            if ($line -match 'Parallel Time:\s*([\d.]+)') {
                $compute_time = [double]::Parse($Matches[1], [System.Globalization.CultureInfo]::InvariantCulture)
            }
        }

        $results[$procs][$size] = @{
            Serial  = $serial_time
            Compute = $compute_time
        }

        Write-Host "  [Serial=$serial_time, Compute=$compute_time]" -ForegroundColor Yellow
        Write-Host ""
    }
}

# Print table
Write-Host "`n============================================" -ForegroundColor Yellow
Write-Host "  Performance Table - Time (seconds)" -ForegroundColor Yellow
Write-Host "============================================`n" -ForegroundColor Yellow

Write-Host "进程数`t128`t256`t512`t1024`t2048" -ForegroundColor Green
Write-Host "-----`t---`t---`t---`t----`t-----"

foreach ($procs in $proc_counts) {
    $line = "$procs`t"
    foreach ($size in $matrix_sizes) {
        $ct = $results[$procs][$size].Compute
        $line += ("{0:F3}`t" -f $ct)
    }
    Write-Host $line -ForegroundColor Green
}

# Speedup table
Write-Host "`n============================================" -ForegroundColor Yellow
Write-Host "  Speedup (Serial Time / Compute Time)" -ForegroundColor Yellow
Write-Host "============================================`n" -ForegroundColor Yellow

Write-Host "进程数`t128`t256`t512`t1024`t2048" -ForegroundColor Green
Write-Host "-----`t---`t---`t---`t----`t-----"

foreach ($procs in $proc_counts) {
    $line = "$procs`t"
    foreach ($size in $matrix_sizes) {
        $st = $results[$procs][$size].Serial
        $ct = $results[$procs][$size].Compute
        if ($ct -gt 0.00001) {
            $speedup = $st / $ct
            $line += ("{0:F2}x`t" -f $speedup)
        } else {
            $line += "N/A`t"
        }
    }
    Write-Host $line -ForegroundColor Green
}

Write-Host ""
