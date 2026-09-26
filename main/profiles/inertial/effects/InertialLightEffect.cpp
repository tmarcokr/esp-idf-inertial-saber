#include "InertialLightEffect.hpp"

#include "SmartLedTypes.hpp"
#include "effects/Flash.hpp"

#include "esp_random.h"

#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace InertialSaber::Effects {

using namespace Espressif::Wrappers::SmartLed;

InertialLightEffect::InertialLightEffect(
    Engine& ledEngine,
    const InertialSaber::Profiles::Inertial::InertialDefinition& definition)
    : InertialEffect(0)
    , m_ledEngine(ledEngine)
    , m_def(definition)
    , m_baseHue(definition.bladeBaseHue) {}

void InertialLightEffect::activate() {
  if (m_active)
    return;
  m_active = true;
  m_breathPhase = 0.0f;
  m_lastTimestampMs = 0;

  auto blade = std::make_unique<InertialBladeEffect>();
  m_bladeEffect = blade.get();
  m_bladeEffect->setHsb(m_baseHue, 255, 255);
  m_ledEngine.setBaseEffect(std::move(blade));
}

void InertialLightEffect::deactivate() {
  if (!m_active)
    return;
  m_active = false;
  m_bladeEffect = nullptr;

  m_ledEngine.setBaseEffect(nullptr);
}

bool InertialLightEffect::test(const Core::SaberDataPacket &packet) {
  if (!m_active)
    return false;

  m_kineticEnergy = packet.kineticEnergy;
  m_orientation = packet.orientation;
  m_inertialOverload = packet.inertialOverload;
  m_inertialBurst = packet.inertialBurst;

  if (m_lastTimestampMs == 0) {
    m_lastTimestampMs = packet.timestampMs;
  }
  m_deltaMs = packet.timestampMs - m_lastTimestampMs;
  m_lastTimestampMs = packet.timestampMs;

  return m_active;
}

void InertialLightEffect::run() {
  if (!m_bladeEffect)
    return;

  updateBreathPhase();

  float saturation = 1.0f;
  float brightness = 1.0f;

  if (isExcited()) {
    saturation = calculateThermalBleed();
    brightness = calculatePlasmaFlicker();
  } else {
    brightness = calculateIdlePulse();
  }

  uint8_t s_u8 = static_cast<uint8_t>(saturation * 255.0f);
  uint8_t v_u8 = static_cast<uint8_t>(brightness * 255.0f);
  m_bladeEffect->setHsb(m_baseHue, s_u8, v_u8);

  if (m_inertialBurst) {
    triggerPlasmaRuptureOverlay();
  }
}

void InertialLightEffect::updateBreathPhase() {
  float bladeAngleRad = m_orientation * (static_cast<float>(M_PI) / 2.0f);
  float freq = m_def.lightIdleBaseFreq + (std::sin(bladeAngleRad) * 0.5f);
  
  m_breathPhase += (static_cast<float>(m_deltaMs) / 1000.0f) * freq * 2.0f * static_cast<float>(M_PI);

  if (m_breathPhase > 2.0f * static_cast<float>(M_PI)) {
    m_breathPhase -= 2.0f * static_cast<float>(M_PI);
  }
}

bool InertialLightEffect::isExcited() const {
  return (m_kineticEnergy >= 0.5f) || (m_inertialOverload > 0.0f);
}

float InertialLightEffect::calculateIdlePulse() const {
  float pulse = (std::sin(m_breathPhase) + 1.0f) / 2.0f;
  return (1.0f - m_def.lightIdlePulseDepth) + (pulse * m_def.lightIdlePulseDepth);
}

float InertialLightEffect::calculateThermalBleed() const {
  return 1.0f - (m_inertialOverload * m_def.lightMaxThermalBleed);
}

float InertialLightEffect::calculatePlasmaFlicker() const {
  uint32_t rng = esp_random();
  float r = static_cast<float>(rng) / static_cast<float>(UINT32_MAX);
  float noise = (r * 2.0f) - 1.0f;
  float energyFactor = std::clamp(m_kineticEnergy / 4.0f, 0.0f, 1.0f);
  noise *= energyFactor;

  return std::clamp(1.0f - std::abs(noise * m_def.lightFlickerIntensity), 0.8f, 1.0f);
}

void InertialLightEffect::triggerPlasmaRuptureOverlay() {
  uint16_t burstHue = (m_baseHue + 180) % 360;
  Color burstColor = hsvToRgb(burstHue, 255, 255);
  m_ledEngine.pushOverlay(std::make_unique<Flash>(burstColor, m_def.lightBurstDurationMs, 0, 1));
}

} // namespace InertialSaber::Effects
