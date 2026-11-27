@echo off
cd /d "%~dp0"
if not exist ".venv" (
  echo [*] Creating Python virtual environment...
  python -m venv .venv
  .venv\Scripts\pip install --upgrade pip
  .venv\Scripts\pip install -r requirements.txt
)
.venv\Scripts\python trainer.py %*
pause
