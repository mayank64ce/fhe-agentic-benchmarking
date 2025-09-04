import math
from random import random
from agents.react_agent import ReactAgent
from agents.tool import tool
import subprocess
import os
from tools.compiler import Compiler
from tools.executor import Executor

# define tools
@tool 
def rag_tool(query: str) -> str:
    """
    Performs a RAG (Retrieval-Augmented Generation) search based on the provided query.

    Args:
        query (str): The search query.
    
    Returns:
        str: The search results.
    """
    return "A funbar of two numbers a and b is defined as: (a + b) + (a * b)"

# define agent
model = "deepseek/deepseek-chat-v3.1:free"

# initialize save directory here
save_dir = os.path.join("logs", model.split("/")[1].replace(":", "_").replace("-", "_"))
os.makedirs(save_dir, exist_ok=True)

test_dir = "unit_tests/task_add"

compiler = Compiler(save_dir=save_dir)
executor = Executor(save_dir=save_dir, test_dir=test_dir)

@tool
def compile_code(code: str) -> str:
    """
    Compiles the given C++ code.

    Args:
        code (str): The C++ code to compile.
    
    Returns:
        bool: Success message if compilation is successful, error message otherwise.
    """
    return compiler.compile(code)

@tool
def execute_code() -> str:
    """
    Executes the given code on unit tests and returns the output.

    Args:
        code (str): The code to execute.
    
    Returns:
        str: Success message if execution is successful, error message with failed cases otherwise.
    """
    # if random() > 0.5:
    #     return True
    # return False
    return executor.execute()


agent = ReactAgent(tools=[compile_code, execute_code, rag_tool], model=model)

# user_prompt = "I want to calculate the sum of 1234 and 5678 and multiply the result by 5. Then compute the logarithm of the final result."
# user_prompt = "Write me a poem about a dog."

# user_prompt = "Write me a c++ code to add 2 numbers. And make sure it is compilable. Also make sure it is executable."
user_prompt = "Write me a C++ code to calculate sum of two input vectors of length 5. Do not use extra logging in the program, just the computation. Make sure that the compilation and execution is successful. Do not give extra information."
output = agent.run(user_prompt, max_rounds=5)
print(output)