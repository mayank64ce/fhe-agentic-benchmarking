import os
from tools.security_check import check_secure

def evaluate_security_over_runs(
    logs_dir: str = 'logs_zscot',
    generated_filename: str = 'program.c',
    report_filename: str = 'security.log',
):
    """
    For each run folder at logs_dir/<model>/<task>/<run>/,
    call check_secure() on generated_filename and write report_filename.
    """
    for model_name in os.listdir(logs_dir):
        model_path = os.path.join(logs_dir, model_name)
        if not os.path.isdir(model_path):
            continue

        for task_name in os.listdir(model_path):
            task_path = os.path.join(model_path, task_name)
            if not os.path.isdir(task_path):
                continue

            for run_id in os.listdir(task_path):
                run_path = os.path.join(task_path, run_id)
                if not os.path.isdir(run_path):
                    continue

                generated_file = os.path.join(run_path, generated_filename)
                report_file = os.path.join(run_path, report_filename)

                if not os.path.exists(generated_file):
                    print(f"[skip] No {generated_filename} in {run_path}")
                    continue

                is_secure = check_secure(generated_file)

                with open(report_file, 'w', encoding='utf-8') as rep:
                    rep.write(f"Secure: {is_secure}\n")

                print(f"[{model_name}/{task_name}/{run_id}] Secure={is_secure}")


if __name__ == "__main__":
    evaluate_security_over_runs("logs_zscot")
