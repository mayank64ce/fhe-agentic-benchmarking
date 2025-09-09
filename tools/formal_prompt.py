import sys
import os
sys.path.append("..")  # Add the parent directory to the sys.path
from promptml.parser import PromptParser
from jinja2 import Template
from agents.utils.completions import completions_create, ChatHistory, build_prompt_structure
from agents.utils.extraction import extract_tag_content
from openai import OpenAI
from dotenv import load_dotenv
load_dotenv()

client = OpenAI(
    base_url="https://openrouter.ai/api/v1",
    api_key=os.getenv("OPENROUTER_API_KEY"),
)


def promptml_to_string(prompt: dict):
    """
    Converts a PromptML dictionary to a string representation.

    Args:
        prompt (dict): The PromptML dictionary.

    Returns:
        str: The string representation of the PromptML.
    """
    # post process dictionary to string with jinja template style
    template_str = """{{ parsed_prompt.context }}

Make sure to follow the below instructions:
{% for instr in parsed_prompt.instructions %}
    - {{ instr }}
{% endfor %}

{{ parsed_prompt.objective }}"""

    template = Template(template_str)
    rendered_prompt = template.render(parsed_prompt=prompt)
    return rendered_prompt
    

def convert_to_formal_prompt(intent: str, dafny_code: str, code_reqs: str) -> str:
    """
    Converts informal code requirements and Dafny code into a formal prompt.

    Args:
        intent (str): The user's intent.
        dafny_code (str): The Dafny code to be converted.
        code_reqs (str): The informal code requirements.

    Returns:
        str: The formal prompt.
    """
    prompt = f"""
@prompt
    @context
        You are an expert coding agent familiar with cryptographic concepts.
    @end

    @objective
        {intent}
    @end

    @instructions 
        @step
            Follow the following Dafny-like code as a pseudo-code.
            {dafny_code}
        @end
        @step
            Follow the following informal code requirements strictly.
            {code_reqs}
        @end
    @end
@end"""

    formal_prompt = PromptParser(prompt).parse()
    # post process dictionary to string with jinja template style
    formal_prompt = promptml_to_string(formal_prompt)
    return formal_prompt


def convert_intent_and_final_spec_to_dafny_and_code_reqs(intent: str, final_spec: str, model: str) -> tuple[str, str]:
    """
    Converts user intent and final specification into Dafny code and informal code requirements.

    Args:
        intent (str): The user's intent.
        final_spec (str): The final specification.
        model (str): The model to be used for the conversion.

    Returns:
        tuple[str, str]: A tuple containing the Dafny code and informal code requirements.
    """
    # 1. Get the intent and the final specification
    # 2. Pass them into an LLM.

    system_prompt = """You are an expert Dafny programmer and formal verification specialist. Your task is to receive an `intent` and `specifications` from the user and generate a Dafny-like code implementation along with its formal requirements.

You **MUST** format your response using the following tags. Do not include any introductory text, explanations, or conversational filler outside of the provided tags.

1.  Enclose the human-readable requirements within `<requirements>` and `</requirements>` XML tags. This description should be formatted with markdown and explicitly list preconditions, postconditions, and any other relevant specifications.
2.  Enclose the complete, syntactically correct Dafny-like code within a `<dafny_code>` and `</dafny_code>` XML block.

---

### Example

**User Input:**
`Intent`: A method to find the maximum value in an array of integers.
`Specifications`: The array must not be empty.

**Your Expected Output:**

<requirements>
### Method: Max

**Preconditions:**
* `requires a.Length > 0`: The input array `a` must not be empty.

**Postconditions:**
* `ensures exists i :: 0 <= i < a.Length && max == a[i]`: The returned value `max` must be an element that exists within the array `a`.
* `ensures forall j :: 0 <= j < a.Length ==> a[j] <= max`: Every element in the array `a` must be less than or equal to the returned value `max`.
</requirements>
<dafny_code>
method Max(a: array<int>) returns (max: int)
  requires a.Length > 0
  ensures exists i :: 0 <= i < a.Length && max == a[i]
  ensures forall j :: 0 <= j < a.Length ==> a[j] <= max
{
  max := a[0];
  var i := 1;
  while i < a.Length
    invariant 0 < i <= a.Length
    invariant exists k :: 0 <= k < i && max == a[k]
    invariant forall j :: 0 <= j < i ==> a[j] <= max
  {
    if a[i] > max {
      max := a[i];
    }
    i := i + 1;
  }
}
</dafny_code>""".strip() # Change this

    chat_history = ChatHistory(
        [
            build_prompt_structure(
                prompt = system_prompt,
                role="system",
            )
        ]
    )

    user_prompt = f"""
    User Intent: {intent}
    Final Specification: {final_spec}
    """.strip()

    chat_history.append(
        build_prompt_structure(
            prompt=user_prompt,
            role="user",
        )
    )

    # 3. The LLM will generate the Dafny code and informal code requirements.
    content = completions_create(client, chat_history, model)
    # 4. Parse the LLM output to get the Dafny code and informal code requirements.

    # print(content)

    dafny_code = extract_tag_content(str(content), "dafny_code")
    code_reqs = extract_tag_content(str(content), "requirements")

    return dafny_code, code_reqs


def complete_spec(partial_spec: str | dict) -> str | dict:
    """
    Completes a partial specification.

    Args:
        partial_spec (str | dict): The partial specification to be completed.

    Returns:
        str | dict: The completed specification.
    """

    return partial_spec # change this


def extract_intent_and_spec(user_prompt: str, model: str) -> tuple[str, str | dict]:
    """
    Extracts user intent and final specification from a user prompt.

    Args:
        user_prompt (str): The user's prompt.
        model (str): The model to be used for extraction.

    Returns:
        tuple[str, str | dict]: A tuple containing the user's intent and final specification.
    """
    # 1. Get the user prompt
    # 2. The LLM will generate the intent and final specification.
    system_prompt = """You are an expert AI assistant specializing in analyzing programming requests. Your only job is to deconstruct a user's prompt into a primary **`intent`** and a set of technical **`specifications`**.

- The **`intent`** is the core programming task or the function's main purpose (e.g., "Sort an array of integers," "Validate a user's email address").
- The **`specifications`** are all the technical constraints, requirements for edge cases, language choice, or specific implementation details that the code must follow.

You **MUST** format your response using the following XML-style tags. Do not include any text or explanations outside of these tags.

1.  The `intent` must be enclosed in `<intent>` and `</intent>` tags.
2.  All specifications, as a complete block of text, must be enclosed within `<specifications>` and `</specifications>` tags. Use bullet points for multiple specifications. If there are no specifications mentioned in the user prompt, the block **should** be empty.

---

### Examples

**User Prompt 1:**
"Write a javascript function to check if a number is prime. If the input isn't a positive integer, it should throw an error."

**Your Expected Output:**
```xml
<intent>Check if a number is prime</intent>
<specifications>
- The function must be written in JavaScript
- Should throw an error if the input is not a positive integer
</specifications>
```

**User Prompt 2:**
"I need a C++ class for a Min-Heap data structure.

**Your Expected Output:**
```xml
<intent>Create a class for a Min-Heap data structure</intent>
<specifications>
- The class must be written in C++
</specifications>
```

**User Prompt 3:**
"Reverse a string."

**Your Expected Output:**
```xml
<intent>Reverse a string</intent>
<specifications>
</specifications>
```""".strip()

    chat_history = ChatHistory(
        [
            build_prompt_structure(
            prompt=system_prompt,
            role="system",
        )
        ]
    )

    chat_history.append(
        build_prompt_structure(
            prompt=user_prompt,
            role="user",
        )
    )

    content = completions_create(client, chat_history, model)
    # print(content)
    # 3. Parse the LLM output to get the intent and final specification.
    intent = extract_tag_content(str(content), "intent")
    specifications = extract_tag_content(str(content), "specifications")

    return intent, specifications


def test_convert_to_formal_prompt():
    intent = "I want to calculate the funbar of two numbers a and b."
    dafny_code = """
method funbar(a: int, b: int) returns (result: int)
    ensures result == (a + b) + (a * b)
{
    result := (a + b) + (a * b);
}
"""
    code_reqs = """
    - The code should be written in C++.
    - The code should be compilable and executable.
    - The code should not contain any extra logging or print statements.
"""
    formal_prompt = convert_to_formal_prompt(intent, dafny_code, code_reqs)
    print(formal_prompt)


def test_convert_intent_and_final_spec_to_dafny_and_code_reqs():
    intent = "I want to calculate the funbar of two numbers a and b."
    final_spec = """
    The funbar of two numbers a and b is defined as: (a + b) + (a * b)
    """
    model = "deepseek/deepseek-chat-v3.1:free"
    dafny_code, code_reqs = convert_intent_and_final_spec_to_dafny_and_code_reqs(intent, final_spec, model)
    print("Dafny Code:")
    print(dafny_code.content[0])
    print()
    print("Code Requirements:")
    print(code_reqs.content[0])


def test_extract_intent_and_spec():
    # user_prompt = "Write a javascript function to check if a number is prime. If the input isn't a positive integer, it should throw an error."
    user_prompt = "Summarize this documentation for me."
    model = "deepseek/deepseek-chat-v3.1:free"
    intent, specifications = extract_intent_and_spec(user_prompt, model)
    print("Intent:")
    print(intent.content[0])
    print()
    print("Final Specification:")
    print(specifications.content[0])

# Bringing it all together

def formalize_user_prompt(user_prompt: str, model: str) -> str:
    """
    Formalizes a user prompt into a formal prompt using Dafny code and informal code requirements.
    Args:
        user_prompt (str): The user's prompt.
        model (str): The model to be used for the conversion.
    Returns:
        str: The formal prompt.
    """

    # 1. Extract intent and final specification from the user prompt
    intent, final_spec = extract_intent_and_spec(user_prompt, model)
    # 2. Complete the incomplete specification if needed
    final_spec = complete_spec(final_spec)
    # 3. Get dafny code and reqs from intent and final spec
    dafny_code, code_reqs = convert_intent_and_final_spec_to_dafny_and_code_reqs(intent.content[0], final_spec.content[0], model)
    # 4. Combine them into a formal prompt
    formal_prompt = convert_to_formal_prompt(intent.content[0], dafny_code.content[0], code_reqs.content[0])
    return formal_prompt


def test_formalize_user_prompt():
    user_prompt = "Write a javascript function to check if a number is prime. If the input isn't a positive integer, it should throw an error."
    model = "deepseek/deepseek-chat-v3.1:free"
    formal_prompt = formalize_user_prompt(user_prompt, model)
    print("User Prompt:",'\n', user_prompt)
    print()
    print("Formal Prompt:")
    print(formal_prompt)

if __name__ == "__main__":
    # test_convert_to_formal_prompt()
    # test_convert_intent_and_final_spec_to_dafny_and_code_reqs()
    # test_extract_intent_and_spec()
    test_formalize_user_prompt()
    pass