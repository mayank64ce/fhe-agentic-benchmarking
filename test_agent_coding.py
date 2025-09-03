import math
from random import random
from agents.react_agent import ReactAgent
from agents.tool import tool

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
    if random() > 0.5:
        return True
    return False

@tool
def execute_code(code: str) -> str:
    """
    Executes the given code on unit tests and returns the output.

    Args:
        code (str): The code to execute.
    
    Returns:
        str: The output of the executed code.
    """
    if random() > 0.5:
        return "Code execution failed."
    return "Code executed successfully."

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
user_prompt = "Write me a C++ code to calculate the funbar of two numbers a and b. Make sure the code is compilable and executable."

output = agent.run(user_prompt)
print(output)