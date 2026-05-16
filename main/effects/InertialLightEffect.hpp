#pragma once

#include "Engine.hpp"
#include "InertialBladeEffect.hpp"
#include "InertialDefinition.hpp"
#include "InertialEffect.hpp"

#include <cstdint>

namespace InertialSaber::Effects {

/**
 * @brief Flow Modulator (Priority 0) that bridges the Bus with the SmartLed
 * Engine. Computes HSB per cycle following the InertialLight wiki spec and
 * pushes the result atomically to InertialBladeEffect for rendering.
 */
class InertialLightEffect final : public Core::InertialEffect {
public:
  explicit InertialLightEffect(
      Espressif::Wrappers::SmartLed::Engine& ledEngine,
      const Core::InertialDefinition& definition);

  bool Test(const Core::SaberDataPacket &packet) override;
  void Run() override;

  void activate();
  void deactivate();

private:
  Espressif::Wrappers::SmartLed::Engine& m_ledEngine;
  const Core::InertialDefinition& m_def;
  InertialBladeEffect* m_bladeEffect = nullptr;
  bool m_active = false;

  uint16_t m_baseHue;
  float m_kineticEnergy = 0.0f;
  float m_orientationVector = 0.0f;
  float m_inertialOverload = 0.0f;
  bool m_inertialBurst = false;

  float m_breathPhase = 0.0f;
  uint32_t m_lastTimestampMs = 0;
};

} // namespace InertialSaber::Effects
