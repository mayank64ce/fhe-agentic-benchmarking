import math
from random import random
from agents.react_agent import ReactAgent
from agents.tool import tool
import subprocess
import os

# define tools
@tool
def compile_code(code: str) -> bool:
    """
    Compiles the given C++ code.

    Args:
        code (str): The C++ code to compile.
    
    Returns:
        bool: True if compilation is successful, False otherwise.
    """
    # Create directory for cpp files if it doesn't exist
    cpp_dir = os.path.join(os.path.dirname(__file__), "cpp_files")
    os.makedirs(cpp_dir, exist_ok=True)

    # Define file paths
    cpp_file = os.path.join(cpp_dir, "program.cpp")
    exe_file = os.path.join(cpp_dir, "program")

    # Save code to file
    with open(cpp_file, "w") as f:
        f.write(code)

    try:
        # Compile the code
        result = subprocess.run(
            ["g++", cpp_file, "-o", exe_file],
            capture_output=True,
            text=True,
            check=False
        )
        
        # Check compilation result
        if result.returncode == 0:
            # print(f"Compilation successful. Executable saved at: {exe_file}")
            return True
        else:
            # print(f"Compilation failed: {result.stderr}")
            return False
    except Exception as e:
        # print(f"Error during compilation: {e}")
        pass
    return False

@tool
def execute_code(code: str) -> bool:
    """
    Executes the given code on unit tests and returns the output.

    Args:
        code (str): The code to execute.
    
    Returns:
        str: The output of the executed code.
    """
    # if random() > 0.5:
    #     return True
    # return False
    return True

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
agent = ReactAgent(tools=[compile_code, execute_code, rag_tool], model=model)

# user_prompt = "I want to calculate the sum of 1234 and 5678 and multiply the result by 5. Then compute the logarithm of the final result."
# user_prompt = "Write me a poem about a dog."

# user_prompt = "Write me a c++ code to add 2 numbers. And make sure it is compilable. Also make sure it is executable."
user_prompt = "Write me a C++ code to calculate the funbar of two numbers a and b. Make sure that the compilation and execution is successful. Do not give extra information."
output = agent.run(user_prompt, max_rounds=5)
print(output)