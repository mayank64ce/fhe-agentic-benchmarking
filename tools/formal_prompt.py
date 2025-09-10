import sys
import os
import json
sys.path.append("..")  # Add the parent directory to the sys.path
from promptml.parser import PromptParser
from jinja2 import Template
from agents.utils.completions import completions_create, ChatHistory, build_prompt_structure
from agents.utils.extraction import extract_tag_content
from prompts import system_prompt_intent_extraction, system_prompt_dafny_conversion
from le import get_lambda
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

TASK: {{ parsed_prompt.objective }}"""

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


def convert_intent_and_final_spec_to_dafny_and_code_reqs(intent: str, final_spec: str | dict, model: str) -> tuple[str, str]:
    """
    Converts user intent and final specification into Dafny code and informal code requirements.

    Args:
        intent (str): The user's intent.
        final_spec (str | dict): The final specification.
        model (str): The model to be used for the conversion.

    Returns:
        tuple[str, str]: A tuple containing the Dafny code and informal code requirements.
    """
    # 1. Get the intent and the final specification
    # 2. Pass them into an LLM.

    system_prompt = system_prompt_dafny_conversion

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

    # breakpoint()
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
        str | dict: The completed specification. This will only contain lambda.
    """

    # 1. if partial_spec has lambda, just keep lambda and return
    if partial_spec.get("minimum_lambda") is not None:
        return {"minimum_lambda": partial_spec["minimum_lambda"]}
    # 2. Otherwise, fill in the missing parameters using defaults from lattice estimator
    n = partial_spec.get("n", 630)
    xs_sigma = partial_spec.get("xs_sigma", 0.5)
    xs_mu = partial_spec.get("xs_mu", 0.5)
    xe_sigma = partial_spec.get("xe_sigma", 131072.0)
    xe_mu = partial_spec.get("xe_mu", 0.0)
    lambda_val = get_lambda(n=n, xs_sigma=xs_sigma, xs_mu=xs_mu, xe_sigma=xe_sigma, xe_mu=xe_mu)
    # 3. run the lattice estimator to get the new lambda
    return {'minimum_lambda': lambda_val} # change this


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
    system_prompt = system_prompt_intent_extraction

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

    # make specifications a dictionary
        # 4. Convert the specification string to a dictionary
    spec_dict = {}
    if specifications.found:
        try:
            spec_dict = json.loads(specifications.content[0])
        except json.JSONDecodeError:
            print("Warning: LLM output for specifications was not valid JSON.")
            pass
    # breakpoint()
    return intent, spec_dict


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
    intent = "Write C code to bitwise AND 2 integers."
    final_spec = {
        'minimum_lambda': 128,
    }
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
    user_prompt = "Write me a C code to bitwise XOR 2 integers. I want the 256-bit security and secret distibution should have 0 mean and unit variance."
    model = "deepseek/deepseek-chat-v3.1:free"
    intent, specifications = extract_intent_and_spec(user_prompt, model)
    print("Intent:")
    print(intent.content[0])
    print()
    print("Final Specification:")
    print(specifications)

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
    dafny_code, code_reqs = convert_intent_and_final_spec_to_dafny_and_code_reqs(intent.content[0], final_spec, model)
    # 4. Combine them into a formal prompt
    formal_prompt = convert_to_formal_prompt(intent.content[0], dafny_code.content[0], code_reqs.content[0])
    return formal_prompt


def test_formalize_user_prompt():
    # user_prompt = "Write a javascript function to check if a number is prime. If the input isn't a positive integer, it should throw an error."
    user_prompt = "Write me a TFHE C code to bitwise XOR 2 integers. The security parameter should be at least 150 bits."
    model = "deepseek/deepseek-chat-v3.1:free"
    formal_prompt = formalize_user_prompt(user_prompt, model)
    print("User Prompt:",'\n', user_prompt)
    print()
    print("Formal Prompt:")
    print(formal_prompt)


def test_complete_spec():
    partial_spec = {
        'n': 750,
        'xs_sigma': 0.5,
        'xs_mu': 0.5,
        'xe_sigma': 131072.0,
        'xe_mu': 0.0
    }
    completed_spec = complete_spec(partial_spec)
    print("Completed Specification:")
    print(completed_spec)


if __name__ == "__main__":
    # test_convert_to_formal_prompt()
    # test_convert_intent_and_final_spec_to_dafny_and_code_reqs()
    # test_extract_intent_and_spec()
    test_formalize_user_prompt()
    # test_complete_spec()
    pass