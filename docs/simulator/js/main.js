import { PhysicsEngine } from './physicsEngine.js';
import { ThreeRenderer } from './threeRenderer.js';
import { AudioMixer } from './audioMixer.js';

// Initialization
const physics = new PhysicsEngine();
const renderer = new ThreeRenderer('canvas-container');
const audio = new AudioMixer();

// UI Elements
const uiG = document.getElementById('val-g');
const barG = document.getElementById('bar-g');
const uiAngle = document.getElementById('val-angle');
const uiTank = document.getElementById('val-tank');
const barTank = document.getElementById('bar-tank');
const uiHum = document.getElementById('val-hum');
const barHum = document.getElementById('bar-hum');
const uiSwingL = document.getElementById('val-swingL');
const barSwingL = document.getElementById('bar-swingL');
const uiSwingH = document.getElementById('val-swingH');
const barSwingH = document.getElementById('bar-swingH');
const uiState = document.getElementById('val-state');
const inputHue = document.getElementById('input-hue');

// Buttons
const btnPower = document.getElementById('btn-power');
const btnBlaster = document.getElementById('btn-blaster');
const btnDrag = document.getElementById('btn-drag');
const btnOverload = document.getElementById('btn-overload');

// Effect States
let isOn = false;
let isDraggingEffect = false;
let blasterActive = false;
let blasterTimer = null;
let manualBurstActive = false;
let manualBurstTimer = null;

// Input Handling for Simulation
document.addEventListener('pointerdown', (e) => {
    // Ignore clicks on UI elements
    if (e.target.closest('#ui-layer')) return;
    
    physics.handlePointerDown(e.clientX, e.clientY);
    audio.init();
});

document.addEventListener('pointermove', (e) => {
    physics.handlePointerMove(e.clientX, e.clientY);
});

document.addEventListener('pointerup', () => {
    physics.handlePointerUp();
});

// Button Event Listeners
btnPower?.addEventListener('click', async () => {
    await audio.init();
    isOn = !isOn;
    audio.power(isOn);
    
    if (isOn) {
        btnPower.innerText = 'Power OFF';
        btnPower.className = 'action-btn danger';
        if(btnBlaster) btnBlaster.disabled = false;
        if(btnDrag) btnDrag.disabled = false;
        if(btnOverload) btnOverload.disabled = false;
    } else {
        btnPower.innerText = 'Power ON';
        btnPower.className = 'action-btn';
        if(btnBlaster) btnBlaster.disabled = true;
        if(btnDrag) btnDrag.disabled = true;
        if(btnOverload) btnOverload.disabled = true;
        isDraggingEffect = false;
    }
});

btnBlaster?.addEventListener('click', () => {
    if (!isOn) return;
    audio.triggerBlaster();
    
    // Visual flash for 100ms
    blasterActive = true;
    if (blasterTimer) clearTimeout(blasterTimer);
    blasterTimer = setTimeout(() => { blasterActive = false; }, 100);
});

btnOverload?.addEventListener('click', () => {
    if (!isOn) return;
    physics.forceFillTank();
});

// Drag effect happens while holding down the button
function startDrag(e) {
    if (e) e.preventDefault(); // Prevent touch-click double firing
    if (!isOn) return;
    isDraggingEffect = true;
    audio.setDrag(true);
}

function stopDrag() {
    if (!isOn) return;
    isDraggingEffect = false;
    audio.setDrag(false);
}

btnDrag?.addEventListener('mousedown', startDrag);
btnDrag?.addEventListener('touchstart', startDrag);
btnDrag?.addEventListener('mouseup', stopDrag);
btnDrag?.addEventListener('touchend', stopDrag);
btnDrag?.addEventListener('mouseleave', stopDrag);

// Update UI
function updateUI(metrics) {
    uiG.innerText = metrics.energiaTotal.toFixed(1);
    // Map G-Force 0-8 to 0-100% for the bar
    barG.style.width = `${Math.min(100, (metrics.energiaTotal / 8.0) * 100)}%`;
    
    uiAngle.innerText = Math.round(metrics.bladeAngle);
    
    const tankPercent = Math.round(metrics.tanqueOverload * 100);
    uiTank.innerText = tankPercent;
    barTank.style.width = `${tankPercent}%`;
    
    // Audio volumes
    const vols = audio.getVolumes();
    
    const humPercent = Math.round(vols.hum * 100);
    if(uiHum) { uiHum.innerText = humPercent; barHum.style.width = `${humPercent}%`; }
    
    const swingLPercent = Math.round(vols.swingL * 100);
    if(uiSwingL) { uiSwingL.innerText = swingLPercent; barSwingL.style.width = `${swingLPercent}%`; }
    
    const swingHPercent = Math.round(vols.swingH * 100);
    if(uiSwingH) { uiSwingH.innerText = swingHPercent; barSwingH.style.width = `${swingHPercent}%`; }
    
    // State Text
    if (!isOn) {
        uiState.innerText = "OFF";
        uiState.className = "badge";
    } else if (metrics.isBursting) {
        uiState.innerText = "PLASMA RUPTURE!";
        uiState.className = "badge danger";
    } else if (metrics.energiaTotal > 4.0) {
        uiState.innerText = "H ZONE (HIGH ENERGY)";
        uiState.className = "badge active";
    } else if (metrics.energiaTotal > 2.0) {
        uiState.innerText = "TRANSITION";
        uiState.className = "badge active";
    } else if (metrics.energiaTotal > 0.5) {
        uiState.innerText = "L ZONE (LOW ENERGY)";
        uiState.className = "badge active";
    } else {
        uiState.innerText = "CALM (BREATHING)";
        uiState.className = "badge";
    }
}

// Main Game Loop
let lastTime = performance.now();

function animate() {
    requestAnimationFrame(animate);
    
    const now = performance.now();
    const dt = now - lastTime;
    lastTime = now;
    
    // Time in seconds for sine waves
    const timeSecs = now / 1000.0;
    
    // 1. Calculate Physics / Kinematics
    const metrics = physics.update(dt);
    
    // Inject new states into metrics for rendering and audio
    metrics.isOn = isOn;
    metrics.isDragging = isDraggingEffect;
    metrics.blasterActive = blasterActive;
    
    // 2. Read User Config
    const baseHue = parseInt(inputHue.value);
    
    // 3. Update Audio Engine
    audio.update(metrics);
    
    // 4. Render 3D Graphics
    renderer.update(metrics, baseHue, timeSecs);
    
    // 5. Update Telemetry UI
    updateUI(metrics);
}

// Start loop
animate();
