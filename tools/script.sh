#!/bin/bash

task_name="task_not"
informal_prompt="Write a TFHE code in C that performs a bitwise NOT operation on one input integer. Make sure that the code compiles and executes successfully."
models=("qwen/qwen3-coder" "openai/gpt-5" "google/gemini-2.5-pro" "deepseek/deepseek-chat-v3.1:free")

for model in "${models[@]}"; do
    echo "Processing model: $model for task: $task_name"
    python save_formal_prompts.py \
        --task_name "$task_name" \
        --informal_prompt "$informal_prompt" \
        --model "$model" \
        --run_id 42 \
        --json_path="temp.json"
    echo "Saved formal prompt for model: $model"
done