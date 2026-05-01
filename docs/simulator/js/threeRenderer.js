import * as THREE from 'three';

export class ThreeRenderer {
    constructor(containerId) {
        this.container = document.getElementById(containerId);
        
        // Scene setup
        this.scene = new THREE.Scene();
        // Add a subtle background environment
        this.scene.background = new THREE.Color(0x1a1a24); // Lighter dark-blueish grey
        
        // Camera setup
        this.camera = new THREE.PerspectiveCamera(45, window.innerWidth / window.innerHeight, 0.1, 100);
        this.camera.position.set(0, 0, 15);
        
        // Renderer setup
        this.renderer = new THREE.WebGLRenderer({ antialias: true });
        this.renderer.setSize(window.innerWidth, window.innerHeight);
        this.renderer.setPixelRatio(window.devicePixelRatio);
        this.renderer.toneMapping = THREE.ACESFilmicToneMapping;
        this.renderer.toneMappingExposure = 1.5;
        this.container.appendChild(this.renderer.domElement);
        
        // Lightsaber Construction
        this.saberGroup = new THREE.Group();
        this.scene.add(this.saberGroup);
        
        // 1. The Hilt (Mango)
        const hiltGeo = new THREE.CylinderGeometry(0.15, 0.15, 1.2, 32);
        const hiltMat = new THREE.MeshStandardMaterial({ 
            color: 0x888888, // Brighter silver metal
            metalness: 0.8, 
            roughness: 0.3 
        });
        this.hilt = new THREE.Mesh(hiltGeo, hiltMat);
        this.hilt.position.y = -0.6; // Shift down so origin is at the emitter
        this.saberGroup.add(this.hilt);
        
        // 2. The Blade Core (Hoja)
        const bladeGeo = new THREE.CylinderGeometry(0.12, 0.12, 6.0, 32);
        // Shift geometry so it grows upward from the emitter
        bladeGeo.translate(0, 3.0, 0); 
        
        this.bladeMat = new THREE.MeshBasicMaterial({ 
            color: 0xffffff,
            transparent: true,
            opacity: 1.0
        });
        this.blade = new THREE.Mesh(bladeGeo, this.bladeMat);
        this.saberGroup.add(this.blade);
        
        // 3. The Blade Glow (Aura)
        const glowGeo = new THREE.CylinderGeometry(0.25, 0.25, 6.0, 32);
        glowGeo.translate(0, 3.0, 0);
        this.glowMat = new THREE.MeshBasicMaterial({
            color: 0x00e5ff,
            transparent: true,
            opacity: 0.4,
            blending: THREE.AdditiveBlending,
            depthWrite: false
        });
        this.glow = new THREE.Mesh(glowGeo, this.glowMat);
        this.saberGroup.add(this.glow);

        // Lighting
        const ambientLight = new THREE.AmbientLight(0xffffff, 0.5);
        this.scene.add(ambientLight);
        
        const dirLight = new THREE.DirectionalLight(0xffffff, 1);
        dirLight.position.set(5, 5, 5);
        this.scene.add(dirLight);

        // Resize handler
        window.addEventListener('resize', this.onWindowResize.bind(this), false);
        
        // Helper object for HSB conversions
        this.colorObj = new THREE.Color();
    }

    onWindowResize() {
        this.camera.aspect = window.innerWidth / window.innerHeight;
        this.camera.updateProjectionMatrix();
        this.renderer.setSize(window.innerWidth, window.innerHeight);
    }

    // Update visuals based on InertialLight specification
    update(physicsMetrics, baseHue, time) {
        const { energiaTotal, bladeAngle, tanqueOverload, isBursting, isOn, isDragging, blasterActive } = physicsMetrics;
        
        // Power On/Off Animation (Scale blade and glow)
        const targetScaleY = isOn ? 1.0 : 0.0;
        this.blade.scale.y += (targetScaleY - this.blade.scale.y) * 0.2;
        this.glow.scale.y = this.blade.scale.y;

        // Apply rotation to the entire saber group
        // Convert degrees to radians. Three.js Z axis points out, so we rotate around Z for screen space.
        this.saberGroup.rotation.z = -bladeAngle * (Math.PI / 180);

        // If blade is practically off, skip coloring logic
        if (this.blade.scale.y < 0.05 && !isOn) {
            this.bladeMat.opacity = 0;
            this.glowMat.opacity = 0;
            this.renderer.render(this.scene, this.camera);
            return;
        }

        this.bladeMat.opacity = 1.0;

        let finalHue = baseHue;
        let finalSat = 1.0;
        let finalBright = 1.0;

        if (isDragging) {
            // Friction Burn (Drag): Tip incandescence
            finalHue = 30; // Orange
            finalSat = 1.0;
            finalBright = 1.0;
            const noise = Math.random() * 0.4;
            this.glowMat.opacity = 0.6 + noise;
        } 
        else if (isBursting) {
            // STATE 3: Plasma Rupture (Complementary Flash)
            finalHue = (baseHue + 180) % 360;
            finalSat = 1.0;
            finalBright = 1.0;
            this.glowMat.opacity = 1.0; // Max aura
        } 
        else if (blasterActive) {
            // Deflection Burst (Blaster): White flash
            finalHue = baseHue;
            finalSat = 0.0; // White
            finalBright = 1.0;
            this.glowMat.opacity = 1.0;
        }
        else if (energiaTotal > 0.5 || tanqueOverload > 0.0) {
            // STATE 2: Thermal Excitation
            // Saturation loss (Plasma Heating)
            const maxThermalBleed = 0.8;
            finalSat = 1.0 - (tanqueOverload * maxThermalBleed);
            
            // Reactive Brightness (Flicker)
            const noise = (Math.random() * 2 - 1.0) * Math.min(energiaTotal / 4.0, 1.0);
            finalBright = Math.max(0.8, Math.min(1.0, 1.0 - Math.abs(noise * 0.2)));
            
            this.glowMat.opacity = 0.6 + (tanqueOverload * 0.2);
        } 
        else {
            // STATE 1: Live Breathing
            const baseFreq = 1.0;
            const actualFreq = baseFreq + (Math.sin(bladeAngle * Math.PI / 180) * 0.5);
            const pulse = (Math.sin(time * actualFreq * 2 * Math.PI) + 1.0) / 2.0;
            
            finalSat = 1.0;
            finalBright = 0.85 + (pulse * 0.15);
            this.glowMat.opacity = 0.4 + (pulse * 0.1);
        }

        // Apply HSB to materials
        // Three.js setHSL uses values from 0.0 to 1.0
        const h = finalHue / 360.0;
        const s = finalSat;
        const l = finalBright * 0.5; // HSL Lightness at 0.5 is pure color

        this.colorObj.setHSL(h, s, l);
        
        // The core becomes whiter as saturation drops
        this.bladeMat.color.setHSL(h, s * 0.2, finalBright * 0.9); // Core is always brighter
        this.glowMat.color.copy(this.colorObj);

        // Render scene
        this.renderer.render(this.scene, this.camera);
    }
}