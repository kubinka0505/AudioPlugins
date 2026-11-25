import os
import re
import json
from pathlib import Path

#-=-=-=-#

def remove_trailing_commas(string: str):
	return re.sub(r",\s*([\]}])", r"\1", string)

def clean(data):
	if isinstance(data, dict):
		cleaned = {}

		for k, v in data.items():
			cleaned_v = clean(v)

			if (
				cleaned_v is not None
				and cleaned_v != ""
				and cleaned_v != []
				and cleaned_v != {}
			):
				cleaned[k] = cleaned_v

		return cleaned
	elif isinstance(data, list):
		cleaned_list = []

		for item in data:
			cleaned_item = clean(item)

			if cleaned_item is not None and cleaned_item != "" and cleaned_item != []:
				cleaned_list.append(cleaned_item)

		return cleaned_list
	else:
		return data

def optimize_file(input_path: str, output_path: str):
	try:
		with open(input_path, "r", encoding = "UTF-8") as f:
			raw_content = f.read()

		# Preprocess to strip invalid trailing commas
		fixed_content = remove_trailing_commas(raw_content)
		data_input = json.loads(fixed_content)

		# Clean empty lists, strings, nulls, and empty dicts
		data_output = clean(data_input)

		# Save back with tab indentation
		with open(output_path, "w", encoding = "UTF-8") as f:
			json.dump(data_output, f, indent = "\t", ensure_ascii = False)
	except Exception as e:
		print(f"{input_path}: {e}")

def optimize_directory(root_dir, in_place: bool = True, output_dir: bool = None):
	root_path = Path(root_dir)

	for path in root_path.rglob("*.json"):
		input_path = str(path)

		if in_place:
			output_path = input_path
		else:
			rel_path = path.relative_to(root_path)
			target_path = Path(output_dir) / rel_path
			target_path.parent.mkdir(parents = True, exist_ok = True)
			output_path = str(target_path)

		optimize_file(input_path, output_path)

#-=-=-=-#

if __name__ == "__main__":
	if len(os.sys.argv) < 2:
		os.sys.exit(1)

	target_directory = os.sys.argv[1]
	if not os.path.isdir(target_directory):
		print(f"Error: {target_directory} is not a valid directory.")
		os.sys.exit(1)

	optimize_directory(target_directory, in_place = True)