import math
from agents.react_agent import ReactAgent
from agents.tool import tool

# define tools
@tool
def sum_two_elements(a: int, b: int) -> int:
    """
    Computes the sum of two integers.

    Args:
        a (int): The first integer.
        b (int): The second integer.
    
    Returns:
        int: The sum of the `a` and `b`.
    """
    return a + b

@tool
def multiply_two_elements(a: int, b: int) -> int:
    """
    Multiplies two integers.

    Args:
        a (int): The first integer.
        b (int): The second integer.
    
    Returns:
        int: The product of the `a` and `b`.
    """
    return a * b

@tool
def compute_log(x: int) -> float | str:
    """
    Computes the logarithm of an integer `x` with an optional base.

    Args:
        x (int): The integer to compute the logarithm for.
    
    Returns:
        float: The logarithm of `x` to the specified `base`.
    """
    if x <= 0:
        return "Logarithm is undefined for non-positive values."
    return math.log(x)

# define agent
model = "deepseek/deepseek-chat-v3.1:free"
agent = ReactAgent(tools=[sum_two_elements, multiply_two_elements, compute_log], model=model)

# user_prompt = "I want to calculate the sum of 1234 and 5678 and multiply the result by 5. Then compute the logarithm of the final result."
user_prompt = "Write me a poem about a dog."
output = agent.run(user_prompt)
print(output)