#!bin/bash
for run_id in {0..4}
do
    echo "Starting run ${run_id}"
    python test_formal_rag_agent.py --run_id ${run_id}
    
    if [ $run_id -lt 4 ]; then
        sleep_time=$((5 + RANDOM % 26))  # Random time between 5 and 30 seconds
        echo "Sleeping for ${sleep_time} seconds before next run"
        sleep $sleep_time
    fi
done