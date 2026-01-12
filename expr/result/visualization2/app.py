from flask import Flask, send_from_directory, jsonify, request, abort
import os
import csv

app = Flask(__name__, static_folder='static')

# CSVs are one level up from this folder
CSV_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))


# Human-readable translations for common metrics
METRIC_TRANSLATIONS = {
    'IPC': 'IPC（每周期指令数）',
    'IPCI': 'IPCI（相对 IPC）',
    'L2C_Coverage': 'L2 命中覆盖率',
    'L2C_Accuracy': 'L2 命中准确度',
    'L2C_Overprediction': 'L2 过预测',
    'L2C_Timeliness': 'L2 及时性',
    'DRAM_Traffic': 'DRAM 流量（相对）',
    'L1D_average_miss_latency': 'L1D 平均缺失延迟',
    'L2C_average_miss_latency': 'L2 平均缺失延迟',
    'LLC_average_miss_latency': 'LLC 平均缺失延迟',
    'L1-MPKI': 'L1 MPKI',
    'L2-MPKI': 'L2 MPKI',
    'MPKI': '总 MPKI'
}


def list_csv_files():
    files = []
    for f in os.listdir(CSV_DIR):
        if f.lower().endswith('.csv'):
            files.append(f)
    files.sort()
    return files


def read_csv_header(path):
    with open(path, 'r', encoding='utf-8') as fh:
        reader = csv.reader(fh)
        header = next(reader)
    return header


def read_csv_rows(path):
    rows = []
    with open(path, 'r', encoding='utf-8') as fh:
        reader = csv.reader(fh)
        header = next(reader)
        for r in reader:
            if not r:
                continue
            rows.append(r)
    return header, rows


@app.route('/')
def index():
    return send_from_directory(app.static_folder, 'index.html')


@app.route('/<path:filename>')
def static_files(filename):
    return send_from_directory(app.static_folder, filename)


@app.route('/api/tracesets')
def api_tracesets():
    files = list_csv_files()
    # return name without extension as id
    data = [{'id': os.path.splitext(f)[0], 'file': f} for f in files]
    return jsonify(data)


@app.route('/api/metrics')
def api_metrics():
    traceset = request.args.get('traceset')
    if not traceset:
        return abort(400, 'traceset required')
    filename = traceset if traceset.lower().endswith('.csv') else traceset + '.csv'
    path = os.path.join(CSV_DIR, filename)
    if not os.path.exists(path):
        return abort(404, 'traceset not found')
    header = read_csv_header(path)
    # exclude Trace and Prefetcher
    metrics = [h for h in header if h not in ('Trace', 'Prefetcher')]
    translated = [{ 'key': m, 'name': METRIC_TRANSLATIONS.get(m, m) } for m in metrics]
    return jsonify(translated)


@app.route('/api/data')
def api_data():
    traceset = request.args.get('traceset')
    metric = request.args.get('metric')
    if not traceset or not metric:
        return abort(400, 'traceset and metric required')
    filename = traceset if traceset.lower().endswith('.csv') else traceset + '.csv'
    path = os.path.join(CSV_DIR, filename)
    if not os.path.exists(path):
        return abort(404, 'traceset not found')

    header, rows = read_csv_rows(path)
    # find column indices
    try:
        trace_idx = header.index('Trace')
        prefetch_idx = header.index('Prefetcher')
        metric_idx = header.index(metric)
    except ValueError:
        return abort(400, 'invalid metric or CSV missing required columns')

    traces = []
    prefetchers = []
    # maintain order of appearance
    values = {}

    for r in rows:
        trace = r[trace_idx]
        pre = r[prefetch_idx]
        # extend trace list if new
        if trace not in traces:
            traces.append(trace)
        if pre not in prefetchers:
            prefetchers.append(pre)
            values[pre] = [None] * len(traces)
        # ensure values arrays have correct length if new trace discovered after prefetcher
        for p in prefetchers:
            if len(values[p]) < len(traces):
                values[p].extend([None] * (len(traces) - len(values[p])))

        # parse metric value
        raw = r[metric_idx] if metric_idx < len(r) else ''
        try:
            val = float(raw)
        except Exception:
            val = None

        # set value at trace index
        t_index = traces.index(trace)
        values[pre][t_index] = val

    # Ensure all value arrays align with traces length
    for p in prefetchers:
        if len(values[p]) < len(traces):
            values[p].extend([None] * (len(traces) - len(values[p])))

    return jsonify({'traces': traces, 'prefetchers': prefetchers, 'values': values})


if __name__ == '__main__':
    # Run on port 8000
    app.run(host='0.0.0.0', port=8003, debug=True)
