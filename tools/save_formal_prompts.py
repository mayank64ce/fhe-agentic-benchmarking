import json
from formal_prompt import formalize_user_prompt
import os
from argparse import ArgumentParser


def save_formal_prompt(task_name: str, informal_prompt: str, model: str, run_id: int, json_path: str):
    """
    Computes the formal prompt and saves it in a JSON file under the key:
    dictionary[task_name][formal_prompt_{run_id}] = formal_prompt
    
    Args:
        task_name (str): The name of the task.
        informal_prompt (str): The user's informal prompt.
        model (str): The model identifier to use for formalization.
        run_id (int): The specific run ID for this formalization.
        json_path (str): The path to the output JSON file.
    """
    # Step 1: Compute the formal prompt by calling the provided function.
    # The default seed value (0) will be used.
    formal_prompt = formalize_user_prompt(informal_prompt, model, run_id)
    
    # Step 2: Read existing data from the JSON file.
    # If the file doesn't exist or is empty, initialize an empty dictionary.
    data = {}
    if os.path.exists(json_path):
        try:
            with open(json_path, 'r') as f:
                data = json.load(f)
        except json.JSONDecodeError:
            # File is empty or malformed, proceed with an empty dictionary.
            pass

    # Step 3: Update the dictionary with the new formal prompt.
    # Ensure the sub-dictionary for the task_name exists.
    if task_name not in data:
        data[task_name] = {}
        
    # Create the key for this specific run and assign the formal prompt.
    key = model.split("/")[1].replace(":", "_").replace("-", "_")
    data[task_name][key] = formal_prompt
    
    # Step 4: Write the updated dictionary back to the JSON file.
    # 'indent=4' ensures the JSON is pretty-printed and human-readable.
    with open(json_path, 'w') as f:
        json.dump(data, f, indent=4)


if __name__ == "__main__":
    parser = ArgumentParser(description="Save formal prompts to a JSON file.")
    parser.add_argument("--task_name", type=str, required=True, help="The name of the task.")
    parser.add_argument("--informal_prompt", type=str, required=True, help="The user's informal prompt.")
    parser.add_argument("--model", type=str, required=True, help="The model identifier to use for formalization.")
    parser.add_argument("--run_id", type=int, required=True, help="The specific run ID for this formalization.")
    parser.add_argument("--json_path", type=str, required=True, help="The path to the output JSON file.")
    
    args = parser.parse_args()
    
    save_formal_prompt(args.task_name, args.informal_prompt, args.model, args.run_id, args.json_path)