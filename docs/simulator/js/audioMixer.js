export class AudioMixer {
    constructor() {
        this.initialized = false;
        this.ctx = null;
        this.buffers = {};
        this.sources = {};
        this.gains = {};
        
        // Define all sound files
        this.soundFiles = {
            hum: ['sound/Proffie/hum.wav'],
            swingL: Array.from({length: 3}, (_, i) => `sound/Proffie/swingl/swingl${i+1}.wav`),
            swingH: Array.from({length: 3}, (_, i) => `sound/Proffie/swingh/swingh${i+1}.wav`),
            poweron: Array.from({length: 4}, (_, i) => `sound/Proffie/out/out${i+1}.wav`),
            poweroff: Array.from({length: 2}, (_, i) => `sound/Proffie/in/in${i+1}.wav`),
            burst: Array.from({length: 16}, (_, i) => `sound/Proffie/swng/swng${i+1}.wav`),
            blaster: Array.from({length: 8}, (_, i) => `sound/Proffie/blst/blst${i+1}.wav`),
            drag: ['sound/Proffie/drag/drag1.wav'],
            enddrag: Array.from({length: 8}, (_, i) => `sound/Proffie/enddrag/enddrag${i+1}.wav`)
        };

        this.currentVolumes = {
            hum: 0,
            swingL: 0,
            swingH: 0
        };

        this.activeIndices = {
            hum: 0,
            swing: 0
        };

        this.isOn = false;
        this.isDragging = false;
        this.wasCalm = true;
    }

    async init() {
        if (this.initialized) return;
        
        try {
            this.ctx = new (window.AudioContext || window.webkitAudioContext)();
            this.masterGain = this.ctx.createGain();
            this.masterGain.connect(this.ctx.destination);
            
            await this.loadSounds();
            this.setupNodes();
            
            this.initialized = true;
            console.log("AudioMixer initialized with real sounds.");
        } catch (e) {
            console.error("Web Audio API failed to initialize:", e);
        }
    }

    async loadSounds() {
        const loadPromises = Object.entries(this.soundFiles).map(async ([key, urls]) => {
            this.buffers[key] = [];
            for (const url of urls) {
                try {
                    const response = await fetch(url);
                    if (!response.ok) throw new Error(`HTTP error! status: ${response.status}`);
                    const arrayBuffer = await response.arrayBuffer();
                    const buffer = await this.ctx.decodeAudioData(arrayBuffer);
                    this.buffers[key].push(buffer);
                } catch (e) {
                    console.warn(`Failed to load ${url}:`, e);
                }
            }
        });
        await Promise.all(loadPromises);
    }

    setupNodes() {
        // Initialize Gains
        ['hum', 'swingL', 'swingH', 'drag'].forEach(key => {
            if (!this.buffers[key] || this.buffers[key].length === 0) return;
            
            this.gains[key] = this.ctx.createGain();
            this.gains[key].gain.value = 0; // Start muted
            this.gains[key].connect(this.masterGain);

            // Pre-start the drag source. Hum and swings are started in power(on).
            if (key === 'drag') {
                this.sources[key] = this.ctx.createBufferSource();
                this.sources[key].buffer = this.buffers[key][0];
                this.sources[key].loop = true;
                this.sources[key].connect(this.gains[key]);
                this.sources[key].start();
            }
        });
    }

    playSound(key, volume = 1.0) {
        if (!this.initialized || !this.buffers[key] || this.buffers[key].length === 0) return;
        
        const bufferArray = this.buffers[key];
        const randomBuffer = bufferArray[Math.floor(Math.random() * bufferArray.length)];

        const source = this.ctx.createBufferSource();
        source.buffer = randomBuffer;
        
        const gain = this.ctx.createGain();
        gain.gain.value = volume;
        
        source.connect(gain).connect(this.masterGain);
        source.start();
        return source;
    }

    power(on) {
        if (!this.initialized) return;
        this.isOn = on;
        
        if (on) {
            // Select random hum and swing pair on startup
            if (this.buffers['hum'] && this.buffers['hum'].length > 0) {
                this.activeIndices.hum = Math.floor(Math.random() * this.buffers['hum'].length);
            }
            if (this.buffers['swingL'] && this.buffers['swingL'].length > 0) {
                this.activeIndices.swing = Math.floor(Math.random() * this.buffers['swingL'].length);
            }

            // Start looping sounds
            ['hum', 'swingL', 'swingH'].forEach(key => {
                if (!this.buffers[key] || this.buffers[key].length === 0) return;
                
                // Stop existing source if any
                if (this.sources[key]) {
                    try { this.sources[key].stop(); } catch(e){}
                    this.sources[key].disconnect();
                }

                const index = key === 'hum' ? this.activeIndices.hum : this.activeIndices.swing;
                const safeIndex = Math.min(index, this.buffers[key].length - 1);

                this.sources[key] = this.ctx.createBufferSource();
                this.sources[key].buffer = this.buffers[key][safeIndex];
                this.sources[key].loop = true;
                this.sources[key].connect(this.gains[key]);
                this.sources[key].start();
            });

            this.playSound('poweron');
            this.fadeGain('hum', 0.8, 0.5); // Fade hum in
        } else {
            this.playSound('poweroff');
            this.fadeGain('hum', 0.0, 0.3); // Fade hum out
            this.fadeGain('swingL', 0.0, 0.1);
            this.fadeGain('swingH', 0.0, 0.1);
            this.setDrag(false);

            // Note: Sources are kept running but muted, will be restarted on next power on
        }
    }

    triggerBurst() {
        if (!this.initialized || !this.isOn) return;
        this.playSound('burst', 1.0);
    }

    triggerBlaster() {
        if (!this.initialized || !this.isOn) return;
        this.playSound('blaster', 0.8);
    }

    setDrag(active) {
        if (!this.initialized || !this.isOn) return;
        if (this.isDragging === active) return;
        this.isDragging = active;

        if (active) {
            this.fadeGain('drag', 1.0, 0.1);
        } else {
            this.fadeGain('drag', 0.0, 0.1);
            this.playSound('enddrag', 0.8);
        }
    }

    fadeGain(key, targetValue, duration) {
        if (this.gains[key]) {
            this.gains[key].gain.linearRampToValueAtTime(targetValue, this.ctx.currentTime + duration);
            if (this.currentVolumes[key] !== undefined) {
                this.currentVolumes[key] = targetValue;
            }
        }
    }

    getVolumes() {
        return this.currentVolumes;
    }

    // Update audio volumes based on InertialSwing specification
    update(metrics) {
        if (!this.initialized || !this.isOn) return;
        
        const { energiaTotal, bladeAngle, isBursting } = metrics;
        
        if (isBursting && !this.wasBursting) {
            this.triggerBurst();
        }
        this.wasBursting = isBursting;

        if (isBursting) {
            return;
        }

        // 1. Master Swing Volume Calculation
        const masterVolume = Math.max(0, Math.min(1.0, (energiaTotal - 0.5) / (4.0 - 0.5)));

        // 2. Tonal Balance (Inertial Crossfade + Gravity)
        const baseMix = Math.max(0, Math.min(1.0, (energiaTotal - 2.0) / (4.0 - 2.0)));
        const gravityMod = Math.sin(bladeAngle * Math.PI / 180) * 0.2;
        const finalMix = Math.max(0, Math.min(1.0, baseMix + gravityMod));

        // 3. Output per channel
        // Don't fade swings if dragging
        let targetSwingL = 0;
        let targetSwingH = 0;

        if (!this.isDragging) {
            targetSwingL = masterVolume * (1.0 - finalMix) * 0.6;
            targetSwingH = masterVolume * finalMix * 0.6;
        }

        this.fadeGain('swingL', targetSwingL, 0.05);
        this.fadeGain('swingH', targetSwingH, 0.05);

        // 4. "Zero-Volume SD Swapper" implementation
        // Only swap if we are practically silent (Calm state) to avoid pops/clicks
        if (masterVolume < 0.01 && this.buffers['swingL'] && this.buffers['swingL'].length > 1) {
             // Only swap occasionally to avoid thrashing, or we just swap every time it goes silent
             // To simulate the SD swapper, we'll pick a new pair when entering CALM state
             if (!this.wasCalm) {
                 const newSwingIndex = Math.floor(Math.random() * this.buffers['swingL'].length);
                 
                 // If the index changed, perform the swap
                 if (newSwingIndex !== this.activeIndices.swing) {
                     this.activeIndices.swing = newSwingIndex;
                     
                     ['swingL', 'swingH'].forEach(key => {
                        if (this.sources[key]) {
                            try { this.sources[key].stop(); } catch(e){}
                            this.sources[key].disconnect();
                        }
                        
                        const safeIndex = Math.min(newSwingIndex, this.buffers[key].length - 1);
                        this.sources[key] = this.ctx.createBufferSource();
                        this.sources[key].buffer = this.buffers[key][safeIndex];
                        this.sources[key].loop = true;
                        this.sources[key].connect(this.gains[key]);
                        this.sources[key].start();
                     });
                 }
                 this.wasCalm = true;
             }
        } else if (masterVolume >= 0.01) {
             this.wasCalm = false;
        }
        
        // Pitch modulation (playbackRate) based on speed
        if (this.sources['swingL'] && this.sources['swingL'].playbackRate) {
             this.sources['swingL'].playbackRate.value = 1.0 + (energiaTotal * 0.02);
        }
        if (this.sources['swingH'] && this.sources['swingH'].playbackRate) {
             this.sources['swingH'].playbackRate.value = 1.0 + (energiaTotal * 0.05);
        }
    }
}