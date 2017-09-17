@echo off
echo "+++ Running %1"
FOR /L %%i IN (1,1,100) DO (
    %1 --use-colour no --order rand --rng-seed time > nul
	if %errorlevel% neq 0 exit /b %errorlevel%
)
echo "Tests OK"
