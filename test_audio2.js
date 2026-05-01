import fs from 'fs';

// Mock Web Audio API
global.window = {};
global.window.AudioContext = class {
  createGain() { return { gain: { value: 0, linearRampToValueAtTime: ()=>{} }, connect: () => {} }; }
  createBufferSource() { return { start: () => {}, stop: () => {}, disconnect: () => {}, playbackRate: { value: 1.0 } }; }
  decodeAudioData() { return Promise.resolve({}); }
  get destination() { return {}; }
  get currentTime() { return 0; }
};
global.window.webkitAudioContext = global.window.AudioContext;

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
      audio.update({ energiaTotal: 0, bladeAngle: 0, isBursting: false });
      console.log("AudioMixer updated calm");
      audio.update({ energiaTotal: 5, bladeAngle: 0, isBursting: false });
      console.log("AudioMixer updated high");
      audio.setDrag(true);
      console.log("Drag set");
      audio.power(false);
      console.log("AudioMixer powered OFF");
      process.exit(0);
  } catch(e) {
      console.error("Crash:", e);
      process.exit(1);
  }
});
