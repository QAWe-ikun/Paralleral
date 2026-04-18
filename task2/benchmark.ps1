# MPI Collective Matrix Multiplication - Benchmark Script
# Runs all process counts and matrix sizes combinations for different algorithms

$MPIEXEC = "D:\Microsoft MPI\Bin\mpiexec.exe"
$ROW_EXE = "D:\experiment\Parallel\task2\bin\mpi_collective_mat_mul.exe"
$COL_EXE = "D:\experiment\Parallel\task2\bin\mpi_col_distrib_mat_mul.exe"
$BLOCK_EXE = "D:\experiment\Parallel\task2\bin\mpi_2d_block_mat_mul.exe"
$SERIAL_EXE = "D:\experiment\Parallel\task2\bin\serial_mat_mul.exe"

$proc_counts = @(1, 2, 4, 8, 16)
$matrix_sizes = @(128, 256, 512, 1024, 2048)

Write-Host "`n============================================" -ForegroundColor Cyan
Write-Host "  MPI Collective Matrix Multiplication - Performance Test (Row-wise Distribution)" -ForegroundColor Cyan
Write-Host "============================================`n" -ForegroundColor Cyan

$row_results = @{}

foreach ($procs in $proc_counts) {
    $row_results[$procs] = @{}
    foreach ($size in $matrix_sizes) {
        Write-Host "Running (Row-wise): $procs procs, size=$size ..." -ForegroundColor Gray

        if ($procs -eq 1) {
            # 单进程直接运行串行版本，不需要 mpiexec
            $raw = & $SERIAL_EXE $size 2>&1
        } else {
            $raw = & $MPIEXEC -n $procs $ROW_EXE $size 2>&1
        }
        $output = $raw | Out-String
        Write-Host $output

        $serial_time = 0.0
        $compute_time = 0.0
        $total_time = 0.0

        foreach ($line in $raw) {
            if ($line -match 'Serial Time:\s*([\d.]+)') {
                $serial_time = [double]::Parse($Matches[1], [System.Globalization.CultureInfo]::InvariantCulture)
            }
            if ($line -match 'Compute Time:\s*([\d.]+)') {
                $compute_time = [double]::Parse($Matches[1], [System.Globalization.CultureInfo]::InvariantCulture)
            }
            if ($line -match 'Total Time:\s*([\d.]+)') {
                $total_time = [double]::Parse($Matches[1], [System.Globalization.CultureInfo]::InvariantCulture)
            }
        }

        # 对于集合通信版本，我们使用 Total Time 来计算加速比
        # 对于进程数 1（串行版本），我们使用 Serial Time 作为有效时间
        if ($procs -eq 1) {
            $effective_time = $serial_time
        } else {
            $effective_time = $total_time
        }

        $row_results[$procs][$size] = @{
            Serial = $serial_time
            Total = $total_time
            Effective = $effective_time
        }

        Write-Host "  [Serial=$serial_time, Total=$total_time, Effective=$effective_time]" -ForegroundColor Yellow
        Write-Host ""
    }
}

Write-Host "`n============================================" -ForegroundColor Cyan
Write-Host "  MPI Collective Matrix Multiplication - Performance Test (Column-wise Distribution)" -ForegroundColor Cyan
Write-Host "============================================`n" -ForegroundColor Cyan

$col_results = @{}

foreach ($procs in $proc_counts) {
    $col_results[$procs] = @{}
    foreach ($size in $matrix_sizes) {
        if ($procs -eq 1) {
            # 1 个进程时复用行划分的数据（因为都是串行执行）
            $col_results[$procs][$size] = $row_results[$procs][$size]
            Write-Host "Running (Column-wise): $procs procs, size=$size ... (reusing row-wise data)" -ForegroundColor Gray
            continue
        }
        
        Write-Host "Running (Column-wise): $procs procs, size=$size ..." -ForegroundColor Gray

        $raw = & $MPIEXEC -n $procs $COL_EXE $size 2>&1
        $output = $raw | Out-String
        Write-Host $output

        $serial_time = 0.0
        $compute_time = 0.0
        $total_time = 0.0

        foreach ($line in $raw) {
            if ($line -match 'Serial Time:\s*([\d.]+)') {
                $serial_time = [double]::Parse($Matches[1], [System.Globalization.CultureInfo]::InvariantCulture)
            }
            if ($line -match 'Compute Time:\s*([\d.]+)') {
                $compute_time = [double]::Parse($Matches[1], [System.Globalization.CultureInfo]::InvariantCulture)
            }
            if ($line -match 'Total Time:\s*([\d.]+)') {
                $total_time = [double]::Parse($Matches[1], [System.Globalization.CultureInfo]::InvariantCulture)
            }
        }

        $effective_time = $total_time

        $col_results[$procs][$size] = @{
            Serial = $serial_time
            Total = $total_time
            Effective = $effective_time
        }

        Write-Host "  [Serial=$serial_time, Total=$total_time, Effective=$effective_time]" -ForegroundColor Yellow
        Write-Host ""
    }
}

# 添加2D Block测试
Write-Host "`n============================================" -ForegroundColor Cyan
Write-Host "  MPI Collective Matrix Multiplication - Performance Test (2D Block Distribution)" -ForegroundColor Cyan
Write-Host "============================================`n" -ForegroundColor Cyan

$block_results = @{}

foreach ($procs in $proc_counts) {
    $block_results[$procs] = @{}
    foreach ($size in $matrix_sizes) {
        if ($procs -eq 1) {
            # 1 个进程时复用行划分的数据（因为都是串行执行）
            $block_results[$procs][$size] = $row_results[$procs][$size]
            Write-Host "Running (2D Block): $procs procs, size=$size ... (reusing row-wise data)" -ForegroundColor Gray
            continue
        }
        
        Write-Host "Running (2D Block): $procs procs, size=$size ..." -ForegroundColor Gray

        $raw = & $MPIEXEC -n $procs $BLOCK_EXE $size 2>&1
        $output = $raw | Out-String
        Write-Host $output

        $serial_time = 0.0
        $compute_time = 0.0
        $total_time = 0.0

        foreach ($line in $raw) {
            if ($line -match 'Serial Time:\s*([\d.]+)') {
                $serial_time = [double]::Parse($Matches[1], [System.Globalization.CultureInfo]::InvariantCulture)
            }
            if ($line -match 'Compute Time:\s*([\d.]+)') {
                $compute_time = [double]::Parse($Matches[1], [System.Globalization.CultureInfo]::InvariantCulture)
            }
            if ($line -match 'Total Time:\s*([\d.]+)') {
                $total_time = [double]::Parse($Matches[1], [System.Globalization.CultureInfo]::InvariantCulture)
            }
        }

        $effective_time = $total_time

        $block_results[$procs][$size] = @{
            Serial = $serial_time
            Total = $total_time
            Effective = $effective_time
        }

        Write-Host "  [Serial=$serial_time, Total=$total_time, Effective=$effective_time]" -ForegroundColor Yellow
        Write-Host ""
    }
}

# Print Row-wise distribution table
Write-Host "`n============================================" -ForegroundColor Yellow
Write-Host "  Performance Table - Row-wise Distribution (Time in seconds)" -ForegroundColor Yellow
Write-Host "============================================`n" -ForegroundColor Yellow

Write-Host "进程数`t128`t256`t512`t1024`t2048" -ForegroundColor Green
Write-Host "-----`t---`t---`t---`t----`t-----"

foreach ($procs in $proc_counts) {
    $line = "$procs`t"
    foreach ($size in $matrix_sizes) {
        $et = $row_results[$procs][$size].Effective
        $line += ("{0:F3}`t" -f $et)
    }
    Write-Host $line -ForegroundColor Green
}

# Print Column-wise distribution table
Write-Host "`n============================================" -ForegroundColor Yellow
Write-Host "  Performance Table - Column-wise Distribution (Time in seconds)" -ForegroundColor Yellow
Write-Host "============================================`n" -ForegroundColor Yellow

Write-Host "进程数`t128`t256`t512`t1024`t2048" -ForegroundColor Green
Write-Host "-----`t---`t---`t---`t----`t-----"

foreach ($procs in $proc_counts) {
    $line = "$procs`t"
    foreach ($size in $matrix_sizes) {
        $et = $col_results[$procs][$size].Effective
        $line += ("{0:F3}`t" -f $et)
    }
    Write-Host $line -ForegroundColor Green
}

# Print 2D Block distribution table
Write-Host "`n============================================" -ForegroundColor Yellow
Write-Host "  Performance Table - 2D Block Distribution (Time in seconds)" -ForegroundColor Yellow
Write-Host "============================================`n" -ForegroundColor Yellow

Write-Host "进程数`t128`t256`t512`t1024`t2048" -ForegroundColor Green
Write-Host "-----`t---`t---`t---`t----`t-----"

foreach ($procs in $proc_counts) {

    $line = "$procs`t"
    foreach ($size in $matrix_sizes) {
        $et = $block_results[$procs][$size].Effective
        $line += ("{0:F3}`t" -f $et)
    }
    Write-Host $line -ForegroundColor Green

}

# Speedup table for Row-wise distribution
Write-Host "`n============================================" -ForegroundColor Yellow
Write-Host "  Speedup (Row-wise Distribution) - Serial Time / Effective Time" -ForegroundColor Yellow
Write-Host "============================================`n" -ForegroundColor Yellow

Write-Host "进程数`t128`t256`t512`t1024`t2048" -ForegroundColor Green
Write-Host "-----`t---`t---`t---`t----`t-----"

foreach ($procs in $proc_counts) {
    $line = "$procs`t"
    foreach ($size in $matrix_sizes) {
        $st = $row_results[1][$size].Serial  # 从进程数 1 获取串行时间
        $et = $row_results[$procs][$size].Effective
        if ($et -gt 0) {
            $speedup = $st / $et
            $line += ("{0:F2}x`t" -f $speedup)
        } else {
            $line += "1.00x`t"
        }
    }
    Write-Host $line -ForegroundColor Green
}

# Speedup table for Column-wise distribution (relative to serial)
Write-Host "`n============================================" -ForegroundColor Yellow
Write-Host "  Speedup (Column-wise Distribution) - Serial Time / Effective Time" -ForegroundColor Yellow
Write-Host "============================================`n" -ForegroundColor Yellow

Write-Host "进程数`t128`t256`t512`t1024`t2048" -ForegroundColor Green
Write-Host "-----`t---`t---`t---`t----`t-----"

foreach ($procs in $proc_counts) {
    $line = "$procs`t"
    foreach ($size in $matrix_sizes) {
        $st = $row_results[1][$size].Serial  # Use serial time from row-wise as baseline
        $et = $col_results[$procs][$size].Effective
        if ($et -gt 0) {
            $speedup = $st / $et
            $line += ("{0:F2}x`t" -f $speedup)
        } else {
            $line += "1.00x`t"
        }
    }
    Write-Host $line -ForegroundColor Green
}

# Speedup table for 2D Block distribution (relative to serial)
Write-Host "`n============================================" -ForegroundColor Yellow
Write-Host "  Speedup (2D Block Distribution) - Serial Time / Effective Time" -ForegroundColor Yellow
Write-Host "============================================`n" -ForegroundColor Yellow

Write-Host "进程数`t128`t256`t512`t1024`t2048" -ForegroundColor Green
Write-Host "-----`t---`t---`t---`t----`t-----"

foreach ($procs in $proc_counts) {

    $line = "$procs`t"
    foreach ($size in $matrix_sizes) {
        $st = $row_results[1][$size].Serial  # Use serial time from row-wise as baseline
        $et = $block_results[$procs][$size].Effective
        if ($et -gt 0) {
            $speedup = $st / $et
            $line += ("{0:F2}x`t" -f $speedup)
        } else {
            $line += "1.00x`t"
        }
    }
    Write-Host $line -ForegroundColor Green
}

# Comparison between Row-wise and Column-wise distributions
Write-Host "`n============================================" -ForegroundColor Yellow
Write-Host "  Comparison: Row-wise vs Column-wise Distribution" -ForegroundColor Yellow
Write-Host "  (Ratio: Row-wise Time / Column-wise Time)" -ForegroundColor Yellow
Write-Host "============================================`n" -ForegroundColor Yellow

Write-Host "进程数`t128`t256`t512`t1024`t2048" -ForegroundColor Green
Write-Host "-----`t---`t---`t---`t----`t-----"

foreach ($procs in $proc_counts) {
    $line = "$procs`t"
    foreach ($size in $matrix_sizes) {
        $row_time = $row_results[$procs][$size].Effective
        $col_time = $col_results[$procs][$size].Effective
        if ($col_time -gt 0) {
            $ratio = $row_time / $col_time
            $line += ("{0:F2}`t" -f $ratio)
        } else {
            $line += "1.00`t"
        }
    }
    Write-Host $line -ForegroundColor Green

}

# Comparison between Row-wise and 2D Block distributions
Write-Host "`n============================================" -ForegroundColor Yellow
Write-Host "  Comparison: Row-wise vs 2D Block Distribution" -ForegroundColor Yellow
Write-Host "  (Ratio: Row-wise Time / 2D Block Time)" -ForegroundColor Yellow
Write-Host "============================================`n" -ForegroundColor Yellow

Write-Host "进程数`t128`t256`t512`t1024`t2048" -ForegroundColor Green
Write-Host "-----`t---`t---`t---`t----`t-----"

foreach ($procs in $proc_counts) {

    $line = "$procs`t"
    foreach ($size in $matrix_sizes) {
        $row_time = $row_results[$procs][$size].Effective
        $block_time = $block_results[$procs][$size].Effective
        if ($block_time -gt 0) {
            $ratio = $row_time / $block_time
            $line += ("{0:F2}`t" -f $ratio)
        } else {
            $line += "1.00`t"
        }
    }
    Write-Host $line -ForegroundColor Green

}

Write-Host ""