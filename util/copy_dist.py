import os
import shutil

#-=-=-=-#

def get_product_name(root_dir: str = ".") -> str:
	cmake_path = os.path.join(root_dir, "CMakeLists.txt")

	if not os.path.isfile(cmake_path):
		raise FileNotFoundError(f"CMakeLists not found at: {cmake_path}")

	with open(cmake_path, "r", encoding = "UTF-8") as f:
		for line in f:
			stripped = line.strip()

			if stripped.upper().startswith("PRODUCT_NAME"):
				parts = stripped.split(None, 1)

				if len(parts) == 2:
					return parts[1].strip().strip('"')

	raise ValueError("PRODUCT_NAME not found in CMakeLists")

def copy_arch_folder(
	src_contents: str,
	dst_contents: str,

	src_arch: str,
	dst_arch: str,

	plugin_name: str
) -> None:
	src_arch_dir = os.path.join(src_contents, src_arch)
	dst_arch_dir = os.path.join(dst_contents, dst_arch)

	if not os.path.isdir(src_arch_dir):
		raise FileNotFoundError(f"Missing architecture folder: {src_arch_dir}")

	os.makedirs(dst_arch_dir, exist_ok = True)

	binary_name = f"{plugin_name}.vst3"
	src_binary = os.path.join(src_arch_dir, binary_name)
	dst_binary = os.path.join(dst_arch_dir, binary_name)

	if os.path.isdir(src_binary):
		shutil.copytree(src_binary, dst_binary, dirs_exist_ok = True)
	elif os.path.isfile(src_binary):
		shutil.copy2(src_binary, dst_binary)
	else:
		raise FileNotFoundError(f"Missing VST3 binary: {src_binary}")


def main(PROJECT_ROOT, DIST_DIR, CONFIG) -> None:
	plugin_name = get_product_name(PROJECT_ROOT)

	build32_contents = os.path.join(
		PROJECT_ROOT, "build32", f"{plugin_name}_artefacts", CONFIG, "VST3", f"{plugin_name}.vst3", "Contents"
	)

	build64_contents = os.path.join(
		PROJECT_ROOT, "build64", f"{plugin_name}_artefacts", CONFIG, "VST3", f"{plugin_name}.vst3", "Contents"
	)

	if os.path.exists(DIST_DIR):
		shutil.rmtree(DIST_DIR)

	dst_contents = os.path.join(DIST_DIR, plugin_name, "Contents")
	os.makedirs(dst_contents, exist_ok = True)

	# Copy shared Resources (e.g., moduleinfo.json)
	src_resources = os.path.join(build64_contents, "Resources")
	if not os.path.isdir(src_resources):
		src_resources = os.path.join(build32_contents, "Resources")
	if os.path.isdir(src_resources):
		shutil.copytree(src_resources, os.path.join(dst_contents, "Resources"), dirs_exist_ok = True)

	# Copy x64 and x86 binaries into Contents
	copy_arch_folder(build64_contents, dst_contents, "x86_64-win", "x64-win", plugin_name)
	copy_arch_folder(build32_contents, dst_contents, "x86-win", "x86-win", plugin_name)

if __name__ == "__main__":
	PROJECT_ROOT = os.getcwd()
	DIST_DIR = os.path.join(PROJECT_ROOT, "dist")
	CONFIG = "Release"

	main(PROJECT_ROOT, DIST_DIR, CONFIG)