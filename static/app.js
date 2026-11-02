/**
 * Frontend JavaScript Controller
 * Monte Carlo Pi Simulation & Parallel Computing Dashboard
 */

const STATE = {
    threads: 4,
    samples: 10000000,
    speedupChart: null,
    timeChart: null,
    benchmarkData: []
};

document.addEventListener('DOMContentLoaded', () => {
    fetchSystemInfo();
    initCanvas();
    animateRandomScatter();
    initCharts();
});

// ============================================================================
// SYSTEM INFO & TABS
// ============================================================================
async function fetchSystemInfo() {
    try {
        const res = await fetch('/api/system-info');
        const data = await res.json();
        const cpuText = document.getElementById('cpuInfoText');
        if (cpuText) {
            cpuText.innerText = `${data.cpu_cores} Cores CPU Aktif`;
        }
        const threadRange = document.getElementById('threadRange');
        if (threadRange && data.cpu_cores) {
            threadRange.max = Math.max(16, data.cpu_cores);
        }
    } catch (e) {
        console.error("Gagal mengambil info sistem:", e);
    }
}

function switchTab(tabName) {
    const tabs = [
        { key: 'simulation', contentId: 'tabSimulation', btnId: 'tabBtnSimulation' },
        { key: 'benchmark', contentId: 'tabBenchmark', btnId: 'tabBtnBenchmark' },
        { key: 'race', contentId: 'tabRaceDemo', btnId: 'tabBtnRaceDemo' }
    ];

    tabs.forEach(t => {
        const content = document.getElementById(t.contentId);
        const btn = document.getElementById(t.btnId);
        if (content) content.classList.remove('active');
        if (btn) btn.classList.remove('active');
    });

    const activeTab = tabs.find(t => t.key === tabName);
    if (activeTab) {
        const activeContent = document.getElementById(activeTab.contentId);
        const activeBtn = document.getElementById(activeTab.btnId);
        if (activeContent) activeContent.classList.add('active');
        if (activeBtn) activeBtn.classList.add('active');
    }

    // Trigger chart resize if benchmark tab is opened
    if (tabName === 'benchmark') {
        if (STATE.speedupChart) STATE.speedupChart.resize();
        if (STATE.timeChart) STATE.timeChart.resize();
    }
}

// ============================================================================
// PARAMETER CONTROLS
// ============================================================================
function updateThreadUI(val) {
    STATE.threads = parseInt(val);
    document.getElementById('threadValue').innerText = `${val} Threads`;
    
    // Update button chips active states
    document.querySelectorAll('.quick-buttons button').forEach(btn => {
        btn.classList.remove('active');
        if (btn.innerText.includes(`${val} Core`)) {
            btn.classList.add('active');
        }
    });
}

function setThreadPreset(val) {
    const range = document.getElementById('threadRange');
    if (range) range.value = val;
    updateThreadUI(val);
}

function setSamplePreset(samples, label) {
    STATE.samples = samples;
    document.getElementById('samplesInput').value = samples;
    document.getElementById('sampleCountDisplay').innerText = `${label} (${samples.toLocaleString()} Titik)`;

    document.querySelectorAll('.sample-presets .btn-chip').forEach(btn => {
        btn.classList.remove('active');
    });
    event.target.classList.add('active');
}

// ============================================================================
// RUN SINGLE SIMULATION (OPENMP C BACKEND)
// ============================================================================
async function runSimulation() {
    const btn = document.getElementById('btnRunSimulation');
    const spinner = document.getElementById('runSpinner');
    const btnText = btn.querySelector('.btn-text');

    btn.disabled = true;
    spinner.style.display = 'inline-block';
    btnText.style.display = 'none';

    try {
        const payload = {
            threads: STATE.threads,
            samples: STATE.samples
        };

        const res = await fetch('/api/run', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(payload)
        });

        const data = await res.json();
        if (data.success) {
            updateMetricsUI(data);
            animateRandomScatter(); // Refresh canvas with dynamic points
        } else {
            alert("Error: " + (data.error || "Gagal menjalankan simulasi"));
        }
    } catch (err) {
        console.error(err);
        alert("Gagal menghubungi server OpenMP backend.");
    } finally {
        btn.disabled = false;
        spinner.style.display = 'none';
        btnText.style.display = 'inline-flex';
    }
}

function updateMetricsUI(data) {
    const piVal = document.getElementById('metricPiEst');
    const piErr = document.getElementById('metricPiError');
    const timeVal = document.getElementById('metricTime');
    const insideVal = document.getElementById('metricInside');
    const ratioVal = document.getElementById('metricRatio');
    const tpVal = document.getElementById('metricThroughput');

    piVal.innerText = data.estimated_pi.toFixed(8);
    piErr.innerText = `Galat Error: ${data.absolute_error.toExponential(4)}`;
    timeVal.innerText = `${data.execution_time.toFixed(4)} s`;

    insideVal.innerText = `${data.inside_count.toLocaleString()} / ${data.total_samples.toLocaleString()}`;
    const ratio = (data.inside_count / data.total_samples) * 100;
    ratioVal.innerText = `Rasio: ${ratio.toFixed(4)}% (Ideal: 78.5398%)`;

    tpVal.innerText = `${data.throughput.toFixed(1)} Mpts/s`;

    const conv = document.getElementById('overlayConvergence');
    if (conv) {
        const acc = (1.0 - (data.absolute_error / Math.PI)) * 100;
        conv.innerText = `Akurasi: ${acc.toFixed(4)}%`;
    }
}

// ============================================================================
// BENCHMARK SUITE (10^9 SAMPLES)
// ============================================================================
async function runBenchmarkSuite() {
    const btn = document.getElementById('btnRunBenchmark');
    const spinner = document.getElementById('benchSpinner');
    const btnText = btn.querySelector('.btn-text');

    btn.disabled = true;
    spinner.style.display = 'inline-block';
    btnText.style.display = 'none';

    try {
        const payload = { samples: 1000000000 };
        const res = await fetch('/api/benchmark', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(payload)
        });

        const data = await res.json();
        if (data.success && data.results) {
            STATE.benchmarkData = data.results;
            populateBenchmarkTable(data.results, data.total_samples);
            updateCharts(data.results);
        } else {
            alert("Error: " + (data.error || "Gagal menjalankan benchmark suite"));
        }
    } catch (err) {
        console.error(err);
        alert("Gagal menjalankan benchmark skala 1 Miliar.");
    } finally {
        btn.disabled = false;
        spinner.style.display = 'none';
        btnText.style.display = 'inline-flex';
    }
}

function populateBenchmarkTable(results, totalSamples) {
    const tbody = document.getElementById('benchmarkTableBody');
    tbody.innerHTML = '';

    results.forEach(row => {
        const tr = document.createElement('tr');
        tr.innerHTML = `
            <td><strong>${row.threads} Thread${row.threads > 1 ? 's' : ''}</strong></td>
            <td>${totalSamples.toLocaleString()}</td>
            <td>${row.inside_count.toLocaleString()}</td>
            <td class="cyan font-mono">${row.estimated_pi.toFixed(9)}</td>
            <td class="font-mono">${row.absolute_error.toExponential(3)}</td>
            <td class="emerald font-mono">${row.execution_time.toFixed(4)} s</td>
            <td><span class="speedup-badge">${row.speedup.toFixed(2)}x</span></td>
            <td><span class="font-mono">${row.efficiency.toFixed(1)}%</span></td>
        `;
        tbody.appendChild(tr);
    });
}

function exportTableCSV() {
    if (!STATE.benchmarkData || STATE.benchmarkData.length === 0) {
        alert("Jalankan benchmark terlebih dahulu sebelum mengekspor data.");
        return;
    }

    let csv = "threads,total_samples,inside_count,estimated_pi,absolute_error,execution_time,speedup,efficiency\n";
    STATE.benchmarkData.forEach(r => {
        csv += `${r.threads},1000000000,${r.inside_count},${r.estimated_pi},${r.absolute_error},${r.execution_time},${r.speedup},${r.efficiency}\n`;
    });

    const blob = new Blob([csv], { type: 'text/csv' });
    const url = window.URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.setAttribute('href', url);
    a.setAttribute('download', 'benchmark_monte_carlo_pi.csv');
    a.click();
}

// ============================================================================
// CHART.JS INITIALIZATION & UPDATES
// ============================================================================
function initCharts() {
    const defaultThreads = [1, 2, 4, 8];
    const defaultSpeedup = [1.0, 1.98, 2.65, 3.89];
    const defaultTime = [3.868, 1.951, 1.460, 0.995];

    // Speedup Chart
    const ctxSpeedup = document.getElementById('speedupChart').getContext('2d');
    STATE.speedupChart = new Chart(ctxSpeedup, {
        type: 'line',
        data: {
            labels: defaultThreads.map(t => `${t} Core`),
            datasets: [
                {
                    label: 'Speedup Aktual (OpenMP)',
                    data: defaultSpeedup,
                    borderColor: '#38bdf8',
                    backgroundColor: 'rgba(56, 189, 248, 0.2)',
                    borderWidth: 3,
                    pointBackgroundColor: '#38bdf8',
                    pointRadius: 6,
                    tension: 0.2,
                    fill: true
                },
                {
                    label: 'Ideal Linear (100% Efisiensi)',
                    data: defaultThreads,
                    borderColor: '#10b981',
                    borderWidth: 2,
                    borderDash: [5, 5],
                    pointRadius: 0,
                    fill: false
                }
            ]
        },
        options: getChartOptions('Speedup Multiplier (x)')
    });

    // Time Chart
    const ctxTime = document.getElementById('timeChart').getContext('2d');
    STATE.timeChart = new Chart(ctxTime, {
        type: 'bar',
        data: {
            labels: defaultThreads.map(t => `${t} Core`),
            datasets: [{
                label: 'Waktu Eksekusi (detik)',
                data: defaultTime,
                backgroundColor: [
                    'rgba(244, 63, 94, 0.7)',
                    'rgba(245, 158, 11, 0.7)',
                    'rgba(59, 130, 246, 0.7)',
                    'rgba(16, 185, 129, 0.7)'
                ],
                borderColor: [
                    '#f43f5e',
                    '#f59e0b',
                    '#3b82f6',
                    '#10b981'
                ],
                borderWidth: 1.5,
                borderRadius: 8
            }]
        },
        options: getChartOptions('Waktu (detik)')
    });

    // Render initial sample table data
    populateBenchmarkTable([
        { threads: 1, inside_count: 785406850, estimated_pi: 3.1416274, absolute_error: 3.47e-5, execution_time: 3.868, speedup: 1.0, efficiency: 100.0 },
        { threads: 2, inside_count: 785416573, estimated_pi: 3.1416662, absolute_error: 7.36e-5, execution_time: 1.951, speedup: 1.98, efficiency: 99.1 },
        { threads: 4, inside_count: 785413371, estimated_pi: 3.1416534, absolute_error: 6.08e-5, execution_time: 1.460, speedup: 2.65, efficiency: 66.2 },
        { threads: 8, inside_count: 785389258, estimated_pi: 3.1415570, absolute_error: 3.56e-5, execution_time: 0.995, speedup: 3.89, efficiency: 48.6 }
    ], 1000000000);
}

function getChartOptions(yTitle) {
    return {
        responsive: true,
        maintainAspectRatio: false,
        plugins: {
            legend: {
                labels: { color: '#334155', font: { family: 'Inter', size: 12, weight: '600' } }
            }
        },
        scales: {
            x: {
                grid: { color: 'rgba(0, 0, 0, 0.06)' },
                ticks: { color: '#475569', font: { family: 'Inter', weight: '600' } }
            },
            y: {
                title: { display: true, text: yTitle, color: '#1e293b', font: { weight: '600' } },
                grid: { color: 'rgba(0, 0, 0, 0.06)' },
                ticks: { color: '#475569', font: { family: 'JetBrains Mono', weight: '600' } }
            }
        }
    };
}

function updateCharts(results) {
    const labels = results.map(r => `${r.threads} Core`);
    const speedups = results.map(r => r.speedup);
    const ideals = results.map(r => r.threads);
    const times = results.map(r => r.execution_time);

    STATE.speedupChart.data.labels = labels;
    STATE.speedupChart.data.datasets[0].data = speedups;
    STATE.speedupChart.data.datasets[1].data = ideals;
    STATE.speedupChart.update();

    STATE.timeChart.data.labels = labels;
    STATE.timeChart.data.datasets[0].data = times;
    STATE.timeChart.update();
}

// ============================================================================
// RACE CONDITION LAB DEMO
// ============================================================================
async function runRaceDemo() {
    const btn = document.getElementById('btnRunRaceDemo');
    const spinner = document.getElementById('raceSpinner');
    const btnText = btn.querySelector('.btn-text');

    btn.disabled = true;
    spinner.style.display = 'inline-block';
    btnText.style.display = 'none';

    try {
        const payload = { samples: 20000000 };
        const res = await fetch('/api/race-demo', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(payload)
        });

        const data = await res.json();
        if (data.success) {
            document.getElementById('safePoints').innerText = `${data.safe.inside.toLocaleString()} titik`;
            document.getElementById('safePi').innerText = data.safe.pi.toFixed(8);
            document.getElementById('safeError').innerText = data.safe.error.toExponential(4);

            document.getElementById('unsafePoints').innerText = `${data.unsafe.inside.toLocaleString()} titik`;
            document.getElementById('unsafePi').innerText = data.unsafe.pi.toFixed(8);
            document.getElementById('unsafeError').innerText = data.unsafe.error.toFixed(4) + " (RUSAK)";

            const status = document.getElementById('raceLossStatus');
            status.innerHTML = `<i class="fa-solid fa-triangle-exclamation"></i> Terbukti Hilang ${data.lost_points.toLocaleString()} Titik!`;
        } else {
            alert("Error: " + (data.error || "Gagal menjalankan demo race condition"));
        }
    } catch (err) {
        console.error(err);
        alert("Gagal menghubungi endpoint lab race condition.");
    } finally {
        btn.disabled = false;
        spinner.style.display = 'none';
        btnText.style.display = 'inline-flex';
    }
}

// ============================================================================
// CANVAS MONTE CARLO VISUALIZER
// ============================================================================
let canvas, ctx;

function initCanvas() {
    canvas = document.getElementById('monteCarloCanvas');
    if (!canvas) return;
    ctx = canvas.getContext('2d');
    drawBaseCanvas();
}

function drawBaseCanvas() {
    const w = canvas.width;
    const h = canvas.height;

    ctx.fillStyle = '#070b13';
    ctx.fillRect(0, 0, w, h);

    // Grid lines
    ctx.strokeStyle = 'rgba(255, 255, 255, 0.05)';
    ctx.lineWidth = 1;
    for (let x = 0; x <= w; x += w / 4) {
        ctx.beginPath();
        ctx.moveTo(x, 0); ctx.lineTo(x, h);
        ctx.stroke();
    }
    for (let y = 0; y <= h; y += h / 4) {
        ctx.beginPath();
        ctx.moveTo(0, y); ctx.lineTo(w, y);
        ctx.stroke();
    }

    // Quarter-circle arc
    ctx.save();
    ctx.beginPath();
    ctx.arc(0, h, w, 1.5 * Math.PI, 2 * Math.PI);
    ctx.strokeStyle = 'rgba(6, 182, 212, 0.8)';
    ctx.lineWidth = 3;
    ctx.shadowColor = '#06b6d4';
    ctx.shadowBlur = 15;
    ctx.stroke();
    ctx.restore();
}

async function animateRandomScatter() {
    if (!ctx) return;
    try {
        const res = await fetch('/api/visual-sample?count=2200');
        const data = await res.json();
        
        drawBaseCanvas();

        const w = canvas.width;
        const h = canvas.height;

        data.points.forEach(pt => {
            const px = pt.x * w;
            const py = h - (pt.y * h);

            ctx.beginPath();
            ctx.arc(px, py, 1.8, 0, 2 * Math.PI);
            if (pt.inside) {
                ctx.fillStyle = 'rgba(6, 182, 212, 0.75)';
            } else {
                ctx.fillStyle = 'rgba(244, 63, 94, 0.65)';
            }
            ctx.fill();
        });
    } catch (e) {
        console.error("Gagal memuat visual sample:", e);
    }
}
