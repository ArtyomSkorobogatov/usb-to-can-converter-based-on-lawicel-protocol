import os
import json
import subprocess

def run_cmd(cmd, cwd=None):
    return subprocess.run(cmd, capture_output=True, text=True, cwd=cwd)

if __name__ == '__main__':
    repo_root = os.getcwd()

    try:
        with open('requirements.json', 'r') as f:
            requirements = json.load(f)
    except Exception as e:
        print("Error reading [requirements.json]:", e)
        exit(1)

    print("\nAdding submodules...")
    for url, v in requirements.items():
        submodule_path = os.path.join(repo_root, v["path"])

        if not os.path.isdir(submodule_path):
            result = run_cmd(["git", "submodule", "add", url, v["path"]])
            if result.returncode == 0:
                print(f"Submodule [{url}] added successfully")
            else:
                print(f"Error adding submodule [{url}]:", result.stderr)
        else:
            print(f"Submodule path [{v['path']}] already exists, skipping 'add'")

    print("\nUpdating submodules...")
    result = run_cmd(["git", "submodule", "update", "--init", "--recursive"])
    if result.returncode == 0:
        print("Submodules updated successfully")
    else:
        print("Error updating submodules:", result.stderr)

    print("\nChecking out tags...")
    for url, v in requirements.items():
        submodule_path = os.path.join(repo_root, v["path"])

        result = run_cmd(["git", "checkout", "master"], cwd=submodule_path)
        if result.returncode != 0:
            result = run_cmd(["git", "checkout", "main"], cwd=submodule_path)
            if result.returncode != 0:
                print(f"Error checking out 'master' or 'main' in [{v['path']}]:", result.stderr)
                continue

        result = run_cmd(["git", "fetch", "--all", "--tags"], cwd=submodule_path)
        if result.returncode != 0:
            print(f"Error fetching data in [{v['path']}]:", result.stderr)
            continue

        result = run_cmd(["git", "checkout", v["tag"]], cwd=submodule_path)
        if result.returncode == 0:
            print(f"Submodule <{url}> checked out on tag [{v['tag']}] successfully")
        else:
            print(f"Submodule <{url}> error checking out tag [{v['tag']}]:", result.stderr)
