import subprocess

def save_code_to_file(code: str, file_path: str) -> None:
    """
    Saves the given code to a file.

    Args:
        code (str): The code to save.
        file_path (str): The path to the file where the code will be saved.
    """
    with open(file_path, "w") as f:
        f.write(code)

class Compiler:
    """
    A class to compile C++ code.
    """

    def __init__(self, save_dir):
        self.save_dir = save_dir

    def compile(self, code: str=None, cpp_file: str=None) -> str:
        # first save the code
        if cpp_file is None:
            cpp_file = f"{self.save_dir}/program.c"
            exe_file = f"{self.save_dir}/program"
            save_code_to_file(code, cpp_file)
        else:
            with open(cpp_file, "r") as f:
                code = f.read()
            exe_file = cpp_file.replace(".c", "")
        
        # then compile the code
        try:
            # Compile the code
            result = subprocess.run(
                ["g++", cpp_file, "-o", exe_file, "-I/usr/local/include", "-L/usr/local/lib","-ltfhe-spqlios-fma"],
                capture_output=True,
                text=True,
                check=False
            )
        except Exception as e:
            return f"Error during compilation: {e}"

        if result.returncode == 0:
            return "Compilation successful."
        else:
            return f"Compilation failed: {result.stderr[:50]}"
        
        # Check compilation result
        

