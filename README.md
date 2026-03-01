# FHE-Bench: Benchmarking LLM Agents for Fully Homomorphic Encryption Code Generation

[![ICLR 2026](https://img.shields.io/badge/ICLR-2026-blue.svg)](https://iclr.cc/)
[![Python 3.12](https://img.shields.io/badge/Python-3.12-green.svg)](https://python.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

This repository contains the official implementation for our ICLR 2026 paper:

> **FHE-Bench: Benchmarking LLM Agents for Fully Homomorphic Encryption Code Generation**

> **Can LLMs write correct and secure Fully Homomorphic Encryption (FHE) code?**

This benchmark evaluates large language models on their ability to implement cryptographically correct and secure programs using the [OpenFHE](https://github.com/openfheorganization/openfhe-development) TFHE scheme. We measure both *functional correctness* and *security compliance*, and combine them into a single joint metric: **pass@1(func\_sec)**.

---

## 🏆 Model Leaderboard

Scores are the **weighted average of pass@1(func\_sec) across all 10 tasks**, using the best technique for each task:
- Simple tasks → **FHE-Coder** (B + Formal Prompt + Security Check + RAG)
- Complex tasks → **FHE-Coder + Human Guidance** (structured decomposition)

| Rank | Model | Weighted Avg ↓ | Adder | AND | CNN | Dot Product | Mat×Mat | Mat×Vec | MLP | Multiplier | ReLU | Vec Add |
|------|-------|:--------------:|:-----:|:---:|:---:|:-----------:|:-------:|:-------:|:---:|:----------:|:----:|:-------:|
| 🥇 1 | **GPT-5** | **0.86** | 1.00 | 1.00 | 0.80 | 0.80 | 0.80 | 0.80 | 0.40 | 1.00 | 1.00 | 1.00 |
| 🥈 2 | **Gemini-2.5-Pro** | **0.78** | 1.00 | 0.80 | 0.80 | 0.80 | 0.80 | 0.80 | 0.20 | 0.80 | 1.00 | 0.80 |
| 🥈 2 | **DeepSeek-V3.1** | **0.78** | 1.00 | 0.80 | 0.60 | 0.60 | 0.80 | 0.80 | 0.80 | 0.80 | 0.80 | 0.80 |
| 4 | **Qwen-2.5-Coder-480B** | **0.50** | 0.80 | 1.00 | 0.20 | 0.40 | 0.20 | 0.40 | 0.00 | 0.80 | 0.80 | 0.40 |

## Overview

FHE-Bench is a comprehensive benchmark for evaluating the ability of Large Language Model (LLM) agents to generate correct and secure Fully Homomorphic Encryption (FHE) code. The benchmark features:

- **Formal Specifications**: Tasks defined with Dafny-like formal specifications including pre/postconditions
- **Security Validation**: Automated verification of cryptographic security parameters using lattice estimator
- **RAG Augmentation**: Retrieval-Augmented Generation for FHE API documentation
- **Multi-Library Support**: Support for TFHE and OpenFHE (CKKS scheme)

## Key Features

- **ReAct Agent Framework**: LLM agents that reason and act iteratively to generate FHE code
- **Diverse Task Suite**: Cryptographic operations from bitwise AND to full transformer blocks
- **Automated Testing**: Code compilation, execution, and correctness verification
- **Security Analysis**: Lattice-based security parameter validation (minimum λ enforcement)

## Installation

### Prerequisites

- [Miniconda](https://docs.conda.io/en/latest/miniconda.html) or [Anaconda](https://www.anaconda.com/)
- [SageMath](https://doc.sagemath.org/html/en/installation/index.html) (required for lattice estimator)

### Setup

1. **Clone the repository**
   ```bash
   git clone https://github.com/your-username/tfhe-agentic-benchmarking.git
   cd tfhe-agentic-benchmarking
   ```

2. **Create the conda environment**
   ```bash
   conda env create -f environment.yml
   conda activate sage
   ```

3. **Clone the lattice estimator**
   ```bash
   git clone https://github.com/malb/lattice-estimator.git tools/lattice_estimator
   ```

4. **Configure API keys**

   Create a `.env` file in the project root:
   ```bash
   OPENROUTER_API_KEY=your_openrouter_api_key
   OPENAI_API_KEY=your_openai_api_key
   ```

## Usage

### Running Experiments

```bash
# Basic agent (code generation only)
python test_basic_agent.py --run_id 0

# RAG-augmented agent
python test_rag_agent.py --run_id 0

# Formal specification agent
python test_formal_agent.py --run_id 0

# Full pipeline: formal + secure + RAG
python test_formal_secure_rag_agent.py --run_id 0
```

### Interactive Interface

```bash
streamlit run chat_bot_triple.py
```

## Project Structure

```
tfhe-agentic-benchmarking/
├── agents/                      # Core agent framework
│   ├── react_agent.py          # ReAct agent implementation
│   ├── tool.py                 # Tool decorator and execution
│   └── utils/                  # Utilities (logging, completions)
├── tools/                       # Agent tools
│   ├── compiler.py             # Code compilation
│   ├── executor.py             # Test execution
│   ├── security_check.py       # Security validation
│   ├── summary_rag.py          # RAG for API docs
│   └── lattice_estimator/      # Security estimation (external)
├── prompts/                     # Task definitions
│   ├── task_and.py             # Bitwise AND
│   ├── task_relu.py            # ReLU activation
│   ├── task_dot_product.py     # Vector dot product
│   ├── task_matrix_*.py        # Matrix operations
│   ├── task_attention.py       # Attention mechanism
│   ├── task_transformer.py     # Transformer block
│   └── task_softmax.py         # Softmax operation
├── tfhe_documentation/          # TFHE API summaries
├── openfhe_documentation/       # OpenFHE API summaries
├── logs_*/                      # Experiment logs
└── environment.yml              # Conda environment
```

## Benchmark Tasks

| Task | Description | FHE Scheme |
|------|-------------|------------|
| `task_and` | Bitwise AND operation | TFHE |
| `task_relu` | ReLU activation function | CKKS |
| `task_dot_product` | Vector dot product | CKKS |
| `task_matrix_vector_mult` | Matrix-vector multiplication | CKKS |
| `task_matrix_matrix_mult` | Matrix-matrix multiplication | CKKS |
| `task_attention` | Attention mechanism | CKKS |
| `task_transformer` | Full transformer block | CKKS |
| `task_softmax` | Softmax operation | CKKS |

## Agent Configurations

| Configuration | Description |
|--------------|-------------|
| `basic` | Code generation only |
| `rag` | + API documentation retrieval |
| `formal` | + Formal specifications |
| `secure` | + Security validation |
| `formal_secure_rag` | Full pipeline |

## Citation

If you find this work useful, please cite our paper:

```bibtex
@inproceedings{
    author2026fhebench,
    title={FHE-Bench: Benchmarking LLM Agents for Fully Homomorphic Encryption Code Generation},
    author={Author Names},
    booktitle={International Conference on Learning Representations},
    year={2026},
    url={https://openreview.net/forum?id=XXXXX}
}
```

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- [Lattice Estimator](https://github.com/malb/lattice-estimator) for security parameter estimation
- [TFHE Library](https://tfhe.github.io/tfhe/) for threshold FHE implementation
- [OpenFHE](https://www.openfhe.org/) for CKKS scheme support
