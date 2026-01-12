let tracesetSelect = null;
let metricSelect = null;
let chart = null;

function $(id) { return document.getElementById(id); }

async function fetchJSON(url) {
  const r = await fetch(url);
  if (!r.ok) throw new Error(await r.text());
  return r.json();
}

function colorForIndex(i) {
  const palette = [
    '#3366cc','#dc3912','#ff9900','#109618','#990099','#0099c6','#dd4477','#66aa00'
  ];
  return palette[i % palette.length];
}

function renderChart(data, metricName) {
  const ctx = $('barChart').getContext('2d');
  const traces = data.traces;
  const prefetchers = data.prefetchers;
  const values = data.values;

  const datasets = prefetchers.map((p, i) => ({
    label: p,
    data: values[p].map(v => v === null ? null : +v),
    backgroundColor: colorForIndex(i),
  }));

  if (chart) chart.destroy();
  chart = new Chart(ctx, {
    type: 'bar',
    data: { labels: traces, datasets },
    options: {
      responsive: true,
      plugins: { legend: { position: 'top' }, title: { display: true, text: metricName } },
      scales: { x: { stacked: false }, y: { beginAtZero: false } }
    }
  });
}

async function loadTracesets() {
  const sets = await fetchJSON('/api/tracesets');
  tracesetSelect.innerHTML = '';
  sets.forEach(s => {
    const opt = document.createElement('option');
    opt.value = s.id;
    opt.textContent = s.id;
    tracesetSelect.appendChild(opt);
  });
  if (sets.length) {
    tracesetSelect.value = sets[0].id;
    await loadMetrics();
    await updateData();
  }
}

async function loadMetrics() {
  const ts = tracesetSelect.value;
  if (!ts) return;
  const metrics = await fetchJSON(`/api/metrics?traceset=${encodeURIComponent(ts)}`);
  metricSelect.innerHTML = '';
  metrics.forEach(m => {
    const opt = document.createElement('option');
    opt.value = m.key;
    opt.textContent = m.name;
    metricSelect.appendChild(opt);
  });
  if (metrics.length) metricSelect.value = metrics[0].key;
}

async function updateData() {
  const ts = tracesetSelect.value;
  const metric = metricSelect.value;
  if (!ts || !metric) return;
  const data = await fetchJSON(`/api/data?traceset=${encodeURIComponent(ts)}&metric=${encodeURIComponent(metric)}`);
  // metricSelect option text is the readable name
  const metricName = metricSelect.options[metricSelect.selectedIndex].text;
  renderChart(data, metricName || metric);
}

document.addEventListener('DOMContentLoaded', async () => {
  tracesetSelect = $('traceset-select');
  metricSelect = $('metric-select');
  tracesetSelect.addEventListener('change', async () => { await loadMetrics(); await updateData(); });
  metricSelect.addEventListener('change', updateData);
  try {
    await loadTracesets();
  } catch (e) {
    console.error(e);
    alert('无法加载数据: ' + e.message);
  }
});
