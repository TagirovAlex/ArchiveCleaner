# --- НАСТРОЙКИ ---
$ConfigFilePath = "folders.txt"           # Файл с путями к папкам
$DebugMode = $true                        # true - только лог в файл; false - удаление
$LogFilePath = "debug_log.txt"            # Файл для лога отладки
# -----------------

if ($DebugMode) {
    if (Test-Path $LogFilePath) { Remove-Item $LogFilePath }
    New-Item -ItemType File -Path $LogFilePath -Force | Out-Null
    Write-Host "--- РЕЖИМ ОТЛАДКИ ВКЛЮЧЕН ---" -ForegroundColor Yellow
}

if (-not (Test-Path $ConfigFilePath)) {
    Write-Host "Ошибка: Файл $ConfigFilePath не найден!" -ForegroundColor Red
    exit
}

$Folders = Get-Content $ConfigFilePath | Where-Object { $_ -ne "" }

foreach ($FolderPath in $Folders) {
    if (Test-Path $FolderPath) {
        Write-Host "Обработка папки: $FolderPath" -ForegroundColor Cyan
        
        # Получаем все файлы в папке
        $Files = Get-ChildItem -Path $FolderPath -File

        if ($Files.Count -eq 0) {
            Write-Host "  Файлы не найдены." -ForegroundColor Yellow
            continue
        }

        # Группируем файлы по месяцам (ГГГГ-ММ)
        $GroupedFiles = $Files | Group-Object { $_.LastWriteTime.ToString("yyyy-MM") }

        foreach ($Group in $GroupedFiles) {
            # Сортируем все файлы месяца строго от самого старого к самому новому
            $MonthFiles = $Group.Group | Sort-Object LastWriteTime
            
            $KeepList = New-Object System.Collections.Generic.List[System.IO.FileInfo]

            if ($MonthFiles.Count -eq 1) {
                # Если в месяце всего один файл, оставляем его
                $KeepList.Add($MonthFiles[0])
            } else {
                # Оставляем самый первый файл месяца (начало периода)
                $KeepList.Add($MonthFiles[0])
                # Оставляем самый последний файл месяца (конец периода)
                $KeepList.Add($MonthFiles[-1])
            }

            # Обработка удаления/логирования
            foreach ($File in $MonthFiles) {
                if ($KeepList -notcontains $File) {
                    $DeleteMsg = "Удаление: $($File.FullName) (Дата: $($File.LastWriteTime))"
                    
                    if ($DebugMode) {
                        Write-Host "  [ОТЛАДКА] Будет удален: $($File.Name)" -ForegroundColor Gray
                        Add-Content -Path $LogFilePath -Value $DeleteMsg
                    } else {
                        try {
                            Write-Host "  Удаление: $($File.Name)" -ForegroundColor Gray
                            Remove-Item $File.FullName -Force
                        } catch {
                            Write-Host "  Ошибка при удалении $($File.Name): $_" -ForegroundColor Red
                        }
                    }
                } else {
                    Write-Host "  Оставляем: $($File.Name)" -ForegroundColor Green
                }
            }
        }
    } else {
        Write-Host "Путь не найден: $FolderPath" -ForegroundColor Red
    }
    Write-Host "-----------------------------------------------"
}

if ($DebugMode) {
    Write-Host "Отладка завершена. Проверьте файл $LogFilePath" -ForegroundColor Yellow
} else {
    Write-Host "Обработка завершена!" -ForegroundColor White -BackgroundColor DarkGreen
}
