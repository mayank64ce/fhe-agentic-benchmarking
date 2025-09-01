import os
from openai import OpenAI
from dotenv import load_dotenv
from agents.utils.completions import ChatHistory, build_prompt_structure, completions_create
load_dotenv()

client = OpenAI(
    base_url="https://openrouter.ai/api/v1",
    api_key = os.getenv("OPENROUTER_API_KEY")
)

model = "deepseek/deepseek-chat-v3.1:free"

messages = ChatHistory(
    [
        build_prompt_structure(
            prompt="You are a helpful assistant.",
            role="system",
        ),
        build_prompt_structure(
            prompt="Write me a poem about a dog.",
            role="user",
        ),
    ]
)


content = completions_create(client, messages, model)

print(content)