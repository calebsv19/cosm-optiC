import os
import re
import csv

# Set the project directory path (modify this as needed)
project_dir = "./"  # Change to your project's root folder

# Regex to match function definitions (excluding main and built-in functions)
function_def_pattern = re.compile(r'^\s*[a-zA-Z_][a-zA-Z0-9_]*\s+\**([a-zA-Z_][a-zA-Z0-9_]*)\s*\(.*\)\s*\{')

# Regex to match function calls (not declarations)
function_call_pattern = re.compile(r'([a-zA-Z_][a-zA-Z0-9_]*)\s*\(')

# Dictionary to store function definitions and where they are found
function_definitions = {}
function_calls = {}

for root, _, files in os.walk(project_dir):
    for file in files:
        if file.endswith((".c", ".h")):  # Only scan C and header files
            filepath = os.path.join(root, file)
            with open(filepath, "r", errors="ignore") as f:
                lines = f.readlines()

                for line in lines:
                    # Check for function definitions
                    def_match = function_def_pattern.match(line)
                    if def_match:
                        function_name = def_match.group(1)
                        function_definitions[function_name] = file

                    # Check for function calls
                    call_matches = function_call_pattern.findall(line)
                    for call in call_matches:
                        if call not in function_calls:
                            function_calls[call] = set()
                        function_calls[call].add(file)

# Save to CSV file
csv_filename = "function_dependencies.csv"
with open(csv_filename, "w", newline="") as csvfile:
    writer = csv.writer(csvfile)
    writer.writerow(["Function Name", "Defined In", "Called In"])

    all_functions = set(function_definitions.keys()).union(set(function_calls.keys()))
    
    for func in sorted(all_functions):
        defined_in = function_definitions.get(func, "Unknown")
        called_in = ", ".join(function_calls.get(func, []))
        writer.writerow([func, defined_in, called_in])

print(f"Function dependency map saved to {csv_filename}")

