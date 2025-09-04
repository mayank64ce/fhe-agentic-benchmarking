import subprocess
from pathlib import Path

# NOTE TO SELF: This executor is too strict. It compares string of outputs not the values themselves.

class Executor:
    """
    A simplified class to execute C++ code on test cases.
    """
    def __init__(self, save_dir, test_dir):
        self.save_dir = Path(save_dir)
        self.test_dir = Path(test_dir)
        self.executable = self.save_dir / "program"
    
    def execute(self) -> str:
        """
        Execute the compiled program against test cases.
        Returns a simple status message.
        """
        if not self.executable.exists():
            return "Error: Executable not found"
        
        # Find test files
        test_files = list(self.test_dir.glob("test_*.txt"))
        if not test_files:
            return "No test cases found"
        
        passed = 0
        total = len(test_files)
        
        for test_file in sorted(test_files):
            # Get corresponding solution file
            idx = test_file.stem.split("_", 1)[1]
            sol_file = self.test_dir / f"sol_{idx}.txt"
            
            if not sol_file.exists():
                continue
                
            # Run test
            stdin_data = test_file.read_text()
            expected = sol_file.read_text().strip()
            
            try:
                result = subprocess.run(
                    [str(self.executable)],
                    input=stdin_data,
                    capture_output=True,
                    text=True,
                    timeout=5
                )
                
                if result.returncode == 0 and result.stdout.strip() == expected:
                    passed += 1
                    
            except subprocess.TimeoutExpired:
                continue
        
        if passed == total:
            return "Code executed successfully on all test cases."
        else:
            return f"Code passed {passed}/{total} test cases."