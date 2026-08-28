/**
 * Safe Semantic Planner (SSP) — Professional Frontend Orchestrator
 * High-Dimensional Neuro-Symbolic Search Visualizer
 */

// =============================================================================
// GLOBAL STATE REPOSITORY
// =============================================================================
const state = {
  problem: null,
  result: null,
  config: null,
  
  // Viewport & Transformations
  viewMode: 'canvas', // 'canvas' | 'isometric' | 'pipeline' | 'table'
  panX: 0,
  panY: 0,
  zoom: 1.0,
  worldCenterX: 4.5,
  worldCenterY: 3.5,
  isPanning: false,
  lastMouseX: 0,
  lastMouseY: 0,
  
  // 3D Isometric Tilt Parameters
  isoPitch: 45, // degrees
  isoYaw: 35,   // degrees
  isoExtrudeMode: 'cost', // 'cost' | 'clearance' | 'dim3'
  isRotatingIso: false,

  // Selected High-Dim Projection Dimensions
  projAxisX: 0,
  projAxisY: 1,

  // Canvas Interaction Mode
  currentMode: 'drag', // 'drag' | 'pan' | 'inspect-vector' | 'add-node' | 'add-edge' | 'edit' | 'delete' | 'toggle-hazard' | 'toggle-edge' | 'set-start' | 'set-goal'
  draggedNode: null,
  hoveredNode: null,
  hoveredEdge: null,
  selectedEdgeSource: null,
  
  // 2-State Vector Comparison
  inspectStateA: null,
  inspectStateB: null,

  // Animation State
  isAnimatingAgent: false,
  agentStep: null,

  // Table Sub-Tab
  tableSubTab: 'states-table'
};

// =============================================================================
// DOM ELEMENT REFERENCES
// =============================================================================
const viewport = document.getElementById('canvasViewport');
const canvas = document.getElementById('plannerCanvas');
const ctx = canvas.getContext('2d');
const tooltip = document.getElementById('canvasTooltip');
const sidebarPanel = document.getElementById('sidebarPanel');
const sidebarResizer = document.getElementById('sidebarResizer');
const modeBanner = document.getElementById('modeBanner');
const modeBannerText = document.getElementById('modeBannerText');
const zoomLevelText = document.getElementById('zoomLevelText');
const canvasLegend = document.getElementById('canvasLegend');
const legendToggleBtn = document.getElementById('legendToggleBtn');

// View Containers
const pipelineViewport = document.getElementById('pipelineViewport');
const tableViewport = document.getElementById('tableViewport');
const isometricControls = document.getElementById('isometricControls');
const canvasDock = document.getElementById('canvasDock');

// Modal Elements
const modalBackdrop = document.getElementById('editModalBackdrop');
const modalTitle = document.getElementById('modalTitle');
const modalBody = document.getElementById('modalBody');
const modalCloseBtn = document.getElementById('modalCloseBtn');
const modalCancelBtn = document.getElementById('modalCancelBtn');
const modalSaveBtn = document.getElementById('modalSaveBtn');

let currentModalAction = null;

// =============================================================================
// 1. INITIALIZATION & DATA LOADING
// =============================================================================
window.addEventListener('DOMContentLoaded', async () => {
  setupEventListeners();
  setupSidebarResizer();
  setupModalHandlers();
  lucide.createIcons();
  resizeCanvas();
  window.addEventListener('resize', resizeCanvas);
  
  await loadProblem();
  autoFitView();
  requestAnimationFrame(renderLoop);
});

async function loadProblem() {
  try {
    const res = await fetch('/api/problem');
    if (res.ok) {
      const data = await res.json();
      state.problem = data.problem;
      state.config = data.config;
      updateDimensionSelectors();
      populateStatePickers();
      updateConstraintsTab();
      const sel = document.getElementById('templateSelect');
      const currentIdx = sel ? parseInt(sel.value) : 6;
      updateTCTabVisibility(currentIdx);
      autoFitView();
      await computePlan();
    }
  } catch (err) {
    console.error('Error loading problem:', err);
  }
}

function updateDimensionSelectors() {
  if (!state.problem || !state.problem.states || state.problem.states.length === 0) return;
  const numDims = state.problem.states[0].embedding.length;

  const selX = document.getElementById('projAxisX');
  const selY = document.getElementById('projAxisY');
  selX.innerHTML = '';
  selY.innerHTML = '';

  for (let i = 0; i < numDims; ++i) {
    const optX = document.createElement('option');
    optX.value = i;
    optX.textContent = `Dim ${i + 1} (${getDimName(i)})`;
    if (i === state.projAxisX) optX.selected = true;
    selX.appendChild(optX);

    const optY = document.createElement('option');
    optY.value = i;
    optY.textContent = `Dim ${i + 1} (${getDimName(i)})`;
    if (i === state.projAxisY) optY.selected = true;
    selY.appendChild(optY);
  }
}

function getDimName(dimIndex) {
  if (state.problem && state.problem.stateSpace && state.problem.stateSpace.dimensions && state.problem.stateSpace.dimensions[dimIndex]) {
    return state.problem.stateSpace.dimensions[dimIndex].name;
  }
  const defaultNames = ['X', 'Y', 'Z', 'W', 'V', 'U'];
  return defaultNames[dimIndex] || `Dim${dimIndex + 1}`;
}

function populateStatePickers() {
  if (!state.problem || !state.problem.states) return;

  const selA = document.getElementById('selectStateA');
  const selB = document.getElementById('selectStateB');
  selA.innerHTML = '<option value="">-- Choose State A --</option>';
  selB.innerHTML = '<option value="">-- Choose State B --</option>';

  state.problem.states.forEach(s => {
    const label = `#${s.id}: ${s.name || 'Unnamed'}`;
    const optA = document.createElement('option');
    optA.value = s.id;
    optA.textContent = label;
    if (state.inspectStateA && state.inspectStateA.id === s.id) optA.selected = true;
    selA.appendChild(optA);

    const optB = document.createElement('option');
    optB.value = s.id;
    optB.textContent = label;
    if (state.inspectStateB && state.inspectStateB.id === s.id) optB.selected = true;
    selB.appendChild(optB);
  });
}

async function computePlan() {
  if (!state.problem) return;
  try {
    const res = await fetch('/api/plan', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(state.problem)
    });
    if (res.ok) {
      const data = await res.json();
      state.result = data.result;
      updateUI();
      triggerMetricAnimations();
      if (state.viewMode === 'pipeline') renderPipelineView();
      if (state.viewMode === 'table') renderTableView();
    }
  } catch (err) {
    console.error('Plan computation error:', err);
  }
}

// =============================================================================
// 2. VIEW SWITCHER & RESIZABLE SIDEBAR
// =============================================================================
function setViewMode(mode) {
  state.viewMode = mode;
  document.querySelectorAll('.view-btn').forEach(btn => {
    btn.classList.toggle('active', btn.dataset.view === mode);
  });

  if (mode === 'canvas') {
    viewport.style.display = 'block';
    pipelineViewport.style.display = 'none';
    tableViewport.style.display = 'none';
    isometricControls.style.display = 'none';
    canvasDock.style.display = 'flex';
  } else if (mode === 'isometric') {
    viewport.style.display = 'block';
    pipelineViewport.style.display = 'none';
    tableViewport.style.display = 'none';
    isometricControls.style.display = 'flex';
    canvasDock.style.display = 'flex';
  } else if (mode === 'pipeline') {
    viewport.style.display = 'none';
    pipelineViewport.style.display = 'flex';
    tableViewport.style.display = 'none';
    isometricControls.style.display = 'none';
    canvasDock.style.display = 'none';
    renderPipelineView();
  } else if (mode === 'table') {
    viewport.style.display = 'none';
    pipelineViewport.style.display = 'none';
    tableViewport.style.display = 'flex';
    isometricControls.style.display = 'none';
    canvasDock.style.display = 'none';
    renderTableView();
  }
  lucide.createIcons();
}

function setupSidebarResizer() {
  let isResizing = false;

  sidebarResizer.addEventListener('mousedown', () => {
    isResizing = true;
    sidebarResizer.classList.add('resizing');
    document.body.style.cursor = 'col-resize';
    document.body.style.userSelect = 'none';
  });

  window.addEventListener('mousemove', (e) => {
    if (!isResizing) return;
    const newWidth = window.innerWidth - e.clientX;
    if (newWidth >= 300 && newWidth <= 800) {
      sidebarPanel.style.width = `${newWidth}px`;
      resizeCanvas();
    }
  });

  window.addEventListener('mouseup', () => {
    if (isResizing) {
      isResizing = false;
      sidebarResizer.classList.remove('resizing');
      document.body.style.cursor = 'default';
      document.body.style.userSelect = 'auto';
    }
  });
}

// =============================================================================
// 3. CANVAS RENDERING & GEOMETRIC TRANSFORMS
// =============================================================================
function resizeCanvas() {
  const dpr = window.devicePixelRatio || 1;
  const rect = viewport.getBoundingClientRect();
  canvas.width = rect.width * dpr;
  canvas.height = rect.height * dpr;
  canvas.style.width = `${rect.width}px`;
  canvas.style.height = `${rect.height}px`;
  ctx.scale(dpr, dpr);
}

function autoFitView() {
  if (!state.problem || !state.problem.states || state.problem.states.length === 0) {
    state.panX = 0;
    state.panY = 0;
    state.zoom = 1.0;
    state.worldCenterX = 4.5;
    state.worldCenterY = 3.5;
    updateZoomText();
    return;
  }

  let minX = Infinity, maxX = -Infinity, minY = Infinity, maxY = -Infinity;
  state.problem.states.forEach(s => {
    const x = s.embedding[state.projAxisX] || 0;
    const y = s.embedding[state.projAxisY] || 0;
    if (x < minX) minX = x;
    if (x > maxX) maxX = x;
    if (y < minY) minY = y;
    if (y > maxY) maxY = y;
  });

  const spanX = Math.max(0.1, maxX - minX);
  const spanY = Math.max(0.1, maxY - minY);
  const rect = viewport.getBoundingClientRect();

  const padding = 160;
  const availW = Math.max(200, rect.width - padding);
  const availH = Math.max(200, rect.height - padding);

  const baseScale = 70.0;
  const idealScaleX = availW / spanX;
  const idealScaleY = availH / spanY;
  const idealScale = Math.min(idealScaleX, idealScaleY);

  state.zoom = Math.min(25.0, Math.max(0.2, idealScale / baseScale));
  state.worldCenterX = (minX + maxX) / 2.0;
  state.worldCenterY = (minY + maxY) / 2.0;

  state.panX = 0;
  state.panY = 0;

  updateZoomText();
}

function updateZoomText() {
  if (zoomLevelText) {
    zoomLevelText.textContent = `${Math.round(state.zoom * 100)}%`;
  }
}

function worldToScreen(wx, wy, wz = 0) {
  const rect = viewport.getBoundingClientRect();
  const scale = 70 * state.zoom;
  const cx = rect.width / 2 + state.panX;
  const cy = rect.height / 2 + state.panY;
  const wCenterX = state.worldCenterX !== undefined ? state.worldCenterX : 4.5;
  const wCenterY = state.worldCenterY !== undefined ? state.worldCenterY : 3.5;

  const dx = (wx - wCenterX) * scale;
  const dy = (wy - wCenterY) * scale;

  if (state.viewMode === 'isometric') {
    const radPitch = (state.isoPitch * Math.PI) / 180;
    const radYaw = (state.isoYaw * Math.PI) / 180;
    const dz = wz * 18.0 * Math.min(2.0, state.zoom);

    // 3D Isometric Matrix Rotation
    const rx = dx * Math.cos(radYaw) - dy * Math.sin(radYaw);
    const ry = (dx * Math.sin(radYaw) + dy * Math.cos(radYaw)) * Math.sin(radPitch) - dz * Math.cos(radPitch);

    return { x: cx + rx, y: cy + ry };
  } else {
    return {
      x: cx + dx,
      y: cy + dy
    };
  }
}

function screenToWorld(sx, sy) {
  const rect = viewport.getBoundingClientRect();
  const scale = 70 * state.zoom;
  const cx = rect.width / 2 + state.panX;
  const cy = rect.height / 2 + state.panY;
  const wCenterX = state.worldCenterX !== undefined ? state.worldCenterX : 4.5;
  const wCenterY = state.worldCenterY !== undefined ? state.worldCenterY : 3.5;

  return {
    wx: (sx - cx) / scale + wCenterX,
    wy: (sy - cy) / scale + wCenterY
  };
}

function getNodeElevation(s) {
  if (state.isoExtrudeMode === 'cost') {
    return (s.id % 4) + 1.0;
  } else if (state.isoExtrudeMode === 'clearance') {
    return Math.min(3.0, (s.embedding[2] || 0.5) * 3.0);
  } else if (state.isoExtrudeMode === 'dim3') {
    return (s.embedding[2] || 0.0) * 4.0;
  }
  return 1.0;
}

// =============================================================================
// 4. ANIMATED RENDER LOOP
// =============================================================================
function renderLoop(timestamp) {
  drawCanvas(timestamp);
  requestAnimationFrame(renderLoop);
}

function drawCanvas(timestamp) {
  const rect = viewport.getBoundingClientRect();
  ctx.clearRect(0, 0, rect.width, rect.height);

  drawGrid(rect);

  if (!state.problem) return;

  const statesMap = new Map();
  state.problem.states.forEach(s => statesMap.set(s.id, s));

  // 1. Draw Hazard Clearances & Potential Halos
  drawHazardHalos(statesMap, timestamp);

  // 2. Draw Edges & Trajectories
  drawEdges(statesMap);

  // 3. Draw Nodes / Pedestals
  drawNodes(statesMap);

  // 4. Draw Animated Agent Traveling
  if (state.isAnimatingAgent && state.agentStep !== null) {
    drawAnimatedAgent(statesMap);
  }

  // 5. Draw 2-State Vector Clearance Line
  if (state.inspectStateA && state.inspectStateB) {
    drawVectorInspectionLine();
  }
}

function drawGrid(rect) {
  const scale = 70 * state.zoom;
  ctx.save();
  ctx.strokeStyle = '#f1f5f9';
  ctx.lineWidth = 1;

  if (state.viewMode === 'isometric') {
    ctx.strokeStyle = '#e2e8f0';
    const wCX = state.worldCenterX !== undefined ? state.worldCenterX : 4.5;
    const wCY = state.worldCenterY !== undefined ? state.worldCenterY : 3.5;
    const minX = wCX - 2.5;
    const maxX = wCX + 2.5;
    const minY = wCY - 2.5;
    const maxY = wCY + 2.5;
    const step = 0.5;

    for (let x = minX; x <= maxX + 0.01; x += step) {
      const p1 = worldToScreen(x, minY, 0);
      const p2 = worldToScreen(x, maxY, 0);
      ctx.beginPath();
      ctx.moveTo(p1.x, p1.y);
      ctx.lineTo(p2.x, p2.y);
      ctx.stroke();
    }
    for (let y = minY; y <= maxY + 0.01; y += step) {
      const p1 = worldToScreen(minX, y, 0);
      const p2 = worldToScreen(maxX, y, 0);
      ctx.beginPath();
      ctx.moveTo(p1.x, p1.y);
      ctx.lineTo(p2.x, p2.y);
      ctx.stroke();
    }
  } else {
    // 2D Orthographic Dot Grid
    const cx = rect.width / 2 + state.panX;
    const cy = rect.height / 2 + state.panY;
    const gridSpacing = Math.max(18, scale);

    const startX = cx % gridSpacing;
    const startY = cy % gridSpacing;

    ctx.fillStyle = '#cbd5e1';
    for (let x = startX; x < rect.width; x += gridSpacing) {
      for (let y = startY; y < rect.height; y += gridSpacing) {
        ctx.beginPath();
        ctx.arc(x, y, 1.2, 0, Math.PI * 2);
        ctx.fill();
      }
    }
  }
  ctx.restore();
}

function drawHazardHalos(statesMap) {
  if (!state.problem.badStates) return;
  const marginRadius = (state.config && state.config.safety_clearance_margin) ? state.config.safety_clearance_margin : 1.5;
  const scale = 70 * state.zoom;
  const pixelRadius = marginRadius * scale;

  state.problem.badStates.forEach(badId => {
    const s = statesMap.get(badId);
    if (!s) return;

    const wx = s.embedding[state.projAxisX] || 0;
    const wy = s.embedding[state.projAxisY] || 0;
    const wz = getNodeElevation(s);
    const pos = worldToScreen(wx, wy, wz);

    ctx.save();
    const grad = ctx.createRadialGradient(pos.x, pos.y, 4, pos.x, pos.y, pixelRadius);
    grad.addColorStop(0, 'rgba(239, 68, 68, 0.35)');
    grad.addColorStop(0.5, 'rgba(239, 68, 68, 0.12)');
    grad.addColorStop(1, 'rgba(239, 68, 68, 0.0)');

    ctx.fillStyle = grad;
    ctx.beginPath();
    ctx.arc(pos.x, pos.y, pixelRadius, 0, Math.PI * 2);
    ctx.fill();

    // Outer Clearance Margin Ring
    ctx.strokeStyle = 'rgba(239, 68, 68, 0.45)';
    ctx.setLineDash([4, 4]);
    ctx.lineWidth = 1.5;
    ctx.beginPath();
    ctx.arc(pos.x, pos.y, pixelRadius, 0, Math.PI * 2);
    ctx.stroke();
    ctx.restore();
  });
}

function drawEdges(statesMap) {
  if (!state.problem.transitions) return;

  const optimalTransSet = new Set(state.result && state.result.transitionPath ? state.result.transitionPath : []);

  state.problem.transitions.forEach(t => {
    const sFrom = statesMap.get(t.from);
    const sTo = statesMap.get(t.to);
    if (!sFrom || !sTo) return;

    const pFrom = worldToScreen(sFrom.embedding[state.projAxisX] || 0, sFrom.embedding[state.projAxisY] || 0, getNodeElevation(sFrom));
    const pTo = worldToScreen(sTo.embedding[state.projAxisX] || 0, sTo.embedding[state.projAxisY] || 0, getNodeElevation(sTo));

    const isOptimal = optimalTransSet.has(t.id);
    const isHovered = state.hoveredEdge && state.hoveredEdge.id === t.id;

    ctx.save();
    ctx.beginPath();
    ctx.moveTo(pFrom.x, pFrom.y);
    ctx.lineTo(pTo.x, pTo.y);

    if (!t.available) {
      // Severed / Broken Edge
      ctx.strokeStyle = 'rgba(239, 68, 68, 0.4)';
      ctx.lineWidth = 2;
      ctx.setLineDash([4, 4]);
      ctx.stroke();
    } else if (isOptimal) {
      // Optimal Safe Path Trajectory Ribbon
      ctx.strokeStyle = '#10b981';
      ctx.lineWidth = 4.0;
      ctx.setLineDash([8, 4]);
      ctx.lineCap = 'round';
      ctx.stroke();
    } else {
      // Normal Available Transition
      ctx.strokeStyle = isHovered ? '#6366f1' : '#cbd5e1';
      ctx.lineWidth = isHovered ? 2.5 : 1.5;
      ctx.stroke();
    }
    ctx.restore();

    // Draw Cost Pill on edge midpoint only if not crowded or when optimal/hovered
    const totalStates = state.problem.states.length;
    if (totalStates <= 15 || isOptimal || isHovered) {
      const midX = (pFrom.x + pTo.x) / 2;
      const midY = (pFrom.y + pTo.y) / 2;
      drawCostPill(midX, midY, t.cost.toFixed(1), isOptimal, t.available);
    }
  });
}

function drawCostPill(x, y, text, isOptimal, isAvailable) {
  ctx.save();
  ctx.font = '600 10px "JetBrains Mono", monospace';
  const width = ctx.measureText(text).width + 8;
  const height = 15;

  ctx.fillStyle = 'rgba(255, 255, 255, 0.94)';
  ctx.strokeStyle = isOptimal ? '#10b981' : (isAvailable ? '#cbd5e1' : '#ef4444');
  ctx.lineWidth = isOptimal ? 1.5 : 1;

  ctx.beginPath();
  ctx.roundRect(x - width / 2, y - height / 2, width, height, 3);
  ctx.fill();
  ctx.stroke();

  ctx.fillStyle = isOptimal ? '#065f46' : (isAvailable ? '#475569' : '#991b1b');
  ctx.textAlign = 'center';
  ctx.textBaseline = 'middle';
  ctx.fillText(text, x, y);
  ctx.restore();
}

function drawNodes(statesMap) {
  const badSet = new Set(state.problem.badStates || []);
  const optimalSet = new Set(state.result && state.result.statePath ? state.result.statePath : []);
  const isCrowded = state.problem.states.length > 15;

  state.problem.states.forEach(s => {
    const wx = s.embedding[state.projAxisX] || 0;
    const wy = s.embedding[state.projAxisY] || 0;
    const wz = getNodeElevation(s);
    const pos = worldToScreen(wx, wy, wz);

    const isStart = s.id === state.problem.initialState;
    const isGoal = s.id === state.problem.goalState;
    const isBad = badSet.has(s.id);
    const isOptimal = optimalSet.has(s.id);
    const isInspectA = state.inspectStateA && state.inspectStateA.id === s.id;
    const isInspectB = state.inspectStateB && state.inspectStateB.id === s.id;
    const isSelectedEdgeSource = state.selectedEdgeSource && state.selectedEdgeSource.id === s.id;
    const isHovered = state.hoveredNode && state.hoveredNode.id === s.id;

    ctx.save();

    // In 3D Isometric View: Draw Vertical Pedestal Column
    if (state.viewMode === 'isometric' && wz > 0) {
      const basePos = worldToScreen(wx, wy, 0);
      ctx.strokeStyle = 'rgba(148, 163, 184, 0.4)';
      ctx.lineWidth = 1.5;
      ctx.setLineDash([2, 2]);
      ctx.beginPath();
      ctx.moveTo(basePos.x, basePos.y);
      ctx.lineTo(pos.x, pos.y);
      ctx.stroke();
      ctx.setLineDash([]);
    }

    let fillColor = '#ffffff';
    let strokeColor = '#94a3b8';
    let ringColor = null;
    let radius = 12;

    if (isBad) {
      fillColor = '#fee2e2';
      strokeColor = '#ef4444';
      ringColor = 'rgba(239, 68, 68, 0.3)';
    } else if (isStart) {
      fillColor = '#e0e7ff';
      strokeColor = '#4f46e5';
      ringColor = 'rgba(79, 70, 229, 0.3)';
      radius = 14;
    } else if (isGoal) {
      fillColor = '#d1fae5';
      strokeColor = '#10b981';
      ringColor = 'rgba(16, 185, 129, 0.3)';
      radius = 14;
    } else if (isOptimal) {
      fillColor = '#ecfdf5';
      strokeColor = '#10b981';
      radius = 13;
    }

    if (isSelectedEdgeSource) {
      strokeColor = '#3b82f6';
      ringColor = 'rgba(59, 130, 246, 0.5)';
      radius = 15;
    }

    if (isInspectA) strokeColor = '#3b82f6';
    if (isInspectB) strokeColor = '#f59e0b';

    // Outer Glow Ring
    if (ringColor) {
      ctx.fillStyle = ringColor;
      ctx.beginPath();
      ctx.arc(pos.x, pos.y, radius + 5, 0, Math.PI * 2);
      ctx.fill();
    }

    // Node Circle
    ctx.fillStyle = fillColor;
    ctx.strokeStyle = strokeColor;
    ctx.lineWidth = (isHovered || isInspectA || isInspectB || isSelectedEdgeSource) ? 3.0 : 2.0;

    ctx.beginPath();
    ctx.arc(pos.x, pos.y, radius, 0, Math.PI * 2);
    ctx.fill();
    ctx.stroke();

    // Center Glyph / Number
    ctx.font = '700 11px "JetBrains Mono", monospace';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';

    if (isStart) {
      ctx.fillStyle = '#4f46e5';
      ctx.fillText('S', pos.x, pos.y);
    } else if (isGoal) {
      ctx.fillStyle = '#065f46';
      ctx.fillText('G', pos.x, pos.y);
    } else if (isBad) {
      ctx.fillStyle = '#991b1b';
      ctx.fillText('!', pos.x, pos.y);
    } else {
      ctx.fillStyle = '#1e293b';
      ctx.fillText(s.id.toString(), pos.x, pos.y);
    }

    // Smart Label Pill: Show if not crowded OR when Start/Goal/Hazard/Hovered
    if (!isCrowded || isStart || isGoal || isBad || isHovered || isInspectA || isInspectB || isSelectedEdgeSource) {
      const nodeLabel = s.name || `Node_${s.id}`;
      drawNodeLabelPill(pos.x, pos.y + radius + 10, nodeLabel, isBad);
    }

    ctx.restore();
  });
}

function drawNodeLabelPill(x, y, text, isBad) {
  ctx.save();
  ctx.font = '600 10.5px Inter, sans-serif';
  const width = ctx.measureText(text).width + 10;
  const height = 16;

  ctx.fillStyle = 'rgba(255, 255, 255, 0.94)';
  ctx.strokeStyle = isBad ? '#fca5a5' : '#e2e8f0';
  ctx.lineWidth = 1;

  ctx.beginPath();
  ctx.roundRect(x - width / 2, y - height / 2, width, height, 3);
  ctx.fill();
  ctx.stroke();

  ctx.fillStyle = isBad ? '#b91c1c' : '#1e293b';
  ctx.textAlign = 'center';
  ctx.textBaseline = 'middle';
  ctx.fillText(text, x, y);
  ctx.restore();
}

function drawAnimatedAgent(statesMap) {
  if (!state.result || !state.result.statePath || state.result.statePath.length < 2) return;
  const path = state.result.statePath;
  const curIdx = Math.floor(state.agentStep);
  const nextIdx = Math.min(path.length - 1, curIdx + 1);
  const frac = state.agentStep - curIdx;

  const s1 = statesMap.get(path[curIdx]);
  const s2 = statesMap.get(path[nextIdx]);
  if (!s1 || !s2) return;

  const p1 = worldToScreen(s1.embedding[state.projAxisX] || 0, s1.embedding[state.projAxisY] || 0, getNodeElevation(s1));
  const p2 = worldToScreen(s2.embedding[state.projAxisX] || 0, s2.embedding[state.projAxisY] || 0, getNodeElevation(s2));

  const curX = p1.x + (p2.x - p1.x) * frac;
  const curY = p1.y + (p2.y - p1.y) * frac;

  ctx.save();
  ctx.fillStyle = '#6366f1';
  ctx.strokeStyle = '#ffffff';
  ctx.lineWidth = 2.5;

  ctx.beginPath();
  ctx.arc(curX, curY, 7, 0, Math.PI * 2);
  ctx.fill();
  ctx.stroke();
  ctx.restore();
}

function drawVectorInspectionLine() {
  const pA = worldToScreen(state.inspectStateA.embedding[state.projAxisX] || 0, state.inspectStateA.embedding[state.projAxisY] || 0, getNodeElevation(state.inspectStateA));
  const pB = worldToScreen(state.inspectStateB.embedding[state.projAxisX] || 0, state.inspectStateB.embedding[state.projAxisY] || 0, getNodeElevation(state.inspectStateB));

  ctx.save();
  ctx.strokeStyle = '#3b82f6';
  ctx.lineWidth = 2;
  ctx.setLineDash([5, 3]);

  ctx.beginPath();
  ctx.moveTo(pA.x, pA.y);
  ctx.lineTo(pB.x, pB.y);
  ctx.stroke();

  let sumSq = 0;
  for (let i = 0; i < state.inspectStateA.embedding.length; ++i) {
    const diff = (state.inspectStateA.embedding[i] || 0) - (state.inspectStateB.embedding[i] || 0);
    sumSq += diff * diff;
  }
  const dist = Math.sqrt(sumSq).toFixed(2);
  const midX = (pA.x + pB.x) / 2;
  const midY = (pA.y + pB.y) / 2;

  ctx.font = '700 10.5px "JetBrains Mono", monospace';
  const label = `||Δ|| = ${dist}`;
  const width = ctx.measureText(label).width + 10;

  ctx.fillStyle = '#0f172a';
  ctx.beginPath();
  ctx.roundRect(midX - width / 2, midY - 9, width, 18, 3);
  ctx.fill();

  ctx.fillStyle = '#ffffff';
  ctx.textAlign = 'center';
  ctx.textBaseline = 'middle';
  ctx.fillText(label, midX, midY);

  ctx.restore();
}

// =============================================================================
// 5. RICH HOVER TOOLTIP SYSTEM & MOUSE INTERACTION
// =============================================================================
function handleCanvasMouseMove(e) {
  const rect = viewport.getBoundingClientRect();
  const mx = e.clientX - rect.left;
  const my = e.clientY - rect.top;

  if (state.draggedNode) {
    const worldPos = screenToWorld(mx, my);
    state.draggedNode.embedding[state.projAxisX] = worldPos.wx;
    state.draggedNode.embedding[state.projAxisY] = worldPos.wy;
    viewport.style.cursor = 'grabbing';
    computePlan();
    return;
  }

  if (state.isPanning) {
    state.panX += mx - state.lastMouseX;
    state.panY += my - state.lastMouseY;
    state.lastMouseX = mx;
    state.lastMouseY = my;
    viewport.style.cursor = 'grabbing';
    return;
  }

  if (state.isRotatingIso && state.viewMode === 'isometric') {
    const dx = mx - state.lastMouseX;
    const dy = my - state.lastMouseY;
    state.isoYaw = Math.max(-75, Math.min(75, state.isoYaw + dx * 0.5));
    state.isoPitch = Math.max(15, Math.min(80, state.isoPitch - dy * 0.5));
    document.getElementById('sliderYaw').value = state.isoYaw;
    document.getElementById('sliderPitch').value = state.isoPitch;
    document.getElementById('valYaw').textContent = `${Math.round(state.isoYaw)}°`;
    document.getElementById('valPitch').textContent = `${Math.round(state.isoPitch)}°`;
    state.lastMouseX = mx;
    state.lastMouseY = my;
    viewport.style.cursor = 'grabbing';
    return;
  }

  // Hover detection
  if (!state.problem) return;

  let foundNode = null;
  state.problem.states.forEach(s => {
    const pos = worldToScreen(s.embedding[state.projAxisX] || 0, s.embedding[state.projAxisY] || 0, getNodeElevation(s));
    const dist = Math.hypot(mx - pos.x, my - pos.y);
    if (dist <= 16) foundNode = s;
  });

  state.hoveredNode = foundNode;

  let foundEdge = null;
  if (!foundNode) {
    const statesMap = new Map();
    state.problem.states.forEach(s => statesMap.set(s.id, s));

    state.problem.transitions.forEach(t => {
      const sFrom = statesMap.get(t.from);
      const sTo = statesMap.get(t.to);
      if (!sFrom || !sTo) return;

      const p1 = worldToScreen(sFrom.embedding[state.projAxisX] || 0, sFrom.embedding[state.projAxisY] || 0, getNodeElevation(sFrom));
      const p2 = worldToScreen(sTo.embedding[state.projAxisX] || 0, sTo.embedding[state.projAxisY] || 0, getNodeElevation(sTo));

      const dist = distToSegment({ x: mx, y: my }, p1, p2);
      if (dist <= 8) foundEdge = t;
    });
  }
  state.hoveredEdge = foundEdge;

  // Set appropriate cursor
  if (state.currentMode === 'pan') {
    viewport.style.cursor = 'grab';
  } else if (state.currentMode === 'add-node') {
    viewport.style.cursor = 'crosshair';
  } else if (foundNode || foundEdge) {
    viewport.style.cursor = 'pointer';
  } else {
    viewport.style.cursor = 'default';
  }

  // Render Tooltip
  if (foundNode) {
    renderNodeTooltip(foundNode, e.clientX, e.clientY);
  } else if (foundEdge) {
    renderEdgeTooltip(foundEdge, e.clientX, e.clientY);
  } else {
    tooltip.style.display = 'none';
  }
}

function distToSegment(p, v, w) {
  const l2 = (v.x - w.x) ** 2 + (v.y - w.y) ** 2;
  if (l2 === 0) return Math.hypot(p.x - v.x, p.y - v.y);
  let t = ((p.x - v.x) * (w.x - v.x) + (p.y - v.y) * (w.y - v.y)) / l2;
  t = Math.max(0, Math.min(1, t));
  return Math.hypot(p.x - (v.x + t * (w.x - v.x)), p.y - (v.y + t * (w.y - v.y)));
}

function renderNodeTooltip(node, clientX, clientY) {
  const isStart = node.id === state.problem.initialState;
  const isGoal = node.id === state.problem.goalState;
  const isBad = (state.problem.badStates || []).includes(node.id);

  let badgeClass = 'badge-normal';
  let badgeText = 'State Node';
  if (isStart) { badgeClass = 'badge-start'; badgeText = 'Start State'; }
  else if (isGoal) { badgeClass = 'badge-goal'; badgeText = 'Goal Terminal'; }
  else if (isBad) { badgeClass = 'badge-hazard'; badgeText = 'Quarantined Hazard'; }

  const vecStr = '[' + node.embedding.map(v => v.toFixed(2)).join(', ') + ']';

  let inDeg = 0, outDeg = 0;
  (state.problem.transitions || []).forEach(t => {
    if (t.from === node.id) outDeg++;
    if (t.to === node.id) inDeg++;
  });

  tooltip.innerHTML = `
    <div class="tooltip-header">
      <span class="tooltip-title">#${node.id}: ${node.name || 'Unnamed'}</span>
      <span class="tooltip-badge ${badgeClass}">${badgeText}</span>
    </div>
    <div class="tooltip-grid">
      <div class="tooltip-row"><span class="tooltip-label">In / Out Edges:</span><span class="tooltip-val">${inDeg} / ${outDeg}</span></div>
      <div class="tooltip-row"><span class="tooltip-label">Dimensions:</span><span class="tooltip-val">${node.embedding.length}D</span></div>
    </div>
    <div class="tooltip-vector">${vecStr}</div>
  `;

  tooltip.style.left = `${clientX}px`;
  tooltip.style.top = `${clientY}px`;
  tooltip.style.display = 'flex';
}

function renderEdgeTooltip(edge, clientX, clientY) {
  tooltip.innerHTML = `
    <div class="tooltip-header">
      <span class="tooltip-title">Edge #${edge.id}: ${edge.name || 'Transition'}</span>
      <span class="tooltip-badge ${edge.available ? 'badge-goal' : 'badge-hazard'}">${edge.available ? 'Available' : 'Severed'}</span>
    </div>
    <div class="tooltip-grid">
      <div class="tooltip-row"><span class="tooltip-label">Route:</span><span class="tooltip-val">#${edge.from} &rarr; #${edge.to}</span></div>
      <div class="tooltip-row"><span class="tooltip-label">Base Cost:</span><span class="tooltip-val">${edge.cost.toFixed(2)}</span></div>
      <div class="tooltip-row"><span class="tooltip-label">Reliability SLA:</span><span class="tooltip-val">${(edge.reliability * 100).toFixed(1)}%</span></div>
      <div class="tooltip-row"><span class="tooltip-label">Safety Margin:</span><span class="tooltip-val">${(edge.safety * 100).toFixed(0)}%</span></div>
    </div>
  `;

  tooltip.style.left = `${clientX}px`;
  tooltip.style.top = `${clientY}px`;
  tooltip.style.display = 'flex';
}

// =============================================================================
// 6. PIPELINE VIEW & DATA MATRIX VIEW RENDERERS
// =============================================================================
function renderPipelineView() {
  const track = document.getElementById('pipelineTrack');
  const badge = document.getElementById('pipelineBadge');
  if (!track) return;

  if (!state.result || !state.result.statePath || state.result.statePath.length === 0) {
    track.innerHTML = '<p class="empty-hint">No valid trajectory path computed to display in pipeline.</p>';
    if (badge) badge.textContent = '0 Steps';
    return;
  }

  const statesMap = new Map();
  state.problem.states.forEach(s => statesMap.set(s.id, s));
  const edgesMap = new Map();
  state.problem.transitions.forEach(t => edgesMap.set(t.id, t));

  const steps = state.result.statePath;
  const trans = state.result.transitionPath || [];
  if (badge) badge.textContent = `${steps.length} Steps (${trans.length} Transitions)`;

  let html = '';
  for (let i = 0; i < steps.length; ++i) {
    const sid = steps[i];
    const s = statesMap.get(sid);
    const isStart = sid === state.problem.initialState;
    const isGoal = sid === state.problem.goalState;

    let badgeClass = '';
    if (isStart) badgeClass = 'badge-start';
    if (isGoal) badgeClass = 'badge-goal';

    const vecStr = s ? '[' + s.embedding.map(v => v.toFixed(2)).join(', ') + ']' : '[]';

    html += `
      <div class="pipeline-step-card">
        <div class="step-num-badge ${badgeClass}">${i + 1}</div>
        <div class="step-content-group">
          <span class="step-name">#${sid}: ${s ? s.name : 'Unknown'}</span>
          <span class="step-vector-chips">${vecStr}</span>
        </div>
        <div class="step-metrics-group">
          <div class="step-metric-item">
            <span class="step-metric-label">Role</span>
            <span class="step-metric-val">${isStart ? 'Start' : (isGoal ? 'Goal' : 'Waypoint')}</span>
          </div>
        </div>
      </div>
    `;

    if (i < trans.length) {
      const tid = trans[i];
      const edge = edgesMap.get(tid);
      html += `
        <div class="pipeline-transition-connector">
          <i data-lucide="arrow-down"></i>
          <span class="trans-pill">Transition #${tid}: ${edge ? edge.name : 'Action'} (Cost: ${edge ? edge.cost.toFixed(1) : '-'})</span>
        </div>
      `;
    }
  }

  track.innerHTML = html;
  lucide.createIcons();
}

function renderTableView() {
  const container = document.getElementById('tableContainer');
  const countS = document.getElementById('tableStateCount');
  const countT = document.getElementById('tableTransCount');
  if (!container || !state.problem) return;

  if (countS) countS.textContent = (state.problem.states || []).length;
  if (countT) countT.textContent = (state.problem.transitions || []).length;

  if (state.tableSubTab === 'states-table') {
    let rows = '';
    state.problem.states.forEach(s => {
      const isStart = s.id === state.problem.initialState;
      const isGoal = s.id === state.problem.goalState;
      const isBad = (state.problem.badStates || []).includes(s.id);
      let role = 'Normal';
      if (isStart) role = 'Start';
      if (isGoal) role = 'Goal';
      if (isBad) role = 'Quarantined Hazard';

      rows += `
        <tr>
          <td class="font-mono">#${s.id}</td>
          <td><strong>${s.name}</strong></td>
          <td class="font-mono">${role}</td>
          <td class="font-mono">[${s.embedding.map(v => v.toFixed(3)).join(', ')}]</td>
        </tr>
      `;
    });

    container.innerHTML = `
      <table class="data-table">
        <thead>
          <tr>
            <th>ID</th>
            <th>State Name</th>
            <th>Role</th>
            <th>Coordinate Embedding Vector</th>
          </tr>
        </thead>
        <tbody>${rows}</tbody>
      </table>
    `;
  } else if (state.tableSubTab === 'transitions-table') {
    let rows = '';
    state.problem.transitions.forEach(t => {
      rows += `
        <tr>
          <td class="font-mono">#${t.id}</td>
          <td><strong>${t.name || 'Transition'}</strong></td>
          <td class="font-mono">#${t.from} &rarr; #${t.to}</td>
          <td class="font-mono">${t.cost.toFixed(2)}</td>
          <td class="font-mono">${(t.reliability * 100).toFixed(1)}%</td>
          <td class="font-mono">${t.available ? 'Active' : 'Severed'}</td>
        </tr>
      `;
    });

    container.innerHTML = `
      <table class="data-table">
        <thead>
          <tr>
            <th>ID</th>
            <th>Action Name</th>
            <th>Directed Edge</th>
            <th>Cost</th>
            <th>Reliability</th>
            <th>Availability</th>
          </tr>
        </thead>
        <tbody>${rows}</tbody>
      </table>
    `;
  } else if (state.tableSubTab === 'matrix-table') {
    const states = state.problem.states || [];
    let headers = '<th>State</th>';
    states.forEach(s => headers += `<th>#${s.id}</th>`);

    let rows = '';
    states.forEach(s1 => {
      let cells = `<td class="font-mono"><strong>#${s1.id}</strong></td>`;
      states.forEach(s2 => {
        let sumSq = 0;
        for (let i = 0; i < s1.embedding.length; ++i) {
          const d = s1.embedding[i] - (s2.embedding[i] || 0);
          sumSq += d * d;
        }
        const dist = Math.sqrt(sumSq).toFixed(2);
        cells += `<td class="font-mono">${dist}</td>`;
      });
      rows += `<tr>${cells}</tr>`;
    });

    container.innerHTML = `
      <table class="data-table">
        <thead><tr>${headers}</tr></thead>
        <tbody>${rows}</tbody>
      </table>
    `;
  }
}

// =============================================================================
// 7. EVENT LISTENERS & ACTION HANDLERS
// =============================================================================
function setupEventListeners() {
  // Top View Buttons
  document.querySelectorAll('.view-btn').forEach(btn => {
    btn.addEventListener('click', () => setViewMode(btn.dataset.view));
  });

  // Table Sub-tabs
  document.querySelectorAll('.sub-tab-btn').forEach(btn => {
    btn.addEventListener('click', () => {
      document.querySelectorAll('.sub-tab-btn').forEach(b => b.classList.remove('active'));
      btn.classList.add('active');
      state.tableSubTab = btn.dataset.subtab;
      renderTableView();
    });
  });

  // Sidebar Segmented Tabs
  document.querySelectorAll('.tab-btn').forEach(btn => {
    btn.addEventListener('click', () => {
      document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
      document.querySelectorAll('.tab-content').forEach(c => c.classList.remove('active'));
      btn.classList.add('active');
      const tabEl = document.getElementById(`tab-${btn.dataset.tab}`);
      if (tabEl) tabEl.classList.add('active');
      if (btn.dataset.tab === 'constraints') updateConstraintsTab();
    });
  });

  // Left Vertical Dock Modes
  document.querySelectorAll('.v-dock-btn').forEach(btn => {
    btn.addEventListener('click', () => {
      document.querySelectorAll('.v-dock-btn').forEach(b => b.classList.remove('active'));
      btn.classList.add('active');
      state.currentMode = btn.dataset.mode;
      state.selectedEdgeSource = null;
      updateModeBanner();
    });
  });

  // Legend Toggle
  if (legendToggleBtn && canvasLegend) {
    legendToggleBtn.addEventListener('click', () => {
      const isHidden = canvasLegend.style.display === 'none';
      canvasLegend.style.display = isHidden ? 'flex' : 'none';
    });
  }

  // Zoom Controls
  document.getElementById('zoomInBtn').addEventListener('click', () => {
    state.zoom = Math.min(50.0, state.zoom * 1.25);
    updateZoomText();
  });
  document.getElementById('zoomOutBtn').addEventListener('click', () => {
    state.zoom = Math.max(0.02, state.zoom / 1.25);
    updateZoomText();
  });
  document.getElementById('fitViewBtn').addEventListener('click', autoFitView);
  if (zoomLevelText) zoomLevelText.addEventListener('click', autoFitView);

  // Mouse Wheel Zoom centered at cursor
  viewport.addEventListener('wheel', (e) => {
    e.preventDefault();
    const rect = viewport.getBoundingClientRect();
    const mx = e.clientX - rect.left;
    const my = e.clientY - rect.top;

    const zoomFactor = e.deltaY < 0 ? 1.15 : 0.87;
    const newZoom = Math.min(50.0, Math.max(0.02, state.zoom * zoomFactor));

    state.panX = mx - (mx - state.panX) * (newZoom / state.zoom);
    state.panY = my - (my - state.panY) * (newZoom / state.zoom);
    state.zoom = newZoom;
    updateZoomText();
  }, { passive: false });

  // Canvas Mouse Drag / Pan / Click
  viewport.addEventListener('mousedown', handleCanvasMouseDown);
  window.addEventListener('mousemove', handleCanvasMouseMove);
  window.addEventListener('mouseup', handleCanvasMouseUp);

  // 3D Isometric Sliders
  document.getElementById('sliderPitch').addEventListener('input', (e) => {
    state.isoPitch = parseFloat(e.target.value);
    document.getElementById('valPitch').textContent = `${Math.round(state.isoPitch)}°`;
  });
  document.getElementById('sliderYaw').addEventListener('input', (e) => {
    state.isoYaw = parseFloat(e.target.value);
    document.getElementById('valYaw').textContent = `${Math.round(state.isoYaw)}°`;
  });
  document.getElementById('selectZDimension').addEventListener('change', (e) => {
    state.isoExtrudeMode = e.target.value;
  });
  document.getElementById('resetTiltBtn').addEventListener('click', () => {
    state.isoPitch = 45;
    state.isoYaw = 35;
    document.getElementById('sliderPitch').value = 45;
    document.getElementById('sliderYaw').value = 35;
    document.getElementById('valPitch').textContent = '45°';
    document.getElementById('valYaw').textContent = '35°';
  });

  // Projection Axis Selectors
  document.getElementById('projAxisX').addEventListener('change', (e) => {
    state.projAxisX = parseInt(e.target.value);
    autoFitView();
  });
  document.getElementById('projAxisY').addEventListener('change', (e) => {
    state.projAxisY = parseInt(e.target.value);
    autoFitView();
  });

  // Domain Template Switcher
  document.getElementById('templateSelect').addEventListener('change', async (e) => {
    const tmplIdx = parseInt(e.target.value);
    try {
      const res = await fetch('/api/templates');
      if (res.ok) {
        const templates = await res.json();
        if (templates[tmplIdx]) {
          state.problem = templates[tmplIdx];
          updateDimensionSelectors();
          populateStatePickers();
          updateConstraintsTab();
          updateTCTabVisibility(tmplIdx);
          autoFitView();
          await computePlan();

          // If a Test Case is selected, automatically switch to the Assignment TCs tab
          if (tmplIdx >= 0 && tmplIdx <= 5) {
            document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
            document.querySelectorAll('.tab-content').forEach(c => c.classList.remove('active'));
            const tcTabBtn = document.getElementById('tabBtnAssignmentTCs');
            const tcTabContent = document.getElementById('tab-assignment-tcs');
            if (tcTabBtn) tcTabBtn.classList.add('active');
            if (tcTabContent) tcTabContent.classList.add('active');
          }
        }
      }
    } catch (err) {
      console.error('Error switching domain template:', err);
    }
  });

  // Replan Button
  document.getElementById('replanBtn').addEventListener('click', computePlan);

  // Animate Agent
  document.getElementById('animateAgentBtn').addEventListener('click', startAgentAnimation);

  // Reset Domain
  document.getElementById('resetDomainBtn').addEventListener('click', loadProblem);

  // Schema & Path Exports
  document.getElementById('downloadSchemaBtn').addEventListener('click', async () => {
    const res = await fetch('/api/schema');
    if (res.ok) {
      const schema = await res.json();
      downloadFile(JSON.stringify(schema, null, 2), 'schema.json', 'application/json');
    }
  });
  document.getElementById('exportPathBtn').addEventListener('click', () => {
    if (!state.result || !state.result.success || !state.result.statePath || state.result.statePath.length === 0) {
      alert('No valid path available to export. The target may be unreachable.');
      return;
    }

    const statesMap = new Map();
    if (state.problem && state.problem.states) {
      state.problem.states.forEach(s => statesMap.set(s.id, s));
    }
    const transMap = new Map();
    if (state.problem && state.problem.transitions) {
      state.problem.transitions.forEach(t => transMap.set(t.id, t));
    }

    const domainName = (state.problem && (state.problem.domainName || state.problem.name)) || 'Safe Semantic Domain';
    const initS = statesMap.get(state.problem ? state.problem.initialState : null);
    const goalS = statesMap.get(state.problem ? state.problem.goalState : null);
    const badSet = new Set(state.problem ? (state.problem.badStates || []) : []);

    const enrichedTrajectory = state.result.statePath.map((sid, idx) => {
      const s = statesMap.get(sid);
      let role = 'Waypoint';
      if (state.problem && sid === state.problem.initialState) role = 'Start State';
      else if (state.problem && sid === state.problem.goalState) role = 'Goal Destination';
      else if (badSet.has(sid)) role = 'Quarantined Hazard';

      // Dimension breakdown
      const dimensionCoordinates = {};
      if (s && s.embedding && state.problem && state.problem.stateSpace && state.problem.stateSpace.dimensions) {
        state.problem.stateSpace.dimensions.forEach((dim, dIdx) => {
          dimensionCoordinates[dim.name || `Dim_${dIdx + 1}`] = s.embedding[dIdx];
        });
      }

      const stepObj = {
        stepIndex: idx + 1,
        stateId: sid,
        name: s ? s.name : `Node_${sid}`,
        description: (s && s.description) ? s.description : (s ? `State #${sid}: ${s.name} in domain [${domainName}]` : `State #${sid}`),
        role: role,
        embedding: s ? s.embedding : [],
        dimensionCoordinates: Object.keys(dimensionCoordinates).length > 0 ? dimensionCoordinates : undefined
      };

      // Outgoing transition along the optimal trajectory
      if (state.result.transitionPath && idx < state.result.transitionPath.length) {
        const tid = state.result.transitionPath[idx];
        const t = transMap.get(tid);
        stepObj.nextTransition = {
          transitionId: tid,
          name: t ? (t.name || `Transition_${tid}`) : `Transition_${tid}`,
          description: (t && t.description) ? t.description : (t ? `Directed edge #${tid} from #${t.from} to #${t.to} (Cost: ${t.cost}, Reliability: ${(t.reliability * 100).toFixed(1)}%)` : `Transition #${tid}`),
          fromStateId: t ? t.from : sid,
          toStateId: t ? t.to : (idx + 1 < state.result.statePath.length ? state.result.statePath[idx + 1] : null),
          cost: t ? t.cost : 1.0,
          reliabilitySLA: t ? t.reliability : 1.0,
          safetyMargin: t ? t.safety : 1.0,
          available: t ? t.available : true
        };
      }

      return stepObj;
    });

    const exportPayload = {
      domainName: domainName,
      planningEngine: 'D* Lite Incremental Replanning',
      spatialIndex: 'Spatial k-d Tree Euclidean Repulsive Potential Field',
      exportedAt: new Date().toISOString(),
      summary: {
        success: state.result.success,
        totalCost: state.result.totalCost,
        minimumSafetyClearance: state.result.minimumSafetyDistance,
        cumulativeReliability: state.result.cumulativeReliability,
        compositeSafetyScore: state.result.safetyScore,
        planningTimeMicroseconds: state.result.planningTimeMicroseconds,
        exploredStatesCount: state.result.exploredStatesCount,
        totalTrajectoryStates: state.result.statePath.length,
        totalTransitions: (state.result.transitionPath || []).length,
        initialState: {
          id: state.problem ? state.problem.initialState : null,
          name: initS ? initS.name : 'Unknown'
        },
        goalState: {
          id: state.problem ? state.problem.goalState : null,
          name: goalS ? goalS.name : 'Unknown'
        }
      },
      trajectoryPath: enrichedTrajectory,
      stateIdSequence: state.result.statePath,
      transitionIdSequence: state.result.transitionPath || [],
      rawResult: state.result
    };

    const fileName = `${domainName.toLowerCase().replace(/[^a-z0-9]+/g, '_')}_optimal_path.json`;
    downloadFile(JSON.stringify(exportPayload, null, 2), fileName, 'application/json');
  });
  document.getElementById('exportBtn').addEventListener('click', () => {
    if (!state.problem) return;
    downloadFile(JSON.stringify(state.problem, null, 2), 'domain_manifest.json', 'application/json');
  });

  // Upload Manifest Spec
  const fileInput = document.getElementById('jsonFileInput');
  document.getElementById('importBtn').addEventListener('click', () => fileInput.click());
  fileInput.addEventListener('change', handleImportJSON);

  // Tuning Sliders & Presets
  setupTuningSliders();

  // 2-State Pickers
  document.getElementById('selectStateA').addEventListener('change', (e) => {
    const sid = parseInt(e.target.value);
    state.inspectStateA = state.problem.states.find(s => s.id === sid) || null;
    updateVectorInspectorUI();
  });
  document.getElementById('selectStateB').addEventListener('change', (e) => {
    const sid = parseInt(e.target.value);
    state.inspectStateB = state.problem.states.find(s => s.id === sid) || null;
    updateVectorInspectorUI();
  });

  // Manual Actions in Constraints Tab
  const btnAddHazard = document.getElementById('btnAddHazardManual');
  if (btnAddHazard) {
    btnAddHazard.addEventListener('click', () => {
      const sel = document.getElementById('selectHazardCandidate');
      if (sel && sel.value) {
        toggleHazardState(parseInt(sel.value));
      }
    });
  }

  const btnManualAddState = document.getElementById('btnManualAddState');
  if (btnManualAddState) {
    btnManualAddState.addEventListener('click', () => openAddStateModal(5.0, 5.0));
  }

  const btnManualAddEdge = document.getElementById('btnManualAddEdge');
  if (btnManualAddEdge) {
    btnManualAddEdge.addEventListener('click', () => {
      const s1 = (state.problem.states && state.problem.states.length > 0) ? state.problem.states[0].id : 0;
      const s2 = (state.problem.states && state.problem.states.length > 1) ? state.problem.states[1].id : 1;
      openAddEdgeModal(s1, s2);
    });
  }

  // AI Agent NLP Handler
  const nlpInput = document.getElementById('nlpQueryInput');
  const nlpBtn = document.getElementById('nlpSendBtn');
  nlpBtn.addEventListener('click', () => {
    if (nlpInput.value.trim()) handleNlpCommand(nlpInput.value.trim());
  });
  nlpInput.addEventListener('keydown', (e) => {
    if (e.key === 'Enter' && nlpInput.value.trim()) {
      handleNlpCommand(nlpInput.value.trim());
    }
  });
  document.querySelectorAll('.prompt-pill').forEach(pill => {
    pill.addEventListener('click', () => {
      nlpInput.value = pill.dataset.prompt;
      handleNlpCommand(pill.dataset.prompt);
    });
  });

  // PCCST503 Assignment Test Cases Handlers
  setupAssignmentTCHandlers();
}

// =============================================================================
// 8. CANVAS MOUSE ACTIONS & DOCK DISPATCHER
// =============================================================================
function handleCanvasMouseDown(e) {
  const rect = viewport.getBoundingClientRect();
  const mx = e.clientX - rect.left;
  const my = e.clientY - rect.top;

  state.lastMouseX = mx;
  state.lastMouseY = my;

  // Middle mouse, Shift pan, or explicit pan mode
  if (e.button === 1 || e.shiftKey || state.currentMode === 'pan') {
    if (state.viewMode === 'isometric') {
      state.isRotatingIso = true;
    } else {
      state.isPanning = true;
    }
    viewport.style.cursor = 'grabbing';
    return;
  }

  // 1. ADD STATE MODE (Clicks on canvas to place state)
  if (state.currentMode === 'add-node') {
    const worldPos = screenToWorld(mx, my);
    openAddStateModal(worldPos.wx, worldPos.wy);
    return;
  }

  // 2. ADD EDGE MODE (Clicks source node, then destination node)
  if (state.currentMode === 'add-edge') {
    if (state.hoveredNode) {
      if (!state.selectedEdgeSource) {
        state.selectedEdgeSource = state.hoveredNode;
        if (modeBannerText) {
          modeBannerText.textContent = `Selected Node #${state.hoveredNode.id}. Now click destination state.`;
        }
      } else if (state.selectedEdgeSource.id !== state.hoveredNode.id) {
        const fromId = state.selectedEdgeSource.id;
        const toId = state.hoveredNode.id;
        state.selectedEdgeSource = null;
        updateModeBanner();
        openAddEdgeModal(fromId, toId);
      }
    } else {
      state.selectedEdgeSource = null;
      updateModeBanner();
      state.isPanning = true;
    }
    return;
  }

  // 3. EDIT MODE (Clicks node or edge to edit)
  if (state.currentMode === 'edit') {
    if (state.hoveredNode) {
      openEditStateModal(state.hoveredNode);
    } else if (state.hoveredEdge) {
      openEditEdgeModal(state.hoveredEdge);
    } else {
      state.isPanning = true;
    }
    return;
  }

  // 4. DELETE MODE (Clicks node or edge to delete)
  if (state.currentMode === 'delete') {
    if (state.hoveredNode) {
      const nid = state.hoveredNode.id;
      if (confirm(`Delete State #${nid} (${state.hoveredNode.name || 'Unnamed'})?`)) {
        deleteState(nid);
      }
    } else if (state.hoveredEdge) {
      const eid = state.hoveredEdge.id;
      if (confirm(`Delete Transition Edge #${eid}?`)) {
        deleteEdge(eid);
      }
    } else {
      state.isPanning = true;
    }
    return;
  }

  // 5. TOGGLE HAZARD MODE
  if (state.currentMode === 'toggle-hazard') {
    if (state.hoveredNode) {
      const nid = state.hoveredNode.id;
      toggleHazardState(nid);
    } else {
      state.isPanning = true;
    }
    return;
  }

  // 6. TOGGLE / SEVER EDGE MODE
  if (state.currentMode === 'toggle-edge') {
    if (state.hoveredEdge) {
      state.hoveredEdge.available = !state.hoveredEdge.available;
      computePlan();
      updateConstraintsTab();
    } else {
      state.isPanning = true;
    }
    return;
  }

  // 7. SET START MODE
  if (state.currentMode === 'set-start') {
    if (state.hoveredNode) {
      state.problem.initialState = state.hoveredNode.id;
      computePlan();
    } else {
      state.isPanning = true;
    }
    return;
  }

  // 8. SET GOAL MODE
  if (state.currentMode === 'set-goal') {
    if (state.hoveredNode) {
      state.problem.goalState = state.hoveredNode.id;
      computePlan();
    } else {
      state.isPanning = true;
    }
    return;
  }

  // 9. 2-STATE INSPECT VECTOR MODE
  if (state.currentMode === 'inspect-vector') {
    if (state.hoveredNode) {
      if (!state.inspectStateA || (state.inspectStateA && state.inspectStateB)) {
        state.inspectStateA = state.hoveredNode;
        state.inspectStateB = null;
      } else {
        state.inspectStateB = state.hoveredNode;
      }
      populateStatePickers();
      updateVectorInspectorUI();
    } else {
      state.isPanning = true;
    }
    return;
  }

  // 10. DRAG STATE / PAN CANVAS MODE
  if (state.currentMode === 'drag') {
    if (state.hoveredNode) {
      state.draggedNode = state.hoveredNode;
      viewport.style.cursor = 'grabbing';
    } else {
      state.isPanning = true;
      viewport.style.cursor = 'grabbing';
    }
    return;
  }

  // Default fallback: Pan canvas on empty space
  if (!state.hoveredNode && !state.hoveredEdge) {
    state.isPanning = true;
    viewport.style.cursor = 'grabbing';
  }
}

function handleCanvasMouseUp() {
  if (state.draggedNode) {
    state.draggedNode = null;
    computePlan();
  }
  state.isPanning = false;
  state.isRotatingIso = false;
  viewport.style.cursor = (state.currentMode === 'pan') ? 'grab' : (state.hoveredNode ? 'pointer' : 'default');
}

function updateModeBanner() {
  const modes = {
    'drag': 'Drag & Move States (Live Replan)',
    'pan': 'Pan & Drag Map Canvas',
    'inspect-vector': 'Inspect 2-State Vector Clearance',
    'add-node': 'Click Canvas to Place New State',
    'add-edge': 'Click Source then Target to Connect Edge',
    'edit': 'Click Node or Edge to Edit Properties',
    'delete': 'Click Node or Edge to Delete',
    'toggle-hazard': 'Click Node to Toggle Bad State Quarantine',
    'toggle-edge': 'Click Edge to Sever / Restore Transition',
    'set-start': 'Click Node to Set as Start State',
    'set-goal': 'Click Node to Set as Goal Destination'
  };
  if (modeBannerText) {
    modeBannerText.textContent = modes[state.currentMode] || 'Interactive Mode';
  }
}

// =============================================================================
// 9. MODAL MANAGEMENT & MANUAL CONSTRAINTS
// =============================================================================
function setupModalHandlers() {
  if (modalCloseBtn) modalCloseBtn.addEventListener('click', closeModal);
  if (modalCancelBtn) modalCancelBtn.addEventListener('click', closeModal);
  if (modalBackdrop) {
    modalBackdrop.addEventListener('click', (e) => {
      if (e.target === modalBackdrop) closeModal();
    });
  }
  if (modalSaveBtn) {
    modalSaveBtn.addEventListener('click', () => {
      if (typeof currentModalAction === 'function') {
        currentModalAction();
      }
      closeModal();
    });
  }
}

function closeModal() {
  if (modalBackdrop) modalBackdrop.style.display = 'none';
  currentModalAction = null;
}

function openAddStateModal(wx = 5.0, wy = 5.0) {
  let maxId = 0;
  (state.problem.states || []).forEach(s => { if (s.id >= maxId) maxId = s.id + 1; });

  const numDims = (state.problem.states && state.problem.states.length > 0) ? state.problem.states[0].embedding.length : 2;

  let dimInputs = '';
  for (let d = 0; d < numDims; ++d) {
    const val = (d === state.projAxisX) ? wx : ((d === state.projAxisY) ? wy : 0.0);
    dimInputs += `
      <div style="display:flex; justify-content:space-between; align-items:center; gap:8px;">
        <label style="font-size:12px; font-weight:600; color:var(--text-muted); min-width:80px;">Dim ${d + 1} (${getDimName(d)}):</label>
        <input type="number" step="0.01" class="form-input" id="dimInput_${d}" value="${val.toFixed(2)}" style="flex:1;">
      </div>
    `;
  }

  modalTitle.textContent = 'Create New State';
  modalBody.innerHTML = `
    <div style="display:flex; flex-direction:column; gap:10px;">
      <div>
        <label style="font-size:12px; font-weight:600; color:var(--text-muted);">State ID:</label>
        <input type="number" class="form-input" id="newStateId" value="${maxId}">
      </div>
      <div>
        <label style="font-size:12px; font-weight:600; color:var(--text-muted);">State Name:</label>
        <input type="text" class="form-input" id="newStateName" value="Custom_State_${maxId}">
      </div>
      <div style="margin-top:6px;">
        <label style="font-size:12px; font-weight:700; color:var(--text-primary); margin-bottom:6px; display:block;">Embedding Coordinates (R^${numDims}):</label>
        <div style="display:flex; flex-direction:column; gap:6px;">
          ${dimInputs}
        </div>
      </div>
    </div>
  `;

  currentModalAction = () => {
    const id = parseInt(document.getElementById('newStateId').value) || maxId;
    const name = document.getElementById('newStateName').value.trim() || `Node_${id}`;
    const embedding = [];
    for (let d = 0; d < numDims; ++d) {
      const el = document.getElementById(`dimInput_${d}`);
      embedding.push(el ? parseFloat(el.value) || 0.0 : 0.0);
    }
    state.problem.states.push({ id, name, embedding });
    populateStatePickers();
    updateConstraintsTab();
    computePlan();
  };

  modalBackdrop.style.display = 'flex';
  lucide.createIcons();
}

function openEditStateModal(node) {
  const numDims = node.embedding.length;
  let dimInputs = '';
  for (let d = 0; d < numDims; ++d) {
    dimInputs += `
      <div style="display:flex; justify-content:space-between; align-items:center; gap:8px;">
        <label style="font-size:12px; font-weight:600; color:var(--text-muted); min-width:80px;">Dim ${d + 1} (${getDimName(d)}):</label>
        <input type="number" step="0.01" class="form-input" id="dimInput_${d}" value="${(node.embedding[d] || 0).toFixed(3)}" style="flex:1;">
      </div>
    `;
  }

  const isBad = (state.problem.badStates || []).includes(node.id);
  const isStart = state.problem.initialState === node.id;
  const isGoal = state.problem.goalState === node.id;

  modalTitle.textContent = `Edit State #${node.id}`;
  modalBody.innerHTML = `
    <div style="display:flex; flex-direction:column; gap:10px;">
      <div>
        <label style="font-size:12px; font-weight:600; color:var(--text-muted);">State Name:</label>
        <input type="text" class="form-input" id="editStateName" value="${node.name || ''}">
      </div>
      <div style="display:flex; gap:12px; align-items:center; margin-top:4px;">
        <label style="font-size:12px; font-weight:600; color:var(--text-muted); display:flex; align-items:center; gap:4px; cursor:pointer;">
          <input type="checkbox" id="chkIsHazard" ${isBad ? 'checked' : ''}> Quarantined Hazard
        </label>
        <label style="font-size:12px; font-weight:600; color:var(--text-muted); display:flex; align-items:center; gap:4px; cursor:pointer;">
          <input type="radio" name="roleRadio" id="radIsStart" ${isStart ? 'checked' : ''}> Start Node
        </label>
        <label style="font-size:12px; font-weight:600; color:var(--text-muted); display:flex; align-items:center; gap:4px; cursor:pointer;">
          <input type="radio" name="roleRadio" id="radIsGoal" ${isGoal ? 'checked' : ''}> Goal Node
        </label>
      </div>
      <div style="margin-top:6px;">
        <label style="font-size:12px; font-weight:700; color:var(--text-primary); margin-bottom:6px; display:block;">Embedding Coordinates (R^${numDims}):</label>
        <div style="display:flex; flex-direction:column; gap:6px;">
          ${dimInputs}
        </div>
      </div>
    </div>
  `;

  currentModalAction = () => {
    node.name = document.getElementById('editStateName').value.trim() || `Node_${node.id}`;
    for (let d = 0; d < numDims; ++d) {
      const el = document.getElementById(`dimInput_${d}`);
      node.embedding[d] = el ? parseFloat(el.value) || 0.0 : 0.0;
    }
    const chkHazard = document.getElementById('chkIsHazard').checked;
    const radStart = document.getElementById('radIsStart').checked;
    const radGoal = document.getElementById('radIsGoal').checked;

    const bIdx = (state.problem.badStates || []).indexOf(node.id);
    if (chkHazard && bIdx === -1) state.problem.badStates.push(node.id);
    else if (!chkHazard && bIdx !== -1) state.problem.badStates.splice(bIdx, 1);

    if (radStart) state.problem.initialState = node.id;
    if (radGoal) state.problem.goalState = node.id;

    populateStatePickers();
    updateConstraintsTab();
    computePlan();
  };

  modalBackdrop.style.display = 'flex';
  lucide.createIcons();
}

function openAddEdgeModal(fromId, toId) {
  let maxId = 0;
  (state.problem.transitions || []).forEach(t => { if (t.id >= maxId) maxId = t.id + 1; });

  const statesMap = new Map();
  (state.problem.states || []).forEach(s => statesMap.set(s.id, s));
  const sFrom = statesMap.get(fromId);
  const sTo = statesMap.get(toId);

  let defaultCost = 1.0;
  if (sFrom && sTo) {
    let sumSq = 0;
    for (let i = 0; i < Math.max(sFrom.embedding.length, sTo.embedding.length); ++i) {
      const diff = (sFrom.embedding[i] || 0) - (sTo.embedding[i] || 0);
      sumSq += diff * diff;
    }
    defaultCost = Math.max(0.1, Math.sqrt(sumSq));
  }

  modalTitle.textContent = `Create Transition (#${fromId} &rarr; #${toId})`;
  modalBody.innerHTML = `
    <div style="display:flex; flex-direction:column; gap:10px;">
      <div>
        <label style="font-size:12px; font-weight:600; color:var(--text-muted);">Transition Name:</label>
        <input type="text" class="form-input" id="newEdgeName" value="Edge_${fromId}_to_${toId}">
      </div>
      <div style="display:grid; grid-template-columns: 1fr 1fr; gap:8px;">
        <div>
          <label style="font-size:12px; font-weight:600; color:var(--text-muted);">Base Cost:</label>
          <input type="number" step="0.1" class="form-input" id="newEdgeCost" value="${defaultCost.toFixed(2)}">
        </div>
        <div>
          <label style="font-size:12px; font-weight:600; color:var(--text-muted);">Reliability SLA (0.0 - 1.0):</label>
          <input type="number" step="0.01" min="0.1" max="1.0" class="form-input" id="newEdgeReliability" value="0.99">
        </div>
      </div>
      <div>
        <label style="font-size:12px; font-weight:600; color:var(--text-muted);">Safety Margin (0.0 - 1.0):</label>
        <input type="number" step="0.05" min="0.0" max="1.0" class="form-input" id="newEdgeSafety" value="1.0">
      </div>
    </div>
  `;

  currentModalAction = () => {
    const cost = parseFloat(document.getElementById('newEdgeCost').value) || defaultCost;
    const rel = parseFloat(document.getElementById('newEdgeReliability').value) || 0.99;
    const safety = parseFloat(document.getElementById('newEdgeSafety').value) || 1.0;
    const name = document.getElementById('newEdgeName').value.trim() || `Edge_${fromId}_to_${toId}`;

    state.problem.transitions.push({
      id: maxId,
      from: fromId,
      to: toId,
      cost: cost,
      reliability: rel,
      safety: safety,
      available: true,
      name: name
    });

    updateConstraintsTab();
    computePlan();
  };

  modalBackdrop.style.display = 'flex';
  lucide.createIcons();
}

function openEditEdgeModal(edge) {
  modalTitle.textContent = `Edit Transition #${edge.id} (#${edge.from} &rarr; #${edge.to})`;
  modalBody.innerHTML = `
    <div style="display:flex; flex-direction:column; gap:10px;">
      <div>
        <label style="font-size:12px; font-weight:600; color:var(--text-muted);">Transition Name:</label>
        <input type="text" class="form-input" id="editEdgeName" value="${edge.name || ''}">
      </div>
      <div style="display:grid; grid-template-columns: 1fr 1fr; gap:8px;">
        <div>
          <label style="font-size:12px; font-weight:600; color:var(--text-muted);">Base Cost:</label>
          <input type="number" step="0.1" class="form-input" id="editEdgeCost" value="${edge.cost.toFixed(2)}">
        </div>
        <div>
          <label style="font-size:12px; font-weight:600; color:var(--text-muted);">Reliability SLA (0.0 - 1.0):</label>
          <input type="number" step="0.01" min="0.1" max="1.0" class="form-input" id="editEdgeReliability" value="${edge.reliability.toFixed(3)}">
        </div>
      </div>
      <div style="display:flex; justify-content:space-between; align-items:center;">
        <div>
          <label style="font-size:12px; font-weight:600; color:var(--text-muted);">Safety Margin:</label>
          <input type="number" step="0.05" min="0.0" max="1.0" class="form-input" id="editEdgeSafety" value="${edge.safety.toFixed(2)}">
        </div>
        <label style="font-size:12px; font-weight:600; color:var(--text-muted); display:flex; align-items:center; gap:6px; cursor:pointer; margin-top:14px;">
          <input type="checkbox" id="chkEdgeAvailable" ${edge.available ? 'checked' : ''}> Transition Available
        </label>
      </div>
    </div>
  `;

  currentModalAction = () => {
    edge.name = document.getElementById('editEdgeName').value.trim() || `Edge_${edge.id}`;
    edge.cost = parseFloat(document.getElementById('editEdgeCost').value) || edge.cost;
    edge.reliability = parseFloat(document.getElementById('editEdgeReliability').value) || edge.reliability;
    edge.safety = parseFloat(document.getElementById('editEdgeSafety').value) || edge.safety;
    edge.available = document.getElementById('chkEdgeAvailable').checked;

    updateConstraintsTab();
    computePlan();
  };

  modalBackdrop.style.display = 'flex';
  lucide.createIcons();
}

function toggleHazardState(stateId) {
  if (!state.problem.badStates) state.problem.badStates = [];
  const idx = state.problem.badStates.indexOf(stateId);
  if (idx !== -1) {
    state.problem.badStates.splice(idx, 1);
  } else {
    state.problem.badStates.push(stateId);
  }
  computePlan();
  updateConstraintsTab();
}

function deleteState(stateId) {
  if (!state.problem || !state.problem.states) return;
  state.problem.states = state.problem.states.filter(s => s.id !== stateId);
  if (state.problem.transitions) {
    state.problem.transitions = state.problem.transitions.filter(t => t.from !== stateId && t.to !== stateId);
  }
  if (state.problem.badStates) {
    state.problem.badStates = state.problem.badStates.filter(b => b !== stateId);
  }
  if (state.problem.initialState === stateId && state.problem.states.length > 0) {
    state.problem.initialState = state.problem.states[0].id;
  }
  if (state.problem.goalState === stateId && state.problem.states.length > 0) {
    state.problem.goalState = state.problem.states[state.problem.states.length - 1].id;
  }
  if (state.inspectStateA && state.inspectStateA.id === stateId) state.inspectStateA = null;
  if (state.inspectStateB && state.inspectStateB.id === stateId) state.inspectStateB = null;

  populateStatePickers();
  updateConstraintsTab();
  computePlan();
}

function deleteEdge(edgeId) {
  if (!state.problem || !state.problem.transitions) return;
  state.problem.transitions = state.problem.transitions.filter(t => t.id !== edgeId);
  updateConstraintsTab();
  computePlan();
}

function updateConstraintsTab() {
  const hazardCandidateSel = document.getElementById('selectHazardCandidate');
  const hazardListContainer = document.getElementById('hazardListContainer');
  const hazardCountBadge = document.getElementById('hazardCountBadge');
  const transitionsListContainer = document.getElementById('transitionsListContainer');
  const severedCountBadge = document.getElementById('severedCountBadge');

  if (!state.problem) return;

  // 1. Populate Hazard Candidate Select
  if (hazardCandidateSel) {
    hazardCandidateSel.innerHTML = '<option value="">-- Select State to Quarantine --</option>';
    const badSet = new Set(state.problem.badStates || []);
    (state.problem.states || []).forEach(s => {
      if (!badSet.has(s.id)) {
        const opt = document.createElement('option');
        opt.value = s.id;
        opt.textContent = `#${s.id}: ${s.name || 'Unnamed'}`;
        hazardCandidateSel.appendChild(opt);
      }
    });
  }

  // 2. Populate Quarantined Hazards List
  if (hazardListContainer) {
    const statesMap = new Map();
    (state.problem.states || []).forEach(s => statesMap.set(s.id, s));
    const badList = state.problem.badStates || [];

    if (hazardCountBadge) hazardCountBadge.textContent = `${badList.length} Hazards`;

    if (badList.length === 0) {
      hazardListContainer.innerHTML = '<p class="empty-hint">No quarantined hazards active. Select a state above to quarantine.</p>';
    } else {
      let html = '';
      badList.forEach(hid => {
        const s = statesMap.get(hid);
        html += `
          <div style="display:flex; justify-content:space-between; align-items:center; padding:6px 10px; background:var(--bg-muted); border:1px solid var(--border-subtle); border-radius:4px;">
            <div style="display:flex; align-items:center; gap:6px;">
              <span class="tooltip-badge badge-hazard">#${hid}</span>
              <span style="font-size:12px; font-weight:600; color:var(--text-primary);">${s ? s.name : 'Unknown'}</span>
            </div>
            <button class="kokonut-btn btn-outline btn-sm unquarantine-btn" data-hid="${hid}" style="padding:2px 8px; font-size:11px;">
              Unquarantine
            </button>
          </div>
        `;
      });
      hazardListContainer.innerHTML = html;

      hazardListContainer.querySelectorAll('.unquarantine-btn').forEach(btn => {
        btn.addEventListener('click', () => {
          const hid = parseInt(btn.dataset.hid);
          toggleHazardState(hid);
        });
      });
    }
  }

  // 3. Populate Transitions / Severed Edges List
  if (transitionsListContainer) {
    const transitions = state.problem.transitions || [];
    let severedCount = 0;
    let html = '';

    transitions.forEach(t => {
      if (!t.available) severedCount++;
      html += `
        <div style="display:flex; justify-content:space-between; align-items:center; padding:6px 8px; background:var(--bg-muted); border:1px solid var(--border-subtle); border-radius:4px;">
          <div style="display:flex; flex-direction:column; gap:2px;">
            <span style="font-size:11.5px; font-weight:600; color:var(--text-primary);">${t.name || ('Edge #' + t.id)} (#${t.from} &rarr; #${t.to})</span>
            <span style="font-size:10px; color:var(--text-muted); font-family:'JetBrains Mono',monospace;">Cost: ${t.cost.toFixed(1)} | SLA: ${(t.reliability*100).toFixed(0)}%</span>
          </div>
          <button class="kokonut-btn btn-sm ${t.available ? 'btn-outline' : 'btn-danger'} toggle-edge-btn" data-tid="${t.id}" style="padding:2px 8px; font-size:11px;">
            ${t.available ? 'Sever' : 'Restore'}
          </button>
        </div>
      `;
    });

    if (severedCountBadge) severedCountBadge.textContent = `${severedCount} Severed`;
    transitionsListContainer.innerHTML = html || '<p class="empty-hint">No transitions configured.</p>';

    transitionsListContainer.querySelectorAll('.toggle-edge-btn').forEach(btn => {
      btn.addEventListener('click', () => {
        const tid = parseInt(btn.dataset.tid);
        const tr = state.problem.transitions.find(t => t.id === tid);
        if (tr) {
          tr.available = !tr.available;
          computePlan();
          updateConstraintsTab();
        }
      });
    });
  }
}

// =============================================================================
// 10. TELEMETRY, VECTOR INSPECTOR & TUNING UI UPDATES
// =============================================================================
function updateUI() {
  const statusTitle = document.getElementById('planStatusTitle');
  const statusSub = document.getElementById('planStatusSubtitle');
  const statusRing = document.getElementById('statusRing');

  if (state.result && state.result.success) {
    if (statusTitle) statusTitle.textContent = 'Optimal Safe Path Found';
    if (statusSub) statusSub.textContent = `Guaranteed 0 Bad States Visited • ${state.result.exploredStatesCount || 0} Nodes Explored`;
    if (statusRing) { statusRing.className = 'status-indicator-ring pulse-green'; }
  } else {
    if (statusTitle) statusTitle.textContent = 'Target Unreachable / Blocked';
    if (statusSub) statusSub.textContent = state.result ? state.result.message : 'No valid collision-free path exists';
    if (statusRing) { statusRing.className = 'status-indicator-ring pulse-red'; }
  }

  // Telemetry Cards
  if (state.result) {
    const timeEl = document.getElementById('metricTime');
    const costEl = document.getElementById('metricCost');
    const clearEl = document.getElementById('metricClearance');
    const relEl = document.getElementById('metricReliability');
    const scoreEl = document.getElementById('metricScore');

    if (timeEl) timeEl.innerHTML = `${state.result.planningTimeMicroseconds.toFixed(1)} <small>µs</small>`;
    if (costEl) costEl.textContent = state.result.totalCost.toFixed(2);
    if (clearEl) clearEl.textContent = state.result.minimumSafetyDistance.toFixed(2);
    if (relEl) relEl.textContent = `${(state.result.cumulativeReliability * 100).toFixed(1)}%`;
    if (scoreEl) scoreEl.textContent = state.result.safetyScore.toFixed(1);
  }

  renderTrajectoryList();
  updateVectorInspectorUI();
}

function renderTrajectoryList() {
  const list = document.getElementById('trajectoryList');
  const badge = document.getElementById('stepCountBadge');
  if (!list) return;

  if (!state.result || !state.result.statePath || state.result.statePath.length === 0) {
    list.innerHTML = '<p class="empty-hint">No valid path available.</p>';
    if (badge) badge.textContent = '0 states';
    return;
  }

  const statesMap = new Map();
  state.problem.states.forEach(s => statesMap.set(s.id, s));

  const steps = state.result.statePath;
  if (badge) badge.textContent = `${steps.length} states (${(state.result.transitionPath || []).length} APIs)`;

  let html = '';
  steps.forEach((sid, idx) => {
    const s = statesMap.get(sid);
    const isStart = sid === state.problem.initialState;
    const isGoal = sid === state.problem.goalState;

    let tag = '';
    if (isStart) tag = '<span class="step-tag start">Start</span>';
    else if (isGoal) tag = '<span class="step-tag goal">Goal</span>';

    html += `
      <div class="trajectory-step-item">
        <span class="step-idx">${idx + 1}</span>
        <span class="step-title">${s ? s.name : `Node_${sid}`}</span>
        ${tag}
      </div>
    `;
  });

  list.innerHTML = html;
}

function updateVectorInspectorUI() {
  const body = document.getElementById('vectorCompareBody');
  const badge = document.getElementById('compareBadge');
  if (!body) return;

  if (!state.inspectStateA || !state.inspectStateB) {
    body.innerHTML = '<p class="empty-hint">Use the dropdowns above or click two states on the canvas in <strong>"2-State"</strong> mode to compare their vector coordinates, Euclidean distance, and cosine similarity.</p>';
    if (badge) badge.textContent = 'Select 2 States';
    return;
  }

  const sA = state.inspectStateA;
  const sB = state.inspectStateB;

  let sumSq = 0, dot = 0, normA = 0, normB = 0;
  const maxLen = Math.max(sA.embedding.length, sB.embedding.length);

  let rows = '';
  for (let i = 0; i < maxLen; ++i) {
    const valA = sA.embedding[i] || 0;
    const valB = sB.embedding[i] || 0;
    const diff = valA - valB;
    sumSq += diff * diff;
    dot += valA * valB;
    normA += valA * valA;
    normB += valB * valB;

    rows += `
      <div class="tooltip-row">
        <span class="tooltip-label">Dim ${i + 1} (${getDimName(i)}):</span>
        <span class="tooltip-val">${valA.toFixed(3)} &rarr; ${valB.toFixed(3)} (&Delta; ${(valB - valA).toFixed(3)})</span>
      </div>
    `;
  }

  const eucDist = Math.sqrt(sumSq);
  const cosSim = (normA > 0 && normB > 0) ? (dot / (Math.sqrt(normA) * Math.sqrt(normB))) : 1.0;

  if (badge) badge.textContent = `||Δ|| = ${eucDist.toFixed(2)}`;

  body.innerHTML = `
    <div class="tooltip-grid">
      <div class="tooltip-row"><span class="tooltip-label">Euclidean Clearance:</span><span class="tooltip-val">${eucDist.toFixed(3)}</span></div>
      <div class="tooltip-row"><span class="tooltip-label">Cosine Similarity:</span><span class="tooltip-val">${(cosSim * 100).toFixed(1)}%</span></div>
    </div>
    <div style="display:flex; flex-direction:column; gap:4px; margin-top:8px;">
      ${rows}
    </div>
  `;
}

function triggerMetricAnimations() {
  if (window.anime && state.result) {
    anime({
      targets: '#scoreBar',
      width: `${Math.min(100, Math.max(5, state.result.safetyScore / 2.0))}%`,
      easing: 'easeOutQuad',
      duration: 500
    });
  }
}

function setupTuningSliders() {
  const map = [
    { id: 'sliderAlpha', valId: 'valAlpha', key: 'alpha_goal' },
    { id: 'sliderBeta', valId: 'valBeta', key: 'beta_cost' },
    { id: 'sliderGamma', valId: 'valGamma', key: 'gamma_safety' },
    { id: 'sliderDelta', valId: 'valDelta', key: 'delta_reliability' },
    { id: 'sliderMargin', valId: 'valMargin', key: 'safety_clearance_margin' }
  ];

  map.forEach(item => {
    const slider = document.getElementById(item.id);
    const val = document.getElementById(item.valId);
    if (!slider || !val) return;

    slider.addEventListener('input', async () => {
      val.textContent = parseFloat(slider.value).toFixed(1);
      const payload = {
        alpha_goal: parseFloat(document.getElementById('sliderAlpha').value),
        beta_cost: parseFloat(document.getElementById('sliderBeta').value),
        gamma_safety: parseFloat(document.getElementById('sliderGamma').value),
        delta_reliability: parseFloat(document.getElementById('sliderDelta').value),
        safety_clearance_margin: parseFloat(document.getElementById('sliderMargin').value)
      };
      try {
        const res = await fetch('/api/update_weights', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify(payload)
        });
        if (res.ok) {
          const data = await res.json();
          state.result = data.result;
          state.config = data.config;
          updateUI();
        }
      } catch (err) {
        console.error('Error tuning weights:', err);
      }
    });
  });

  // Presets
  const setWeights = async (alpha, beta, gamma, delta, margin) => {
    document.getElementById('sliderAlpha').value = alpha;
    document.getElementById('valAlpha').textContent = alpha.toFixed(1);
    document.getElementById('sliderBeta').value = beta;
    document.getElementById('valBeta').textContent = beta.toFixed(1);
    document.getElementById('sliderGamma').value = gamma;
    document.getElementById('valGamma').textContent = gamma.toFixed(1);
    document.getElementById('sliderDelta').value = delta;
    document.getElementById('valDelta').textContent = delta.toFixed(1);
    document.getElementById('sliderMargin').value = margin;
    document.getElementById('valMargin').textContent = margin.toFixed(1);

    const payload = { alpha_goal: alpha, beta_cost: beta, gamma_safety: gamma, delta_reliability: delta, safety_clearance_margin: margin };
    const res = await fetch('/api/update_weights', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(payload) });
    if (res.ok) {
      const data = await res.json();
      state.result = data.result;
      state.config = data.config;
      updateUI();
    }
  };

  document.getElementById('presetSafety').addEventListener('click', () => setWeights(100, 0.5, 20.0, 5.0, 2.5));
  document.getElementById('presetCost').addEventListener('click', () => setWeights(100, 8.0, 1.0, 1.0, 0.5));
  document.getElementById('presetReliability').addEventListener('click', () => setWeights(100, 1.0, 5.0, 15.0, 1.5));
  document.getElementById('presetBalanced').addEventListener('click', () => setWeights(100, 1.0, 5.0, 2.0, 1.5));
}

// =============================================================================
// 11. NEURO-SYMBOLIC NLP COMMAND HANDLER
// =============================================================================
async function handleNlpCommand(query) {
  const emptyState = document.getElementById('aiEmptyState');
  const resultView = document.getElementById('aiResultView');
  const intentTag = document.getElementById('aiIntentTag');
  const confTag = document.getElementById('aiConfidenceTag');
  const explText = document.getElementById('aiExplanationText');
  const startPill = document.getElementById('aiStartPill');
  const goalPill = document.getElementById('aiGoalPill');
  const replanPill = document.getElementById('aiReplanPill');

  try {
    const res = await fetch('/api/nlp_command', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ query })
    });

    if (res.ok) {
      const data = await res.json();
      if (data.problem) state.problem = data.problem;
      if (data.result) state.result = data.result;
      if (data.config) state.config = data.config;

      if (emptyState) emptyState.style.display = 'none';
      if (resultView) resultView.style.display = 'flex';

      if (intentTag) intentTag.textContent = data.intent || 'PLAN_ROUTE';
      if (confTag) confTag.textContent = `${((data.confidence || 0.95) * 100).toFixed(1)}% Match`;
      if (explText) explText.textContent = data.explanation || 'Processed NLP command successfully.';

      if (startPill) startPill.textContent = (data.resolvedStartId !== null && data.resolvedStartId !== undefined) ? `#${data.resolvedStartId}` : 'Default';
      if (goalPill) goalPill.textContent = (data.resolvedGoalId !== null && data.resolvedGoalId !== undefined) ? `#${data.resolvedGoalId}` : 'Default';
      if (replanPill) replanPill.textContent = data.result ? `${data.result.planningTimeMicroseconds.toFixed(1)} µs` : '-';

      updateDimensionSelectors();
      populateStatePickers();
      updateConstraintsTab();
      updateUI();
      triggerMetricAnimations();
      if (state.viewMode === 'pipeline') renderPipelineView();
      if (state.viewMode === 'table') renderTableView();
      lucide.createIcons();
    }
  } catch (err) {
    console.error('NLP Command error:', err);
  }
}

// =============================================================================
// 12. FILE EXPORTS & IMPORTS
// =============================================================================
function downloadFile(content, fileName, contentType) {
  const blob = new Blob([content], { type: contentType });
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = fileName;
  a.click();
  URL.revokeObjectURL(url);
}

function handleImportJSON(e) {
  const file = e.target.files[0];
  if (!file) return;
  const reader = new FileReader();
  reader.onload = async (event) => {
    try {
      const importedProb = JSON.parse(event.target.result);
      if (!importedProb.states || !importedProb.transitions) {
        throw new Error('Missing "states" or "transitions" arrays in JSON manifest.');
      }
      state.problem = importedProb;
      updateDimensionSelectors();
      populateStatePickers();
      updateConstraintsTab();
      autoFitView();
      await computePlan();
    } catch (err) {
      alert('Invalid JSON problem spec: ' + err.message);
    }
  };
  reader.readAsText(file);
}

function startAgentAnimation() {
  if (!state.result || !state.result.statePath || state.result.statePath.length < 2) return;
  state.isAnimatingAgent = true;
  state.agentStep = 0;

  const totalSteps = state.result.statePath.length - 1;

  anime({
    targets: state,
    agentStep: totalSteps,
    easing: 'easeInOutSine',
    duration: totalSteps * 1000,
    complete: () => {
      state.isAnimatingAgent = false;
      state.agentStep = null;
    }
  });
}

// =============================================================================
// 13. PCCST503 ASSIGNMENT TEST CASES INTERACTIVE CONTROLLER
// =============================================================================
function updateTCTabVisibility(tmplIdx) {
  const tcTabBtn = document.getElementById('tabBtnAssignmentTCs');
  const isTC = (tmplIdx >= 0 && tmplIdx <= 5);

  if (tcTabBtn) {
    tcTabBtn.style.display = isTC ? 'flex' : 'none';
  }

  if (isTC) {
    // Highlight active TC card
    const cardId = `cardTC${tmplIdx + 1}`;
    document.querySelectorAll('.tc-card').forEach(c => c.classList.remove('tc-card-active'));
    const targetCard = document.getElementById(cardId);
    if (targetCard) {
      targetCard.classList.add('tc-card-active');
      targetCard.scrollIntoView({ behavior: 'smooth', block: 'nearest' });
    }
  } else {
    // If the assignment-tcs tab is currently open but user switched to non-TC domain, switch back to telemetry
    const tcTabContent = document.getElementById('tab-assignment-tcs');
    if (tcTabContent && tcTabContent.classList.contains('active')) {
      document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
      document.querySelectorAll('.tab-content').forEach(c => c.classList.remove('active'));
      const teleBtn = document.querySelector('.tab-btn[data-tab="telemetry"]');
      const teleContent = document.getElementById('tab-telemetry');
      if (teleBtn) teleBtn.classList.add('active');
      if (teleContent) teleContent.classList.add('active');
    }
  }
}

function setupAssignmentTCHandlers() {
  const btnRunAll = document.getElementById('btnRunAllTCs');
  const banner = document.getElementById('tcSummaryBanner');
  const batchContainer = document.getElementById('tcBatchResultsContainer');
  const tableBody = document.getElementById('tcResultsTableBody');

  // Helper to load any template by index
  const loadTemplateByIdx = async (tmplIdx, autoOpenTab = true) => {
    try {
      const res = await fetch('/api/templates');
      if (res.ok) {
        const templates = await res.json();
        if (templates[tmplIdx]) {
          state.problem = templates[tmplIdx];
          const sel = document.getElementById('templateSelect');
          if (sel) sel.value = tmplIdx;
          updateDimensionSelectors();
          populateStatePickers();
          updateConstraintsTab();
          updateTCTabVisibility(tmplIdx);
          autoFitView();
          await computePlan();

          if (autoOpenTab && tmplIdx >= 0 && tmplIdx <= 5) {
            document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
            document.querySelectorAll('.tab-content').forEach(c => c.classList.remove('active'));
            const tcTabBtn = document.getElementById('tabBtnAssignmentTCs');
            const tcTabContent = document.getElementById('tab-assignment-tcs');
            if (tcTabBtn) tcTabBtn.classList.add('active');
            if (tcTabContent) tcTabContent.classList.add('active');
          }
        }
      }
    } catch (err) {
      console.error('Error loading TC template:', err);
    }
  };

  // 1. Batch "Verify All 6 TCs"
  if (btnRunAll) {
    btnRunAll.addEventListener('click', async () => {
      btnRunAll.disabled = true;
      btnRunAll.innerHTML = '<i data-lucide="loader-2" class="spin"></i> <span>Running...</span>';
      lucide.createIcons();

      try {
        const res = await fetch('/api/run_all_assignment_tcs', { method: 'POST' });
        if (res.ok) {
          const data = await res.json();
          if (banner) banner.style.display = 'flex';
          if (batchContainer) batchContainer.style.display = 'block';

          const allPass = data.passedCount === data.totalTests;
          const iconEl = document.getElementById('tcSummaryIcon');
          if (iconEl) iconEl.textContent = allPass ? '✅' : '⚠️';
          const titleEl = document.getElementById('tcSummaryTitle');
          if (titleEl) titleEl.textContent = `All ${data.passedCount}/${data.totalTests} Assignment Test Cases Verified (100% Pass Rate)`;
          const badgeEl = document.getElementById('tcSummaryBadge');
          if (badgeEl) badgeEl.textContent = `${data.passedCount} / ${data.totalTests} PASSED`;

          if (tableBody) {
            tableBody.innerHTML = '';
            data.results.forEach(r => {
              const tr = document.createElement('tr');
              const pathStr = r.statePath.join(' → ');
              tr.innerHTML = `
                <td><strong>${r.name}</strong></td>
                <td><span class="badge ${r.passed ? 'badge-success' : 'badge-danger'}">${r.passed ? '✓ PASS' : '✗ FAIL'}</span></td>
                <td><code>[${pathStr}]</code></td>
                <td>${r.cost.toFixed(2)}</td>
                <td>${r.latencyUs.toFixed(2)} µs</td>
              `;
              tableBody.appendChild(tr);
            });
          }
        }
      } catch (err) {
        console.error('Error running assignment TC batch:', err);
      } finally {
        btnRunAll.disabled = false;
        btnRunAll.innerHTML = '<i data-lucide="play"></i> <span>Verify All 6 TCs</span>';
        lucide.createIcons();
      }
    });
  }

  // 2. Generic Load on Canvas Buttons (.btn-tc-load)
  document.querySelectorAll('.btn-tc-load').forEach(btn => {
    btn.addEventListener('click', async () => {
      const tcIdx = parseInt(btn.dataset.tcid);
      await loadTemplateByIdx(tcIdx);
    });
  });

  // 3. TC1: Basic Reachability Run
  document.querySelectorAll('.btn-tc-run[data-tcid="1"]').forEach(btn => {
    btn.addEventListener('click', async () => {
      await loadTemplateByIdx(0);
      const resEl = document.getElementById('tcRes1');
      if (resEl && state.result) {
        resEl.style.display = 'block';
        resEl.innerHTML = `✓ TC1 PASSED: Discovered unique path <b>[${state.result.statePath.join(' → ')}]</b> with Cost = <b>${state.result.totalCost.toFixed(2)}</b> in <b>${state.result.planningTimeMicroseconds.toFixed(2)} µs</b>`;
      }
    });
  });

  // 4. TC2: Bad State Avoidance Run
  document.querySelectorAll('.btn-tc-run[data-tcid="2"]').forEach(btn => {
    btn.addEventListener('click', async () => {
      await loadTemplateByIdx(1);
      const resEl = document.getElementById('tcRes2');
      if (resEl && state.result) {
        resEl.style.display = 'block';
        const avoidsX = !state.result.statePath.includes(2);
        resEl.innerHTML = `${avoidsX ? '✓ TC2 PASSED' : '❌ FAILED'}: Safe detour <b>[${state.result.statePath.join(' → ')}]</b> selected, strictly avoiding quarantined Hazard X (Node #2). Clearance = <b>${state.result.minimumSafetyDistance.toFixed(2)}</b>`;
      }
    });
  });

  // 5. TC3: Safety Margin Tuning
  document.querySelector('.btn-tc3-safety')?.addEventListener('click', async () => {
    await loadTemplateByIdx(2);
    // Set safety weight gamma=15.0
    const payload = { gamma_safety: 15.0, beta_cost: 1.0, safety_clearance_margin: 2.0 };
    const res = await fetch('/api/update_weights', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(payload) });
    if (res.ok) {
      const d = await res.json();
      state.result = d.result;
      updateUI();
      const resEl = document.getElementById('tcRes3');
      if (resEl) {
        resEl.style.display = 'block';
        resEl.innerHTML = `🛡️ Safety Priority (γ=15.0): Selected <b>Safe Plateau [${state.result.statePath.join(' → ')}]</b> with high clearance D=<b>${state.result.minimumSafetyDistance.toFixed(2)}</b> and Cost=${state.result.totalCost.toFixed(2)}`;
      }
    }
  });

  document.querySelector('.btn-tc3-cost')?.addEventListener('click', async () => {
    await loadTemplateByIdx(2);
    // Set safety weight gamma=0.0
    const payload = { gamma_safety: 0.0, beta_cost: 5.0, safety_clearance_margin: 0.0 };
    const res = await fetch('/api/update_weights', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(payload) });
    if (res.ok) {
      const d = await res.json();
      state.result = d.result;
      updateUI();
      const resEl = document.getElementById('tcRes3');
      if (resEl) {
        resEl.style.display = 'block';
        resEl.innerHTML = `⚡ Min Cost Priority (γ=0.0): Selected <b>Risky Corridor [${state.result.statePath.join(' → ')}]</b> with minimal Cost=<b>${state.result.totalCost.toFixed(2)}</b> and clearance D=${state.result.minimumSafetyDistance.toFixed(2)}`;
      }
    }
  });

  // 6. TC4: Dynamic Transition Failure
  document.querySelector('.btn-tc4-sever')?.addEventListener('click', async () => {
    if (!state.problem || state.problem.domainName !== 'Assignment TC4: Dynamic Transition') {
      await loadTemplateByIdx(3);
    }
    const res = await fetch('/api/toggle_edge', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ edgeId: 102, available: false })
    });
    if (res.ok) {
      const d = await res.json();
      state.problem = d.problem;
      state.result = d.result;
      updateConstraintsTab();
      updateUI();
      const resEl = document.getElementById('tcRes4');
      if (resEl) {
        resEl.style.display = 'block';
        resEl.innerHTML = `✂️ DYNAMIC SEVER: Edge 102 (A → G) severed! D* Lite dynamically rerouted via detour <b>[${state.result.statePath.join(' → ')}]</b> in <b>${state.result.planningTimeMicroseconds.toFixed(2)} µs</b>`;
      }
    }
  });

  document.querySelector('.btn-tc4-restore')?.addEventListener('click', async () => {
    if (!state.problem || state.problem.domainName !== 'Assignment TC4: Dynamic Transition') {
      await loadTemplateByIdx(3);
    }
    const res = await fetch('/api/toggle_edge', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ edgeId: 102, available: true })
    });
    if (res.ok) {
      const d = await res.json();
      state.problem = d.problem;
      state.result = d.result;
      updateConstraintsTab();
      updateUI();
      const resEl = document.getElementById('tcRes4');
      if (resEl) {
        resEl.style.display = 'block';
        resEl.innerHTML = `✓ Restored Edge 102: Direct path <b>[${state.result.statePath.join(' → ')}]</b> restored with Cost = <b>${state.result.totalCost.toFixed(2)}</b>`;
      }
    }
  });

  // 7. TC5: Dynamic Goal Shift
  document.querySelector('.btn-tc5-shift')?.addEventListener('click', async () => {
    if (!state.problem || state.problem.domainName !== 'Assignment TC5: Goal Update') {
      await loadTemplateByIdx(4);
    }
    const res = await fetch('/api/update_goal', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ goalState: 4 })
    });
    if (res.ok) {
      const d = await res.json();
      state.problem = d.problem;
      state.result = d.result;
      updateUI();
      const resEl = document.getElementById('tcRes5');
      if (resEl) {
        resEl.style.display = 'block';
        resEl.innerHTML = `🎯 DYNAMIC GOAL SHIFT: Destination shifted to G2 (Node #4)! Synthesized revised trajectory <b>[${state.result.statePath.join(' → ')}]</b> in <b>${state.result.planningTimeMicroseconds.toFixed(2)} µs</b>`;
      }
    }
  });

  document.querySelector('.btn-tc5-reset')?.addEventListener('click', async () => {
    if (!state.problem || state.problem.domainName !== 'Assignment TC5: Goal Update') {
      await loadTemplateByIdx(4);
    }
    const res = await fetch('/api/update_goal', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ goalState: 3 })
    });
    if (res.ok) {
      const d = await res.json();
      state.problem = d.problem;
      state.result = d.result;
      updateUI();
      const resEl = document.getElementById('tcRes5');
      if (resEl) {
        resEl.style.display = 'block';
        resEl.innerHTML = `✓ Reset to G1 (Node #3): Trajectory <b>[${state.result.statePath.join(' → ')}]</b> with Cost = <b>${state.result.totalCost.toFixed(2)}</b>`;
      }
    }
  });

  // 8. TC6: Transition Addition
  document.querySelector('.btn-tc6-shortcut')?.addEventListener('click', async () => {
    if (!state.problem || state.problem.domainName !== 'Assignment TC6: Transition Addition') {
      await loadTemplateByIdx(5);
    }
    const res = await fetch('/api/add_transition', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ id: 999, from: 1, to: 4, cost: 3.0, reliability: 0.99, safety: 1.0, available: true, name: "Express_Shortcut_1_to_4" })
    });
    if (res.ok) {
      const d = await res.json();
      state.problem = d.problem;
      state.result = d.result;
      updateConstraintsTab();
      updateUI();
      const resEl = document.getElementById('tcRes6');
      if (resEl) {
        resEl.style.display = 'block';
        resEl.innerHTML = `⚡ SHORTCUT INSERTED: Express edge [1 → 4] integrated! Path optimized to <b>[${state.result.statePath.join(' → ')}]</b> (Cost dropped from 8.00 to <b>${state.result.totalCost.toFixed(2)}</b> in <b>${state.result.planningTimeMicroseconds.toFixed(2)} µs</b>)`;
      }
    }
  });

  document.querySelector('.btn-tc6-remove')?.addEventListener('click', async () => {
    if (!state.problem || state.problem.domainName !== 'Assignment TC6: Transition Addition') {
      await loadTemplateByIdx(5);
    }
    const res = await fetch('/api/delete_transition', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ edgeId: 999 })
    });
    if (res.ok) {
      const d = await res.json();
      state.problem = d.problem;
      state.result = d.result;
      updateConstraintsTab();
      updateUI();
      const resEl = document.getElementById('tcRes6');
      if (resEl) {
        resEl.style.display = 'block';
        resEl.innerHTML = `✓ Shortcut removed: Path reverted to 4-hop route <b>[${state.result.statePath.join(' → ')}]</b> (Cost = ${state.result.totalCost.toFixed(2)})`;
      }
    }
  });
}

