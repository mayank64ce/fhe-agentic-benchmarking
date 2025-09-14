from argparse import ArgumentParser
from tools.formal_prompt import formalize_user_prompt
from prompts.task_and import task_prompt

if __name__ == "__main__":
    model = "deepseek/deepseek-chat-v3.1:free"
    parser = ArgumentParser()
    parser.add_argument(
        "--run_id",
        type=int,
        default=0,
        help="Run ID for the experiment",
    )
    args = parser.parse_args()

    formal_prompt = formalize_user_prompt(task_prompt, model, args.run_id)
    print(f"Formalized Prompt for run ID = {args.run_id}:\n", formal_prompt)

