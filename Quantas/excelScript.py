import json
import csv
import re
from collections import defaultdict

# Define file paths
txt_filename = "Delay10.txt"  # Update with your actual .txt file name
csv_filename = "processed_experiment_results.csv"

# Step 1: Read the JSON data from the .txt file
with open(txt_filename, "r") as file:
    data = json.load(file)  # Parses the text file as JSON

# Step 2: Extract relevant fields and dynamically track block flip counts
extracted_data = []
max_blocks_flipped = 0
flip_counts_per_test = []

for i, test in enumerate(data["tests"]):  
    blockchain_length = test["Block Chain Length"][0]
    frequency_str = test["Frequency"][0]  # Example: "1 block flipped: 17, 2 blocks flipped: 7, 4 blocks flipped: 3"
    
    # Use regex to extract (number of blocks, count)
    flip_counts = defaultdict(int)  
    for match in re.findall(r"(\d+) blocks? flipped: (\d+)", frequency_str):
        num_blocks, count = map(int, match)
        flip_counts[num_blocks] = count
        max_blocks_flipped = max(max_blocks_flipped, num_blocks)  # Track highest block flip count
    
    total_flipped = test["Network Total Flipped Blocks"][0]
    total_switches = test["Network Total Switches"][0]

    flip_counts_per_test.append(flip_counts)  # Store flip count dictionaries
    extracted_data.append([i + 1, blockchain_length, total_flipped, total_switches])

# Step 3: Generate column headers dynamically
headers = ["Run #", "Blockchain Length"]
headers += [f"{n} Blocks Flipped" for n in range(1, max_blocks_flipped + 1)]
headers += ["Total Flipped Blocks", "Total Switches"]

# Step 4: Write to CSV
with open(csv_filename, mode="w", newline="") as file:
    writer = csv.writer(file)
    
    # Write the header row
    writer.writerow(headers)
    
    # Write extracted data rows
    for i, row in enumerate(extracted_data):
        flip_data = flip_counts_per_test[i]
        flip_values = [flip_data.get(n, 0) for n in range(1, max_blocks_flipped + 1)]
        writer.writerow(row[:2] + flip_values + row[2:])

print(f"Processed data saved to {csv_filename}")
