export class PhysicsEngine {
    constructor() {
        // Output Metrics
        this.energiaTotal = 0;     // Instantaneous G-force
        this.bladeAngle = 0;       // Degrees (-90 to +90)
        this.tanqueOverload = 0.0; // 0.0 to 1.0 (Tank level)
        
        // Tuning Parameters (From InertialSwing Specs)
        this.overloadThreshold = 3.5;
        this.maxG = 8.0;
        this.fillRate = 0.02;      // Scaled for 60fps frame delta
        this.drainRate = 0.03;
        this.burstCooldownMs = 300;
        
        // Internal State
        this.isBursting = false;
        this.lastBurstTime = 0;
        
        // Mouse Simulation State
        this.isDragging = false;
        this.lastMousePos = { x: 0, y: 0 };
        this.targetAngle = 0;
        this.currentAngle = 0;
        this.simulatedVelocity = 0;
    }

    // Input from UI (Simulating IMU)
    handlePointerDown(x, y) {
        this.isDragging = true;
        this.lastMousePos = { x, y };
    }

    handlePointerMove(x, y) {
        if (!this.isDragging) return;
        
        const dx = x - this.lastMousePos.x;
        const dy = y - this.lastMousePos.y;
        
        // Simple drag to angle conversion
        this.targetAngle += dy * 0.5;
        // Clamp angle to realistic limits for the simulation (-90 to 90)
        if(this.targetAngle > 90) this.targetAngle = 90;
        if(this.targetAngle < -90) this.targetAngle = -90;

        // Calculate velocity (simulated G-force)
        const dist = Math.sqrt(dx*dx + dy*dy);
        this.simulatedVelocity = dist; // Pixels per frame -> pseudo force

        this.lastMousePos = { x, y };
    }

    handlePointerUp() {
        this.isDragging = false;
    }

    // Step the physics engine (called every frame)
    update(deltaTimeMs) {
        // 1. Smooth angle interpolation (simulating physical inertia of the hilt)
        this.currentAngle += (this.targetAngle - this.currentAngle) * 0.1;
        this.bladeAngle = this.currentAngle;

        // 2. Decay simulated velocity if not moving
        if (!this.isDragging || this.simulatedVelocity < 0.1) {
            this.simulatedVelocity *= 0.8; // Friction
        }
        
        // Map simulated velocity to G-Force (0 to ~8.0G)
        // Adjust the divisor to make it feel right with the mouse
        const rawG = this.simulatedVelocity / 15.0; 
        
        // Add a base gravity noise and blend
        this.energiaTotal = this.energiaTotal * 0.5 + rawG * 0.5; 
        if(this.energiaTotal < 0.1 && !this.isDragging) this.energiaTotal = 0;

        // 3. Update Inertial Overload Tank
        const now = performance.now();
        if (now - this.lastBurstTime > this.burstCooldownMs) {
            this.isBursting = false;
            
            if (this.energiaTotal > this.overloadThreshold) {
                // Active Charge
                const excess = this.energiaTotal - this.overloadThreshold;
                this.tanqueOverload += excess * this.fillRate;
            } else {
                // Inertia Drain
                this.tanqueOverload -= this.drainRate;
            }
            
            // Clamp tank
            this.tanqueOverload = Math.max(0, Math.min(1.0, this.tanqueOverload));
            
            // Trigger Burst
            if (this.tanqueOverload >= 1.0) {
                this.triggerBurst(now);
            }
        }

        // Return metrics for UI and Renderer
        return {
            energiaTotal: this.energiaTotal,
            bladeAngle: this.bladeAngle,
            tanqueOverload: this.tanqueOverload,
            isBursting: this.isBursting
        };
    }

    triggerBurst(now) {
        this.isBursting = true;
        this.tanqueOverload = 0.0;
        this.lastBurstTime = now;
    }

    forceFillTank() {
        this.tanqueOverload = 1.5;
    }
}