import fs from 'fs';

global.window = {};
global.window.AudioContext = class {
  createGain() { return { gain: { value: 0, linearRampToValueAtTime: ()=>{} }, connect: () => {} }; }
  createBufferSource() { return { start: () => {}, stop: () => {}, disconnect: () => {}, playbackRate: { value: 1.0 }, connect: () => {} }; }
  decodeAudioData() { return Promise.resolve({}); }
  get destination() { return {}; }
  get currentTime() { return 0; }
};
global.window.webkitAudioContext = global.window.AudioContext;
global.fetch = () => Promise.resolve({ ok: true, arrayBuffer: () => Promise.resolve(new ArrayBuffer(0)) });

import('./docs/simulator/js/audioMixer.js').then(async module => {
  try {
      const audio = new module.AudioMixer();
      await audio.init();
      audio.power(true);
      audio.update({ energiaTotal: 0, bladeAngle: 0, isBursting: false });
      console.log("SUCCESS");
      process.exit(0);
  } catch(e) {
      console.error("Crash:", e);
      process.exit(1);
  }
});
