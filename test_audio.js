const fs = require('fs');

// Mock Web Audio API
global.window = {};
global.window.AudioContext = class {
  createGain() { return { gain: { value: 0, linearRampToValueAtTime: ()=>{} }, connect: () => {} }; }
  createBufferSource() { return { start: () => {}, stop: () => {}, disconnect: () => {}, playbackRate: { value: 1.0 } }; }
  decodeAudioData() { return Promise.resolve({}); }
  get destination() { return {}; }
  get currentTime() { return 0; }
};

// Fake fetch
global.fetch = () => Promise.resolve({ ok: true, arrayBuffer: () => Promise.resolve(new ArrayBuffer(0)) });

// Run the file
import('./docs/simulator/js/audioMixer.js').then(async module => {
  try {
      const audio = new module.AudioMixer();
      await audio.init();
      console.log("AudioMixer initialized");
      audio.power(true);
      console.log("AudioMixer powered ON");
      process.exit(0);
  } catch(e) {
      console.error("Crash:", e);
      process.exit(1);
  }
}).catch(err => {
  console.error("Load Crash:", err);
  process.exit(1);
});
