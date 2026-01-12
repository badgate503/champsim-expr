# Visualization Frontend

Run the local Flask server and open the page at http://localhost:8000

Requirements:

- Python 3
- `flask` package (install with `pip install flask`)

Run:

```bash
cd expr/result/visualization2
python app.py
```

The UI will list available trace sets (CSV files in `expr/result/`) and metrics.
Select a traceset and a metric to view a grouped bar chart of all traces and prefetchers.
