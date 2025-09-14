import os
import re
from collections import Counter
from nltk.util import ngrams
from pygments import lex
from pygments.lexers.c_cpp import CLexer
from pygments.token import Comment
from crystalbleu import corpus_bleu
from nltk.translate.bleu_score import SmoothingFunction

# Tokenization Function (Excludes Comments)
def tokenize_code(code):
    lexer = CLexer()
    return [
        token[1] for token in lexer.get_tokens(code)
        if not (re.fullmatch(r'\s+', token[1]) or token[0] in Comment)
    ]

# Function to Calculate Trivially Shared N-grams
def get_trivially_shared_ngrams(ref_tokens, gen_tokens, k=50):
    combined_tokens = ref_tokens + gen_tokens
    all_ngrams = []
    for n in range(3, 5):  # Focus on 3-grams and 4-grams
        all_ngrams.extend(list(ngrams(combined_tokens, n)))
    frequencies = Counter(all_ngrams)
    return dict(frequencies.most_common(k))

# Function to Compute CrystalBLEU Score
def compute_crystalbleu(reference_code, generated_code, k=50):
    ref_tokens = tokenize_code(reference_code)
    gen_tokens = tokenize_code(generated_code)
    
    trivially_shared_ngrams = get_trivially_shared_ngrams(ref_tokens, gen_tokens, k)
    
    smoothing = SmoothingFunction().method1
    score = corpus_bleu(
        [[ref_tokens]], 
        [gen_tokens],
        ignoring=trivially_shared_ngrams,
        smoothing_function=smoothing
    )
    return score

# Paths
logs_dir = 'logs_zscot'
references_dir = 'references'

# folder format: logs/<model_name>/<task_name>/<run_id>/

# Find All Runs
for model_name in os.listdir(logs_dir):
    model_path = os.path.join(logs_dir, model_name)
    if not os.path.isdir(model_path):
        continue

    for task_name in os.listdir(model_path):
        task_path = os.path.join(model_path, task_name)
        if not os.path.isdir(task_path):
            continue
        
        # Load Reference
        reference_file = os.path.join(references_dir, f"{task_name}.c")
        if not os.path.exists(reference_file):
            print(f"Reference not found for task {task_name}")
            continue

        with open(reference_file, 'r') as ref_f:
            reference_code = ref_f.read()

        # Process Each Run
        for run_id in os.listdir(task_path):
            run_path = os.path.join(task_path, run_id)
            # generated_file = os.path.join(run_path, f'{model_name}_{task_name}_{run_id}.cpp')
            generated_file = os.path.join(run_path, f'program.c')
            report_file = os.path.join(run_path, 'cystalbleu.log')

            if not os.path.exists(generated_file):
                print(f"No generated.c in {run_path}")
                continue

            with open(generated_file, 'r') as gen_f:
                generated_code = gen_f.read()

            # Compute CrystalBLEU
            score = compute_crystalbleu(reference_code, generated_code, k=50)

            # Save Report
            with open(report_file, 'w') as report_f:
                report_f.write(f"CrystalBLEU Score: {score:.6f}\n")

            print(f"Run {run_id}: CrystalBLEU = {score:.6f}")
