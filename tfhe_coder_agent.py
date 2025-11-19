import os
import logging
from agents.react_agent import ReactAgent
from agents.tool import tool
from tools.compiler import Compiler
from tools.executor import Executor
from tools.security_check import check_secure
from tools.summary_rag import SummaryRAG

# model = "deepseek/deepseek-chat-v3.1:free"

def get_logger(save_dir: str):
    log_file = os.path.join(save_dir, "run.log")

    logger = logging.getLogger("experiment")
    logger.setLevel(logging.INFO)

    # Clear existing handlers if you re-run in the same process
    if logger.hasHandlers():
        logger.handlers.clear()

    # File handler
    fh = logging.FileHandler(log_file)
    fh.setLevel(logging.INFO)

    # Console handler
    ch = logging.StreamHandler()
    ch.setLevel(logging.INFO)

    # Format
    formatter = logging.Formatter("%(asctime)s - %(levelname)s - %(message)s")
    fh.setFormatter(formatter)
    ch.setFormatter(formatter)

    logger.addHandler(fh)
    logger.addHandler(ch)

    return logger


def setup_tfhe_agent(model, task):
    save_dir = f"chatting_output/{task}/agent_tfhe"
    os.makedirs(save_dir, exist_ok=True)
    test_dir = f"unit_tests/{task}"
    compiler = Compiler(save_dir=save_dir)
    executor = Executor(save_dir=save_dir, test_dir=test_dir)
    retriever = SummaryRAG(
        summary_db_path="./tfhe_documentation/summaries_db.json",
        embedding_model="text-embedding-3-small",
        persist_directory="./chroma_tfhe_summaries",
    )

    logger = get_logger(save_dir)

    @tool
    def compile_execute_secure_code(code: str) -> str:
        """
        Compiles the given C file and executes it on the test cases. After that, it runs a security check on the code.

        Args:
            code (str): The C code to compile and execute.
        
        Returns:
            str: Success message if both compilation and execution are successful, error message otherwise.
        """
        compile_result = compiler.compile(code)
        if "Compilation successful" in compile_result:
            execute_result = executor.execute()
            if "successfully" in execute_result:
                # run security check
                security_result = check_secure(f"{save_dir}/program.c")
                return security_result
            else:
                return execute_result
        else:
            return compile_result


    @tool
    def rag(query: str) -> str:
        """
        This tool will retrieve the most relevant docstrings and function signatures for the query.

        Args:
            query (str): The query string to search for.
        Returns:
            str: Retrieved function signatures and summaries.
        """
        sig, summary, doxygen = retriever.retrieve(query, k=1)[0]
        return "\n".join([doxygen, sig])
    
    agent = ReactAgent(tools=[compile_execute_secure_code, rag], model=model, seed=0, logger=logger)

    return agent, save_dir

