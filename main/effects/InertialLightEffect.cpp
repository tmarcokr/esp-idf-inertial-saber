#include "InertialLightEffect.hpp"
#include "PlatformConfig.hpp"

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

InertialLightEffect::InertialLightEffect(Engine &ledEngine)
    : m_ledEngine(ledEngine) {
  Priority = 0;
}

void InertialLightEffect::activate() {
  if (m_active)
    return;
  m_active = true;
  m_breathPhase = 0.0f;
  m_lastTimestampMs = 0;

  auto blade = std::make_unique<InertialBladeEffect>();
  m_bladeEffect = blade.get();
  m_bladeEffect->setHSB(m_baseHue, 255, 255);
  m_ledEngine.setBaseEffect(std::move(blade));
}

void InertialLightEffect::deactivate() {
  if (!m_active)
    return;
  m_active = false;
  m_bladeEffect = nullptr;

  m_ledEngine.setBaseEffect(nullptr);
}

bool InertialLightEffect::Test(const Core::SaberDataPacket &packet) {
  if (!m_active)
    return false;

  m_kineticEnergy = packet.KineticEnergy;
  m_orientationVector = packet.OrientationVector;
  m_inertialOverload = packet.InertialOverload;
  m_inertialBurst = packet.InertialBurst;

  if (m_lastTimestampMs == 0) {
    m_lastTimestampMs = packet.timestamp_ms;
  }
  uint32_t delta = packet.timestamp_ms - m_lastTimestampMs;
  m_lastTimestampMs = packet.timestamp_ms;

  float angleRad = m_orientationVector * (static_cast<float>(M_PI) / 180.0f);
  float freq =
      Core::Platform::kLightIdleBaseFreq + (std::sin(angleRad) * 0.5f);
  m_breathPhase +=
      (static_cast<float>(delta) / 1000.0f) * freq * 2.0f *
      static_cast<float>(M_PI);

  // Wrap at 2π to prevent float precision loss over long sessions
  if (m_breathPhase > 2.0f * static_cast<float>(M_PI)) {
    m_breathPhase -= 2.0f * static_cast<float>(M_PI);
  }

  return m_active;
}

void InertialLightEffect::Run() {
  if (!m_bladeEffect)
    return;

  uint16_t hue = m_baseHue;
  float saturation = 1.0f;
  float brightness = 1.0f;

  bool isExcited =
      (m_kineticEnergy >= 0.5f) || (m_inertialOverload > 0.0f);

  if (!isExcited) {
    // See: wiki/InertialLight.md §3.1
    float pulse = (std::sin(m_breathPhase) + 1.0f) / 2.0f;
    brightness = (1.0f - Core::Platform::kLightIdlePulseDepth) +
                 (pulse * Core::Platform::kLightIdlePulseDepth);
  } else {
    // See: wiki/InertialLight.md §3.2
    saturation =
        1.0f - (m_inertialOverload * Core::Platform::kLightMaxThermalBleed);

    uint32_t rng = esp_random();
    float r = static_cast<float>(rng) / static_cast<float>(UINT32_MAX);
    float noise = (r * 2.0f) - 1.0f;
    float energyFactor =
        std::clamp(m_kineticEnergy / 4.0f, 0.0f, 1.0f);
    noise *= energyFactor;

    brightness = std::clamp(
        1.0f -
            std::abs(noise * Core::Platform::kLightFlickerIntensity),
        0.8f, 1.0f);
  }

  uint8_t s_u8 = static_cast<uint8_t>(saturation * 255.0f);
  uint8_t v_u8 = static_cast<uint8_t>(brightness * 255.0f);
  m_bladeEffect->setHSB(hue, s_u8, v_u8);

  // See: wiki/InertialLight.md §3.3
  if (m_inertialBurst) {
    uint16_t burstHue = (m_baseHue + 180) % 360;
    Color burstColor = hsvToRgb(burstHue, 255, 255);
    m_ledEngine.pushOverlay(std::make_unique<Flash>(
        burstColor, Core::Platform::kLightBurstDurationMs, 0, 1));
  }
}

} // namespace InertialSaber::Effects
