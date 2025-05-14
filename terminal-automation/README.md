# Terminal Automation Project

This project is designed to automate the execution of multiple terminal commands in separate terminal windows. The primary purpose is to facilitate the running of a Gradle application, a Python script, and the compilation of an RPL border router.

## Prerequisites

Before running the script, ensure that you have the following installed on your system:

- **Gradle**: Make sure Gradle is installed and properly configured in your environment.
- **Python 3**: The script requires Python 3 to run the specified Python application.
- **Make**: Ensure that you have `make` installed to compile the RPL border router.

## Usage

To execute the commands, follow these steps:

1. Open a terminal window.
2. Navigate to the project directory:
   ```bash
   cd /path/to/terminal-automation
   ```
3. Run the command to execute the script:
   ```bash
   ./scripts/run_commands.sh
   ```

The script will open multiple terminal windows and execute the following commands in sequence:

1. Run the Gradle application with the following command:
   ```bash
   ./gradlew run --stacktrace --info --debug
   ```

2. Start the Python application with:
   ```bash
   python3 main.py
   ```

3. Compile the RPL border router with:
   ```bash
   make TARGET=cooja connect-router-cooja
   ```

## Notes

- Ensure that all paths in the script are correctly set to match your environment.
- You may need to adjust permissions to execute the script. Use the following command if necessary:
  ```bash
  chmod +x scripts/run_commands.sh
  ```