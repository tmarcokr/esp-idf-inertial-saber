const fs = require('fs');
const jsdom = require("jsdom");
const { JSDOM } = jsdom;
const dom = new JSDOM(fs.readFileSync('docs/simulator/index.html', 'utf8'));
global.document = dom.window.document;
global.window = dom.window;
global.performance = { now: () => 0 };
global.requestAnimationFrame = () => {};

// Mock Three.js
global.THREE = {
  WebGLRenderer: class { setSize(){} setPixelRatio(){} setAnimationLoop(){} render(){} },
  Scene: class { add(){} },
  PerspectiveCamera: class { position = {set:()=>{}, z:0}; lookAt(){} },
  DirectionalLight: class { position = {set:()=>{}} },
  AmbientLight: class {},
  Mesh: class { rotation = {x:0, y:0, z:0}; position = {y:0} },
  CylinderGeometry: class {},
  MeshStandardMaterial: class { color = {setHSL:()=>{}}; emissive = {setHSL:()=>{}} }
};

// Mock AudioContext
global.AudioContext = class {
  createGain() { return { gain: { value: 0 }, connect: () => {} }; }
  createBufferSource() { return { start: () => {}, connect: () => {} }; }
  decodeAudioData() { return Promise.resolve({}); }
};
global.window.AudioContext = global.AudioContext;

// Fake fetch
global.fetch = () => Promise.resolve({ ok: true, arrayBuffer: () => Promise.resolve(new ArrayBuffer(0)) });

// Run the file
import('./docs/simulator/js/main.js').then(module => {
  console.log("Loaded successfully");
  setTimeout(() => {
    console.log("No crash after 100ms");
    process.exit(0);
  }, 100);
}).catch(err => {
  console.error("Crash:", err);
  process.exit(1);
});
