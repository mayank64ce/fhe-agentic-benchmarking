import { useState, useMemo } from 'react'
import './App.css'
import csvText from '../master_results_with_std.csv?raw'
import {
  BarChart,
  Bar,
  XAxis,
  YAxis,
  CartesianGrid,
  Tooltip,
  Legend,
  ResponsiveContainer,
  Cell,
} from 'recharts'

// ─── Constants ────────────────────────────────────────────────────────────────

const SIMPLE_TASKS = [
  'task_adder',
  'task_and',
  'task_relu',
  'task_multiplier',
  'task_dot_product',
  'task_vector_addition',
]
const COMPLEX_TASKS = [
  'task_cnn',
  'task_matrix_matrix',
  'task_matrix_vector',
  'task_mlp',
]
const ALL_TASKS = [...SIMPLE_TASKS, ...COMPLEX_TASKS]

const BEST_SIMPLE_WF = 'B+Formal+Security Check+RAG'
const BEST_COMPLEX_WF = 'B+Formal+Security Check+RAG + Human Guidance'

const TASK_LABEL = {
  task_adder: 'Adder',
  task_and: 'AND',
  task_relu: 'ReLU',
  task_multiplier: 'Mult.',
  task_dot_product: 'Dot Prod.',
  task_vector_addition: 'Vec Add',
  task_cnn: 'Conv',
  task_matrix_matrix: 'Mat×Mat',
  task_matrix_vector: 'Mat×Vec',
  task_mlp: 'MLP',
}

const MODELS = [
  'Claude-Opus-4.6',
  'GPT-5.4',
  'GPT-5',
  'GLM-4.5-Air',
  'Claude-Sonnet-4.6',
  'Gemini-2.5-pro',
  'Deepseek-Chat-V3.1',
  'Llama-4-Maverick',
  'Mistral-Large-2',
  'Qwen-3-Coder-480B-A35B',
]

const MODEL_LABEL = {
  'Claude-Opus-4.6': 'Claude-Opus-4.6',
  'GPT-5.4': 'GPT-5.4',
  'GPT-5': 'GPT-5',
  'GLM-4.5-Air': 'GLM-4.5-Air',
  'Claude-Sonnet-4.6': 'Claude-Sonnet-4.6',
  'Gemini-2.5-pro': 'Gemini-2.5-Pro',
  'Deepseek-Chat-V3.1': 'DeepSeek-V3.1',
  'Llama-4-Maverick': 'Llama-4-Maverick',
  'Mistral-Large-2': 'Mistral-Large-2',
  'Qwen-3-Coder-480B-A35B': 'Qwen-2.5-Coder-480B',
}

const MODEL_COLOR = {
  'Claude-Opus-4.6': '#e879f9',
  'GPT-5.4': '#60a5fa',
  'GPT-5': '#818cf8',
  'GLM-4.5-Air': '#34d399',
  'Claude-Sonnet-4.6': '#f97316',
  'Gemini-2.5-pro': '#f472b6',
  'Deepseek-Chat-V3.1': '#2dd4bf',
  'Llama-4-Maverick': '#fb923c',
  'Mistral-Large-2': '#a3e635',
  'Qwen-3-Coder-480B-A35B': '#fbbf24',
}

// Ablation steps for simple tasks
const ABLATION_STEPS_SIMPLE = [
  { key: 'baseline', label: 'Baseline', workflow: 'Baseline (B)' },
  { key: 'formal', label: '+Formal', workflow: 'B+Formal Prompt' },
  { key: 'formal_rag', label: '+Formal+RAG', workflow: 'B+Formal Prompt+RAG' },
  { key: 'fhecoder', label: 'FHE-Coder', workflow: 'B+Formal+Security Check+RAG' },
]

// Ablation steps for complex tasks include the human-guidance step
const ABLATION_STEPS_COMPLEX = [
  { key: 'baseline', label: 'Baseline', workflow: 'Baseline (B)' },
  { key: 'formal', label: '+Formal', workflow: 'B+Formal Prompt' },
  { key: 'formal_rag', label: '+Formal+RAG', workflow: 'B+Formal Prompt+RAG' },
  { key: 'fhecoder', label: 'FHE-Coder', workflow: 'B+Formal+Security Check+RAG' },
  {
    key: 'fhecoder_hg',
    label: 'FHE-Coder+HG',
    workflow: 'B+Formal+Security Check+RAG + Human Guidance',
  },
]

// ─── Data processing ──────────────────────────────────────────────────────────

function parseCSV(text) {
  const lines = text.trim().split('\n')
  return lines.slice(1).map(line => {
    const parts = line.split(',')
    return {
      task: parts[0]?.trim() ?? '',
      model: parts[1]?.trim() ?? '',
      workflow: parts[2]?.trim() ?? '',
      metric: parts[3]?.trim() ?? '',
      value: parts[4]?.trim() ? parseFloat(parts[4]) : null,
      stddev: parts[5]?.trim() ? parseFloat(parts[5]) : null,
    }
  }).filter(r => r.task && r.model && r.workflow && r.metric)
}

function buildIndex(rows) {
  const idx = new Map()
  for (const row of rows) {
    const key = `${row.task}||${row.model}||${row.workflow}`
    if (!idx.has(key)) {
      idx.set(key, { task: row.task, model: row.model, workflow: row.workflow })
    }
    const e = idx.get(key)
    if (row.metric === 'pass@1 (functionality)') {
      e.func = row.value
      e.funcStd = row.stddev
    } else if (row.metric === 'pass@1 (security)') {
      e.sec = row.value
      e.secStd = row.stddev
    } else if (row.metric === 'latency') {
      e.latency = row.value
    }
  }
  // Compute func_sec and f1 for every entry
  for (const e of idx.values()) {
    if (e.func != null && e.sec != null) {
      e.funcSec = Math.min(e.func, e.sec)
      e.funcSecStd = e.func <= e.sec ? e.funcStd : e.secStd
      e.f1 = (e.func + e.sec) > 0
        ? 2 * (e.func * e.sec) / (e.func + e.sec)
        : 0
    } else {
      e.funcSec = null
      e.funcSecStd = null
      e.f1 = null
    }
  }
  return idx
}

function get(idx, task, model, workflow) {
  return idx.get(`${task}||${model}||${workflow}`) ?? null
}

// Best-technique score for (model, task): func_sec
function bestScore(idx, task, model) {
  const wf = SIMPLE_TASKS.includes(task) ? BEST_SIMPLE_WF : BEST_COMPLEX_WF
  const r = get(idx, task, model, wf)
  if (r?.funcSec != null) return { score: r.funcSec, std: r.funcSecStd }
  // Fallback: max func_sec across all workflows
  let best = null
  for (const e of idx.values()) {
    if (e.task === task && e.model === model && e.funcSec != null) {
      if (!best || e.funcSec > best.score) {
        best = { score: e.funcSec, std: e.funcSecStd }
      }
    }
  }
  return best
}

// Average a scalar field across all tasks for a given (model, workflow)
function avgField(idx, model, workflow, field) {
  const vals = []
  for (const task of ALL_TASKS) {
    const e = get(idx, task, model, workflow)
    if (e && e[field] != null) vals.push(e[field])
  }
  return vals.length ? vals.reduce((a, b) => a + b, 0) / vals.length : null
}

// Average best-technique func_sec per model across all tasks
function avgBestFuncSec(idx, model) {
  const vals = []
  for (const task of ALL_TASKS) {
    const r = bestScore(idx, task, model)
    if (r) vals.push(r.score)
  }
  return vals.length ? vals.reduce((a, b) => a + b, 0) / vals.length : 0
}

// Average best-technique sec per model across all tasks
function avgBestSec(idx, model) {
  const vals = []
  for (const task of ALL_TASKS) {
    const wf = SIMPLE_TASKS.includes(task) ? BEST_SIMPLE_WF : BEST_COMPLEX_WF
    const e = get(idx, task, model, wf)
    if (e?.sec != null) vals.push(e.sec)
  }
  return vals.length ? vals.reduce((a, b) => a + b, 0) / vals.length : 0
}

// Average best-technique latency per model across all tasks
function avgBestLatency(idx, model) {
  const vals = []
  for (const task of ALL_TASKS) {
    const wf = SIMPLE_TASKS.includes(task) ? BEST_SIMPLE_WF : BEST_COMPLEX_WF
    const e = get(idx, task, model, wf)
    if (e?.latency != null) vals.push(e.latency)
  }
  return vals.length ? vals.reduce((a, b) => a + b, 0) / vals.length : null
}

// Average metrics for a given technique across all tasks
// technique: 'bas' | 'cot' | 'fhecoder'
function avgMetricsForTechnique(idx, model, technique) {
  const funcs = [], secs = [], funcSecs = [], f1s = [], lats = []
  for (const task of ALL_TASKS) {
    let e = null
    if (technique === 'bas') {
      e = get(idx, task, model, 'Baseline (B)')
    } else if (technique === 'cot') {
      e = get(idx, task, model, 'Zero-shot COT')
    } else {
      const wf = SIMPLE_TASKS.includes(task) ? BEST_SIMPLE_WF : BEST_COMPLEX_WF
      e = get(idx, task, model, wf)
    }
    if (e?.func != null) funcs.push(e.func)
    if (e?.sec != null) secs.push(e.sec)
    if (e?.funcSec != null) funcSecs.push(e.funcSec)
    if (e?.f1 != null) f1s.push(e.f1)
    if (e?.latency != null) lats.push(e.latency)
  }
  const avg = arr => arr.length ? arr.reduce((a, b) => a + b, 0) / arr.length : 0
  return {
    func: avg(funcs),
    sec: avg(secs),
    funcSec: avg(funcSecs),
    f1: avg(f1s),
    latency: lats.length ? avg(lats) : null,
  }
}

// Leaderboard for a specific technique: 'bas' | 'cot' | 'fhecoder'
function computeLeaderboardForTechnique(idx, technique) {
  const rows = MODELS.map(model => {
    const tasks = {}
    for (const task of ALL_TASKS) {
      let entry = null
      if (technique === 'bas') {
        entry = get(idx, task, model, 'Baseline (B)')
      } else if (technique === 'cot') {
        entry = get(idx, task, model, 'Zero-shot COT')
      } else {
        const r = bestScore(idx, task, model)
        tasks[task] = r ?? null
        continue
      }
      tasks[task] = entry?.funcSec != null ? { score: entry.funcSec, std: entry.funcSecStd } : null
    }
    const valid = Object.values(tasks).filter(Boolean).map(r => r.score)
    const avg = valid.length ? valid.reduce((a, b) => a + b, 0) / valid.length : 0
    return { model, label: MODEL_LABEL[model], tasks, avg }
  })
  return rows.sort((a, b) => b.avg - a.avg).map((r, i) => ({ ...r, rank: i + 1 }))
}

// Average best-technique func (not func_sec) per model across all tasks
function avgBestFunc(idx, model) {
  const vals = []
  for (const task of ALL_TASKS) {
    const wf = SIMPLE_TASKS.includes(task) ? BEST_SIMPLE_WF : BEST_COMPLEX_WF
    const e = get(idx, task, model, wf)
    if (e?.func != null) vals.push(e.func)
  }
  return vals.length ? vals.reduce((a, b) => a + b, 0) / vals.length : 0
}

function computeLeaderboard(idx) {
  const rows = MODELS.map(model => {
    const tasks = {}
    for (const task of ALL_TASKS) {
      tasks[task] = bestScore(idx, task, model)
    }
    const valid = Object.values(tasks).filter(Boolean).map(r => r.score)
    const avg = valid.length ? valid.reduce((a, b) => a + b, 0) / valid.length : 0
    return { model, label: MODEL_LABEL[model], tasks, avg }
  })
  return rows.sort((a, b) => b.avg - a.avg).map((r, i) => ({ ...r, rank: i + 1 }))
}

// ─── Color helpers ────────────────────────────────────────────────────────────

function scoreColor(v) {
  if (v == null) return '#0d1424'
  if (v >= 0.9) return '#14532d'
  if (v >= 0.7) return '#166534'
  if (v >= 0.5) return '#3a6612'
  if (v >= 0.3) return '#854d0e'
  if (v >= 0.1) return '#7c2d12'
  return '#3b0707'
}

function scoreFg(v) {
  return v == null ? '#334155' : '#e8f5e9'
}

// ─── Leaderboard ──────────────────────────────────────────────────────────────

const TECHNIQUE_OPTIONS = [
  { key: 'bas', label: 'Baseline', desc: 'Baseline' },
  { key: 'cot', label: 'Zero-Shot Chain of Thought', desc: 'Zero-shot COT' },
  { key: 'fhecoder', label: 'FHE-Coder', desc: 'Best technique' },
]

const WEIGHT_DEFS = [
  { key: 'wFuncSec', label: 'pass@1 (functionality + security)', color: '#818cf8' },
  { key: 'wFunc', label: 'pass@1 (functionality)', color: '#2dd4bf' },
  { key: 'wSec', label: 'pass@1 (security)', color: '#f472b6' },
  { key: 'wLatency', label: 'Latency ratio (lower=better)', color: '#fbbf24' },
]

function WeightSlider({ label, value, onChange, color }) {
  return (
    <div className="weight-slider">
      <div className="weight-slider-header">
        <span className="weight-label" style={{ color }}>{label}</span>
        <span className="weight-val" style={{ color }}>{value.toFixed(2)}</span>
      </div>
      <input
        type="range"
        min={0}
        max={1}
        step={0.05}
        value={value}
        onChange={e => onChange(parseFloat(e.target.value))}
        className="weight-range"
        style={{
          '--thumb-color': color,
          '--track-color': color,
          '--fill-pct': `${value * 100}%`,
        }}
      />
      <span className="slider-hint">drag to adjust weight</span>
    </div>
  )
}

function FHEExplanationSection() {
  return (
    <section className="fhe-explanation">
      <div className="explanation-container">
        <h2>Why FHE Coding is Hard?</h2>
        <p className="explanation-intro">
          Fully Homomorphic Encryption (FHE) requires computing on encrypted data. Unlike regular programming, FHE code must handle encryption, homomorphic operations, and decryption — without ever exposing the plaintext.
        </p>

        <div className="code-comparison">
          <div className="code-column plaintext">
            <h3>Standard Code</h3>
            <div className="code-block">
              <span className="code-comment">// No encryption overhead</span><br />
              <span className="code-keyword">int</span> result = a & b;
            </div>
            <p className="code-label">Simple & Direct</p>
          </div>

          <div className="code-column tfhe">
            <h3>FHE Code (TFHE)</h3>
            <div className="code-block">
              <div className="annotation-group">
                <span className="annotation-label">🔧 Parameter Setup &amp; Key Generation</span>
                <span className="code-keyword">const int</span> minimum_lambda = <span className="code-number">110</span>;<br />
                TFheGateBootstrappingParameterSet* params =<br />
                &nbsp;&nbsp;new_default_gate_bootstrapping_parameters(minimum_lambda);<br />
                uint32_t seed[] = &#123; <span className="code-number">314, 1592, 657</span> &#125;;<br />
                tfhe_random_generator_setSeed(seed, <span className="code-number">3</span>);<br />
                TFheGateBootstrappingSecretKeySet* key =<br />
                &nbsp;&nbsp;new_random_gate_bootstrapping_secret_keyset(params);
              </div>

              <div className="annotation-group">
                <span className="annotation-label">🔐 Encryption of Input</span>
                <span className="code-keyword">LweSample</span>* ciphertext1 = new_gate_bootstrapping_ciphertext_array(<span className="code-number">32</span>, params);<br />
                <span className="code-keyword">for</span> (<span className="code-keyword">int</span> i=<span className="code-number">0</span>; i &lt; <span className="code-number">32</span>; i++) &#123;<br />
                &nbsp;&nbsp;bootsSymEncrypt(&amp;ciphertext1[i], (plaintext1 &gt;&gt; i) &amp; <span className="code-number">1</span>, key);<br />
                &#125;
              </div>

              <div className="annotation-group">
                <span className="annotation-label">⚙️ Homomorphic Operation</span>
                <span className="code-keyword">LweSample</span>* result = new_gate_bootstrapping_ciphertext_array(<span className="code-number">32</span>, params);<br />
                <span className="code-keyword">for</span> (<span className="code-keyword">int</span> i=<span className="code-number">0</span>; i &lt; <span className="code-number">32</span>; i++) &#123;<br />
                &nbsp;&nbsp;bootsAND(&amp;result[i], &amp;ciphertext1[i], &amp;ciphertext2[i], key-&gt;cloud);<br />
                &#125;
              </div>

              <div className="annotation-group">
                <span className="annotation-label">🔓 Decryption of Output</span>
                <span className="code-keyword">int32_t</span> decrypted_result = <span className="code-number">0</span>;<br />
                <span className="code-keyword">for</span> (<span className="code-keyword">int</span> i=<span className="code-number">0</span>; i &lt; <span className="code-number">32</span>; i++) &#123;<br />
                &nbsp;&nbsp;decrypted_result |= (bootsSymDecrypt(&amp;result[i], key-&gt;lwe_key) &lt;&lt; i);<br />
                &#125;
              </div>
            </div>
            <p className="code-label">Complex & Crypto-Aware</p>
          </div>
        </div>

        <div className="explanation-insight">
          <strong>The Challenge:</strong> LLMs must generate code that correctly uses FHE libraries, manages encrypted values, and produces valid outputs — all while respecting cryptographic constraints.
        </div>
      </div>
    </section>
  )
}

const SORT_OPTIONS = [
  { key: 'f1',      label: 'F1 Score',        color: '#a78bfa' },
  { key: 'funcSec', label: 'Func + Security', color: '#818cf8' },
  { key: 'func',    label: 'Functionality',   color: '#2dd4bf' },
  { key: 'sec',     label: 'Security',        color: '#f472b6' },
  { key: 'latency', label: 'Latency',         color: '#fbbf24' },
]

function LeaderboardSection({ idx }) {
  const [technique, setTechnique] = useState('fhecoder')
  const [sortBy, setSortBy] = useState('funcSec')

  const ranked = useMemo(() => {
    const rows = MODELS.map(model => {
      const m = avgMetricsForTechnique(idx, model, technique)
      return { model, label: MODEL_LABEL[model], ...m }
    })

    return [...rows].sort((a, b) => {
      if (sortBy === 'latency') {
        const av = a.latency ?? Infinity
        const bv = b.latency ?? Infinity
        return av - bv  // lower latency is better
      }
      return (b[sortBy] ?? 0) - (a[sortBy] ?? 0)
    }).map((r, i) => ({ ...r, rank: i + 1 }))
  }, [idx, technique, sortBy])

  const activeSortColor = SORT_OPTIONS.find(o => o.key === sortBy)?.color ?? '#818cf8'

  return (
    <section className="card">
      <div className="lb-header">
        <div className="lb-title-row">
          <div>
            <h2>Leaderboard</h2>
            <p className="subtitle">Averaged across 10 FHE tasks.</p>
          </div>
        </div>
        <div className="lb-controls-row">
          <div className="lb-sort-row">
            <span className="technique-label">Sort by:</span>
            <div className="radio-group">
              {SORT_OPTIONS.map(opt => (
                <button
                  key={opt.key}
                  className={`radio-btn ${sortBy === opt.key ? 'active' : ''}`}
                  onClick={() => setSortBy(opt.key)}
                  style={sortBy === opt.key ? { borderColor: opt.color, color: opt.color } : {}}
                >
                  {opt.label}
                </button>
              ))}
            </div>
          </div>
          <div className="lb-sort-row">
            <span className="technique-label">Agent Type:</span>
            <div className="radio-group">
              {TECHNIQUE_OPTIONS.map(opt => (
                <button
                  key={opt.key}
                  className={`radio-btn ${technique === opt.key ? 'active' : ''}`}
                  onClick={() => setTechnique(opt.key)}
                  title={opt.desc}
                >
                  {opt.label}
                </button>
              ))}
            </div>
          </div>
        </div>
      </div>

      <div className="composite-table-wrap">
        <table className="composite-table">
          <thead>
            <tr>
              <th>#</th>
              <th style={{ textAlign: 'left' }}>Model</th>
              <th style={{ color: activeSortColor }}>
                {SORT_OPTIONS.find(o => o.key === sortBy)?.label}
                {sortBy === 'latency' ? ' ↑ lower is better' : ' ↓ higher is better'}
              </th>
              <th style={{ color: '#a78bfa' }}>F1 Score</th>
              <th>Func + Security</th>
              <th>Functionality</th>
              <th>Security</th>
              <th>Latency Ratio</th>
            </tr>
          </thead>
          <tbody>
            {ranked.map(row => {
              const sortVal = sortBy === 'latency'
                ? (row.latency != null ? row.latency : null)
                : (row[sortBy] ?? null)
              const barPct = sortBy === 'latency'
                ? null  // don't show bar for latency
                : (sortVal != null ? sortVal * 100 : 0)

              return (
                <tr key={row.model}>
                  <td className="rank-td">
                    <span className={`rank-badge rank-${Math.min(row.rank, 4)}`}>{row.rank}</span>
                  </td>
                  <td className="model-td" style={{ color: MODEL_COLOR[row.model] }}>
                    {row.label}
                  </td>
                  <td className="comp-score-td">
                    {barPct != null ? (
                      <div className="comp-bar-wrap">
                        <div className="comp-bar" style={{ width: `${barPct}%`, background: MODEL_COLOR[row.model] }} />
                        <span className="comp-bar-label">{sortVal.toFixed(2)}</span>
                      </div>
                    ) : (
                      <span className="comp-metric-td" style={{ color: '#fbbf24' }}>
                        {row.latency != null ? `${row.latency.toFixed(2)}×` : '–'}
                      </span>
                    )}
                  </td>
                  <td className="comp-metric-td" style={{ color: '#a78bfa' }}>{row.f1.toFixed(2)}</td>
                  <td className="comp-metric-td" style={{ color: '#818cf8' }}>{row.funcSec.toFixed(2)}</td>
                  <td className="comp-metric-td" style={{ color: '#2dd4bf' }}>{row.func.toFixed(2)}</td>
                  <td className="comp-metric-td" style={{ color: '#f472b6' }}>{row.sec.toFixed(2)}</td>
                  <td className="comp-metric-td" style={{ color: '#fbbf24' }}>
                    {row.latency != null ? `${row.latency.toFixed(2)}×` : '–'}
                  </td>
                </tr>
              )
            })}
          </tbody>
        </table>
      </div>

      <div className="model-color-legend">
        {MODELS.map(m => (
          <div key={m} className="mcl-item">
            <div className="mcl-dot" style={{ background: MODEL_COLOR[m] }} />
            {MODEL_LABEL[m]}
          </div>
        ))}
      </div>
    </section>
  )
}

// ─── Security Illusion ────────────────────────────────────────────────────────

function SecurityIllusionSection({ idx }) {
  const [reveal, setReveal] = useState(false)

  const chartData = useMemo(() => {
    return MODELS.map(model => {
      const blFunc = avgField(idx, model, 'Baseline (B)', 'func')
      const blFuncSec = avgField(idx, model, 'Baseline (B)', 'funcSec')
      const cotFunc = avgField(idx, model, 'Zero-shot COT', 'func')
      const cotFuncSec = avgField(idx, model, 'Zero-shot COT', 'funcSec')
      const fhFuncSec = avgBestFuncSec(idx, model)
      const entry = {
        name: MODEL_LABEL[model],
        model,
        Baseline: reveal ? (blFuncSec ?? 0) : (blFunc ?? 0),
        'Zero-Shot Chain of Thought': reveal ? (cotFuncSec ?? 0) : (cotFunc ?? 0),
      }
      if (reveal) entry['FHE-Coder'] = fhFuncSec
      return entry
    })
  }, [idx, reveal])

  const CustomTooltip = ({ active, payload, label }) => {
    if (!active || !payload?.length) return null
    return (
      <div className="chart-tooltip">
        <p className="tooltip-label">{label}</p>
        {payload.map(p => (
          <div key={p.dataKey} className="tooltip-row">
            <span className="tooltip-key" style={{ color: p.color }}>{p.dataKey}</span>
            <span className="tooltip-val" style={{ color: p.color }}>
              {(p.value * 100).toFixed(1)}%
            </span>
          </div>
        ))}
        <div style={{ marginTop: '0.4rem', fontSize: '0.72rem', color: '#475569' }}>
          Metric: {reveal ? 'pass@1 (functionality + security)' : 'pass@1 (functionality)'}
        </div>
      </div>
    )
  }

  return (
    <section className="card">
      <div className="section-header">
        <div>
          <h2>The Security Illusion</h2>
          <p className="subtitle">
            Baseline and Zero-Shot Chain of Thought look fine on functional tests — toggle to add security evaluation and reveal FHE-Coder's advantage.
          </p>
        </div>
        <div className="toggle-control">
          <span className={`toggle-text ${!reveal ? 'active' : ''}`}>Functional Only</span>
          <button
            className={`t-btn ${reveal ? 'on' : 'off'}`}
            onClick={() => setReveal(r => !r)}
            aria-pressed={reveal}
            aria-label="Reveal FHE-Coder impact"
          >
            <span className="t-thumb" />
          </button>
          <span className={`toggle-text ${reveal ? 'active' : ''}`}>+ FHE-Coder</span>
        </div>
      </div>

      {reveal && (
        <div className="reveal-banner">
          ⚠ Baseline and Zero-Shot Chain of Thought collapse under cryptographic evaluation — FHE-Coder maintains high pass rates.
        </div>
      )}

      <div className="chart-container">
        <ResponsiveContainer width="100%" height={310}>
          <BarChart
            data={chartData}
            margin={{ top: 5, right: 20, left: 0, bottom: 5 }}
            barCategoryGap="25%"
            barGap={4}
          >
            <CartesianGrid strokeDasharray="3 3" stroke="#1e2a3a" vertical={false} />
            <XAxis
              dataKey="name"
              tick={{ fill: '#64748b', fontSize: 12 }}
              axisLine={{ stroke: '#1e2a3a' }}
              tickLine={false}
            />
            <YAxis
              domain={[0, 1]}
              tickFormatter={v => `${(v * 100).toFixed(0)}%`}
              tick={{ fill: '#475569', fontSize: 11 }}
              axisLine={false}
              tickLine={false}
              width={42}
            />
            <Tooltip content={<CustomTooltip />} cursor={{ fill: 'rgba(255,255,255,0.03)' }} />
            <Legend
              wrapperStyle={{ color: '#64748b', paddingTop: 14, fontSize: 13 }}
            />
            <Bar dataKey="Baseline" fill="#334155" radius={[4, 4, 0, 0]} maxBarSize={48} />
            <Bar dataKey="Zero-Shot Chain of Thought" fill="#1e3a5f" radius={[4, 4, 0, 0]} maxBarSize={48} />
            {reveal && (
              <Bar dataKey="FHE-Coder" radius={[4, 4, 0, 0]} maxBarSize={48}>
                {chartData.map(entry => (
                  <Cell key={entry.model} fill={MODEL_COLOR[entry.model]} />
                ))}
              </Bar>
            )}
          </BarChart>
        </ResponsiveContainer>
      </div>
    </section>
  )
}

// ─── Ablation Station ────────────────────────────────────────────────────────

function CompToggle({ label, checked, onChange, color }) {
  return (
    <div
      className={`comp-toggle ${checked ? 'checked' : ''}`}
      onClick={() => onChange(!checked)}
      style={checked ? { borderColor: color + '60' } : {}}
    >
      <span className="comp-toggle-label">{label}</span>
      <button
        className={`mini-toggle ${checked ? 'on' : 'off'}`}
        style={checked ? { background: color + '30' } : {}}
        aria-pressed={checked}
      >
        <span
          className="mini-thumb"
          style={checked ? { background: color } : {}}
        />
      </button>
    </div>
  )
}

function AblationSection({ idx }) {
  const [selectedTask, setSelectedTask] = useState('task_vector_addition')
  const [selectedModel, setSelectedModel] = useState('GPT-5')
  const [formal, setFormal] = useState(false)
  const [rag, setRag] = useState(false)
  const [sec, setSec] = useState(false)
  const [humanGuide, setHumanGuide] = useState(false)

  const isComplex = COMPLEX_TASKS.includes(selectedTask)
  const steps = isComplex ? ABLATION_STEPS_COMPLEX : ABLATION_STEPS_SIMPLE

  // Map toggle state → active step key
  const activeKey = useMemo(() => {
    if (isComplex && formal && rag && sec && humanGuide) return 'fhecoder_hg'
    if (formal && rag && sec) return 'fhecoder'
    if (formal && rag) return 'formal_rag'
    if (formal) return 'formal'
    return 'baseline'
  }, [formal, rag, sec, humanGuide, isComplex])

  const chartData = useMemo(() => {
    return steps.map(step => {
      const e = get(idx, selectedTask, selectedModel, step.workflow)
      return {
        name: step.label,
        key: step.key,
        funcSec: e?.funcSec ?? 0,
        func: e?.func ?? 0,
        sec: e?.sec ?? 0,
        isActive: step.key === activeKey,
      }
    })
  }, [idx, selectedTask, selectedModel, steps, activeKey])

  const CustomTooltip = ({ active, payload, label }) => {
    if (!active || !payload?.length) return null
    const d = payload[0]?.payload
    return (
      <div className="chart-tooltip">
        <p className="tooltip-label">{label}</p>
        <div className="tooltip-row">
          <span className="tooltip-key" style={{ color: '#818cf8' }}>func+sec</span>
          <span className="tooltip-val" style={{ color: '#818cf8' }}>
            {(d.funcSec * 100).toFixed(1)}%
          </span>
        </div>
        <div className="tooltip-row">
          <span className="tooltip-key" style={{ color: '#2dd4bf' }}>func</span>
          <span className="tooltip-val" style={{ color: '#2dd4bf' }}>
            {(d.func * 100).toFixed(1)}%
          </span>
        </div>
        <div className="tooltip-row">
          <span className="tooltip-key" style={{ color: '#f472b6' }}>security</span>
          <span className="tooltip-val" style={{ color: '#f472b6' }}>
            {(d.sec * 100).toFixed(1)}%
          </span>
        </div>
      </div>
    )
  }

  const activeStep = steps.find(s => s.key === activeKey)

  return (
    <section className="card">
      <div className="section-header">
        <div>
          <h2>Ablation Station</h2>
          <p className="subtitle">
            Toggle framework components to see their cumulative impact on pass@1(func+sec).
          </p>
        </div>
      </div>

      <div className="ablation-controls">
        <div className="select-group">
          <label>Task</label>
          <select value={selectedTask} onChange={e => setSelectedTask(e.target.value)}>
            <optgroup label="Simple Tasks">
              {SIMPLE_TASKS.map(t => (
                <option key={t} value={t}>{TASK_LABEL[t]}</option>
              ))}
            </optgroup>
            <optgroup label="Complex Tasks">
              {COMPLEX_TASKS.map(t => (
                <option key={t} value={t}>{TASK_LABEL[t]}</option>
              ))}
            </optgroup>
          </select>
        </div>
        <div className="select-group">
          <label>Model</label>
          <select value={selectedModel} onChange={e => setSelectedModel(e.target.value)}>
            {MODELS.map(m => (
              <option key={m} value={m}>{MODEL_LABEL[m]}</option>
            ))}
          </select>
        </div>
      </div>

      <div className="component-toggles">
        <CompToggle
          label="Formal Prompt"
          checked={formal}
          onChange={setFormal}
          color="#818cf8"
        />
        <CompToggle
          label="RAG Retriever"
          checked={rag}
          onChange={setRag}
          color="#2dd4bf"
        />
        <CompToggle
          label="Security Verifier"
          checked={sec}
          onChange={setSec}
          color="#f472b6"
        />
        {isComplex && (
          <CompToggle
            label="Human Guidance"
            checked={humanGuide}
            onChange={setHumanGuide}
            color="#fbbf24"
          />
        )}
      </div>

      <div className="chart-container">
        <ResponsiveContainer width="100%" height={290}>
          <BarChart
            data={chartData}
            margin={{ top: 5, right: 20, left: 0, bottom: 5 }}
            barCategoryGap="35%"
          >
            <CartesianGrid strokeDasharray="3 3" stroke="#1e2a3a" vertical={false} />
            <XAxis
              dataKey="name"
              tick={{ fill: '#64748b', fontSize: 12 }}
              axisLine={{ stroke: '#1e2a3a' }}
              tickLine={false}
            />
            <YAxis
              domain={[0, 1]}
              tickFormatter={v => `${(v * 100).toFixed(0)}%`}
              tick={{ fill: '#475569', fontSize: 11 }}
              axisLine={false}
              tickLine={false}
              width={42}
            />
            <Tooltip content={<CustomTooltip />} cursor={{ fill: 'rgba(255,255,255,0.03)' }} />
            <Bar dataKey="funcSec" name="pass@1(func+sec)" radius={[4, 4, 0, 0]} maxBarSize={60}>
              {chartData.map(entry => (
                <Cell
                  key={entry.key}
                  fill={entry.isActive ? MODEL_COLOR[selectedModel] : '#1e2a3a'}
                  fillOpacity={entry.isActive ? 1 : 0.6}
                />
              ))}
            </Bar>
          </BarChart>
        </ResponsiveContainer>
      </div>

      <p className="ablation-step-info">
        Active configuration:{' '}
        <strong style={{ color: MODEL_COLOR[selectedModel] }}>{activeStep?.label}</strong>
      </p>
    </section>
  )
}

// ─── Heatmap ─────────────────────────────────────────────────────────────────

function HeatmapSection({ leaderboard }) {
  return (
    <section className="card">
      <div className="section-header">
        <div>
          <h2>Complexity × Model Heatmap</h2>
          <p className="subtitle">
            pass@1(func+sec) for best technique — hover cells for details
          </p>
        </div>
      </div>

      <div className="heatmap-wrap">
        <table className="heatmap-table">
          <thead>
            <tr>
              <th className="hm-model-th">Model</th>
              <th colSpan={SIMPLE_TASKS.length} className="hm-group simple-group">
                Simple Tasks
              </th>
              <th className="hm-spacer" />
              <th colSpan={COMPLEX_TASKS.length} className="hm-group complex-group">
                Complex Tasks
              </th>
            </tr>
            <tr>
              <th className="hm-model-th" />
              {SIMPLE_TASKS.map(t => (
                <th key={t} className="hm-task-th">{TASK_LABEL[t]}</th>
              ))}
              <th className="hm-spacer" />
              {COMPLEX_TASKS.map(t => (
                <th key={t} className="hm-task-th">{TASK_LABEL[t]}</th>
              ))}
            </tr>
          </thead>
          <tbody>
            {leaderboard.map(row => (
              <tr key={row.model}>
                <td className="hm-model-td" style={{ color: MODEL_COLOR[row.model] }}>
                  {row.label}
                </td>
                {SIMPLE_TASKS.map(t => {
                  const r = row.tasks[t]
                  const v = r?.score ?? null
                  return (
                    <td
                      key={t}
                      className="hm-cell"
                      style={{ background: scoreColor(v), color: scoreFg(v) }}
                      title={`${MODEL_LABEL[row.model]} × ${TASK_LABEL[t]}: ${v != null ? (v * 100).toFixed(0) + '%' : 'N/A'}`}
                    >
                      <span className="hm-val">{v != null ? v.toFixed(2) : '–'}</span>
                    </td>
                  )
                })}
                <td className="hm-spacer" />
                {COMPLEX_TASKS.map(t => {
                  const r = row.tasks[t]
                  const v = r?.score ?? null
                  return (
                    <td
                      key={t}
                      className="hm-cell"
                      style={{ background: scoreColor(v), color: scoreFg(v) }}
                      title={`${MODEL_LABEL[row.model]} × ${TASK_LABEL[t]}: ${v != null ? (v * 100).toFixed(0) + '%' : 'N/A'}`}
                    >
                      <span className="hm-val">{v != null ? v.toFixed(2) : '–'}</span>
                    </td>
                  )
                })}
              </tr>
            ))}
          </tbody>
        </table>
      </div>

      <div className="heatmap-legend">
        <span>0%</span>
        <div className="legend-gradient" />
        <span>100%</span>
      </div>
    </section>
  )
}



// ─── Metrics Guide ───────────────────────────────────────────────────────────

const METRICS_INFO = [
  {
    name: 'F1 Score',
    short: 'f1',
    color: '#a78bfa',
    desc: 'Harmonic mean of functionality and security pass rates: 2 × (func × sec) / (func + sec). Rewards balanced performance — a model scoring 0 on either metric gets F1 = 0. Penalizes models that are strong on one axis but weak on the other.',
  },
  {
    name: 'pass@1 (functionality)',
    short: 'functionality',
    color: '#2dd4bf',
    desc: 'Fraction of tasks where the generated code produces correct outputs on all functional test cases. Measures whether the FHE circuit computes the right answer.',
  },
  {
    name: 'pass@1 (security)',
    short: 'security',
    color: '#f472b6',
    desc: 'Fraction of tasks where the generated code uses cryptographically valid FHE parameters — correct scheme selection, sufficient noise budget, and proper key configuration.',
  },
  {
    name: 'pass@1 (functionality + security)',
    short: 'functionality + security',
    color: '#818cf8',
    desc: 'Both functional correctness AND cryptographic security must hold simultaneously. Equivalent to min(func, sec) — a solution that works but is insecure scores 0.',
  },
  {
    name: 'Latency Ratio',
    short: 'latency',
    color: '#fbbf24',
    desc: 'Runtime of the generated FHE code relative to an expert-written reference solution. A value of 1.0× means on par with the expert; higher values indicate slower code. Lower is better.',
  },
]

function MetricsGuideSection() {
  return (
    <section className="card">
      <div className="section-header">
        <div>
          <h2>Metric Definitions</h2>
          <p className="subtitle">
            What each evaluation metric measures and why it matters for FHE code generation.
          </p>
        </div>
      </div>
      <div className="guide-grid">
        {METRICS_INFO.map(m => (
          <div key={m.short} className="guide-card" style={{ borderColor: m.color + '40' }}>
            <div className="guide-card-header">
              <span className="guide-badge" style={{ background: m.color + '20', color: m.color }}>
                {m.short}
              </span>
              <span className="guide-name">{m.name}</span>
            </div>
            <p className="guide-desc">{m.desc}</p>
          </div>
        ))}
      </div>
    </section>
  )
}

// ─── Tasks Guide ─────────────────────────────────────────────────────────────

const TASKS_INFO = {
  simple: [
    { task: 'task_adder', desc: 'Add two encrypted integers using FHE arithmetic circuits.' },
    { task: 'task_and', desc: 'Compute bitwise AND on encrypted bits via Boolean FHE gates.' },
    { task: 'task_relu', desc: 'Apply ReLU activation (max(0, x)) to an encrypted value using polynomial approximation.' },
    { task: 'task_multiplier', desc: 'Multiply two encrypted integers; tests ciphertext multiplication and noise management.' },
    { task: 'task_dot_product', desc: 'Compute the dot product of two encrypted vectors using SIMD batching.' },
    { task: 'task_vector_addition', desc: 'Element-wise addition of two encrypted vectors.' },
  ],
  complex: [
    { task: 'task_cnn', desc: 'Apply a 2D convolution operation over an encrypted input sequence.' },
    { task: 'task_matrix_matrix', desc: 'Multiply two encrypted matrices; requires correct rotation and accumulation patterns.' },
    { task: 'task_matrix_vector', desc: 'Multiply an encrypted matrix by an encrypted vector using diagonal encoding.' },
    { task: 'task_mlp', desc: 'Run inference of a multi-layer perceptron over encrypted inputs end-to-end.' },
  ],
}

function TasksGuideSection() {
  return (
    <section className="card">
      <div className="section-header">
        <div>
          <h2>Task Descriptions</h2>
          <p className="subtitle">
            Brief overview of the 10 FHE programming tasks in the benchmark.
          </p>
        </div>
      </div>

      <div className="tasks-group-label simple-label">Simple Tasks</div>
      <div className="guide-grid">
        {TASKS_INFO.simple.map(({ task, desc }) => (
          <div key={task} className="guide-card task-card">
            <div className="guide-card-header">
              <span className="guide-badge task-badge">{TASK_LABEL[task]}</span>
            </div>
            <p className="guide-desc">{desc}</p>
          </div>
        ))}
      </div>

      <div className="tasks-group-label complex-label" style={{ marginTop: '1.5rem' }}>Complex Tasks</div>
      <div className="guide-grid">
        {TASKS_INFO.complex.map(({ task, desc }) => (
          <div key={task} className="guide-card task-card">
            <div className="guide-card-header">
              <span className="guide-badge task-badge">{TASK_LABEL[task]}</span>
            </div>
            <p className="guide-desc">{desc}</p>
          </div>
        ))}
      </div>
    </section>
  )
}

// ─── App ─────────────────────────────────────────────────────────────────────

export default function App() {
  const [theme, setTheme] = useState('dark')

  const { idx, leaderboard } = useMemo(() => {
    const rows = parseCSV(csvText)
    const idx = buildIndex(rows)
    const leaderboard = computeLeaderboard(idx)
    return { idx, leaderboard }
  }, [])

  const toggleTheme = () => {
    setTheme(theme === 'dark' ? 'light' : 'dark')
  }

  return (
    <div className={`app-${theme}`}>
      <header className="hero">
        <button className="theme-toggle" onClick={toggleTheme} title="Toggle theme">
          {theme === 'dark' ? '☀️' : '🌙'}
        </button>
        <div className="hero-content">
          {/* <div className="hero-badge">Research Benchmark</div> */}
          <h1>FHE-Coder Leaderboard</h1>
          <p className="hero-desc">
            Benchmarking LLM-driven Fully Homomorphic Encryption code generation across
            functional correctness and cryptographic security constraints.
          </p>
          <div className="hero-stats">
            <div className="stat">
              <span className="stat-num">{MODELS.length}</span>
              <span className="stat-label">Models</span>
            </div>
            <div className="stat">
              <span className="stat-num">10</span>
              <span className="stat-label">FHE Tasks</span>
            </div>
            <div className="stat">
              <span className="stat-num">10</span>
              <span className="stat-label">Workflows</span>
            </div>
          </div>
          <div className="hero-actions">
            <a
              href="https://openreview.net/forum?id=4F1py5vQXm"
              target="_blank"
              rel="noopener noreferrer"
              className="gh-btn"
            >
              <svg className="gh-icon" viewBox="0 0 24 24" fill="currentColor" aria-hidden="true">
                <path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8l-6-6zm-1 1.5L18.5 9H13V3.5zM6 20V4h5v7h7v9H6z" />
              </svg>
              Paper
            </a>
            <a
              href="https://github.com/mayank64ce/fhe-agentic-benchmarking"
              target="_blank"
              rel="noopener noreferrer"
              className="gh-btn"
            >
              <svg className="gh-icon" viewBox="0 0 24 24" fill="currentColor" aria-hidden="true">
                <path d="M12 2C6.477 2 2 6.484 2 12.017c0 4.425 2.865 8.18 6.839 9.504.5.092.682-.217.682-.483 0-.237-.008-.868-.013-1.703-2.782.605-3.369-1.343-3.369-1.343-.454-1.158-1.11-1.466-1.11-1.466-.908-.62.069-.608.069-.608 1.003.07 1.531 1.032 1.531 1.032.892 1.53 2.341 1.088 2.91.832.092-.647.35-1.088.636-1.338-2.22-.253-4.555-1.113-4.555-4.951 0-1.093.39-1.988 1.029-2.688-.103-.253-.446-1.272.098-2.65 0 0 .84-.27 2.75 1.026A9.564 9.564 0 0112 6.844c.85.004 1.705.115 2.504.337 1.909-1.296 2.747-1.027 2.747-1.027.546 1.379.202 2.398.1 2.651.64.7 1.028 1.595 1.028 2.688 0 3.848-2.339 4.695-4.566 4.943.359.309.678.92.678 1.855 0 1.338-.012 2.419-.012 2.747 0 .268.18.58.688.482A10.019 10.019 0 0022 12.017C22 6.484 17.522 2 12 2z" />
              </svg>
              Source Code
            </a>
          </div>
        </div>
      </header>

      <main className="main-content">
        <FHEExplanationSection />
        <LeaderboardSection idx={idx} />
        <SecurityIllusionSection idx={idx} />
        <MetricsGuideSection />
        <TasksGuideSection />
      </main>

      <footer className="footer">
        FHE-Coder Benchmark · Interactive Dashboard · Data from experimental runs
      </footer>
    </div>
  )
}
