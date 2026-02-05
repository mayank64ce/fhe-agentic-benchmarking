from .task_and import *
from .task_relu import *
from .task_transformer import *
task_and_prompts = {
    'informal': task_prompt,
    '0': task_prompt_formal_0,
    '1': task_prompt_formal_1,
    '2': task_prompt_formal_2,
    '3': task_prompt_formal_3,
    '4': task_prompt_formal_4,
}

task_relu_prompts = {
    'informal': task_prompt_relu,
    '0': task_prompt_relu_formal,
}

task_transformer_prompts = {
    '0': task_prompt_formal_transformer,
}