#!/usr/bin/env python3
"""Validate the electrical and calibration contract of crt_params.json."""

from __future__ import annotations

import argparse
import json
import math
from pathlib import Path
import sys


STANDARD = {
    "horizontalRateHz": (15734.264, 0.05),
    "frameRateHz": (60.054443, 0.000005),
    "subcarrierMHz": (3.579545, 0.000001),
}

NATIVE_240P_METADATA = {
    "videoMode": "native 240p progressive",
    "sourceTimingProfile": "rayEngine-320x192-262p",
    "totalLines": 262,
    "activeStartLine": 20,
    "activeLines": 240,
    "contentStartLine": 44,
    "contentLines": 192,
    "contentPixelsPerLine": 320,
    "sourceFramebufferLines": 192,
    "presentationAspect": 5.0 / 3.0,
    "overscan": False,
    "signalSamplesPerLine": 910,
    "rasterWidth": 3840,
    "rasterHeight": 2160,
    "tubeRect": {"x": 160, "y": 24, "width": 3520, "height": 2112},
    "nativePresentationScale": 11,
    "rasterFormat": "RGBA16F",
    "framebufferLatch": "vertical blank",
    "setupIRE": 7.5,
}

POSITIVE = (
    "ntscSourceLumaBandwidthMHz",
    "ntscSourceIBandwidthMHz",
    "ntscSourceQBandwidthMHz",
    "ntscLumaBandwidthMHz",
    "ntscChromaBandwidthIMHz",
    "ntscChromaBandwidthQMHz",
    "beamMinWidth",
    "beamMaxWidth",
    "maskTriadsAcross",
    "bloomRadius",
    "outputGamma",
    "tubePeakNits",
    "hostPeakNits",
    "referenceWhiteRadiance",
    "highVoltageResponse",
    "ntscAgcResponse",
    "ntscClampResponse",
    "ntscBurstPllBandwidthHz",
    "ntscHorizontalPllBandwidthHz",
    "ntscVerticalPllBandwidthHz",
    "ntscAccResponse",
    "beamCurrentLimit",
    "videoOutputBandwidthMHz",
    "cathodeDriveHeadroom",
    "phosphorSaturation",
    "maskThermalTau",
    "bPlusResponse",
    "glassRefractiveIndex",
    "glassThicknessMm",
)

NON_NEGATIVE = (
    "ntscColorKillerThreshold",
    "beamCurrentCompression",
    "spaceChargeCompression",
    "spotBloom",
    "dynamicFocus",
    "phosphorMediumWeight",
    "phosphorSlowWeight",
    "highVoltageSag",
    "glassDispersion",
    "faceplateCurvatureX",
    "faceplateCurvatureY",
    "maskHeating",
    "maskThermalDiffusion",
    "maskDoming",
    "maskCrosstalk",
    "highVoltageRipple",
    "bPlusSag",
    "bPlusRipple",
    "internalReflection",
    "ambientIlluminance",
    "ntscLineCombStrength",
)

VECTORS = (
    "videoGain",
    "videoCutoff",
    "gunGamma",
    "phosphorFastDecay",
    "phosphorMediumDecay",
    "phosphorSlowDecay",
    "bloomRadiusRGB",
    "glassTint",
    "glassAbsorption",
    "tubeColorMatrixR",
    "tubeColorMatrixG",
    "tubeColorMatrixB",
)

REQUIRED_SHADERS = (
    "crt_ntsc_prefilter.fs",
    "crt_ntsc_encode.fs",
    "crt_ntsc_bandpass.fs",
    "crt_ntsc_burst.fs",
    "crt_ntsc_burst_state.fs",
    "crt_ntsc_video_state.fs",
    "crt_ntsc_vertical_state.fs",
    "crt_ntsc_decode.fs",
    "crt_drive.fs",
    "crt_deflection.fs",
    "crt_scan_vertical.fs",
    "crt_mask_thermal_state.fs",
    "crt_emission.fs",
    "phosphor_state.fs",
    "crt_phosphor_combine.fs",
    "crt_tube_color.fs",
    "crt_apl_state.fs",
    "crt_observer_response.fs",
    "crt_brightpass.fs",
    "crt_bloom_downsample.fs",
    "crt_bloom_h.fs",
    "crt_bloom_v.fs",
    "crt.fs",
)


def finite_number(value: object) -> bool:
    return isinstance(value, (int, float)) and math.isfinite(float(value))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("profile", nargs="?", default="crt_params.json")
    parser.add_argument("--require-measured", action="store_true")
    args = parser.parse_args()

    path = Path(args.profile)
    data = json.loads(path.read_text(encoding="utf-8"))
    metadata = data.get("profile", {})
    errors: list[str] = []
    warnings: list[str] = []

    for key, (expected, tolerance) in STANDARD.items():
        value = metadata.get(key)
        if not finite_number(value) or abs(float(value) - expected) > tolerance:
            errors.append(f"profile.{key}: expected {expected} +/- {tolerance}, got {value!r}")

    for key, expected in NATIVE_240P_METADATA.items():
        value = metadata.get(key)
        if value != expected:
            errors.append(f"profile.{key}: expected {expected!r}, got {value!r}")

    line_rate = metadata.get("horizontalRateHz")
    total_lines = metadata.get("totalLines")
    frame_rate = metadata.get("frameRateHz")
    if all(finite_number(value) for value in (line_rate, total_lines, frame_rate)):
        derived_rate = float(line_rate) / float(total_lines)
        if abs(float(frame_rate) - derived_rate) > 0.000005:
            errors.append(
                "profile.frameRateHz must equal horizontalRateHz / totalLines"
            )
    pixel_clock = metadata.get("sourcePixelClockMHz")
    if not finite_number(pixel_clock) or abs(float(pixel_clock) - 320.0 / 52.655) > 0.000005:
        errors.append(
            "profile.sourcePixelClockMHz must equal 320 / 52.655 us"
        )

    for key in POSITIVE:
        value = data.get(key)
        if not finite_number(value) or float(value) <= 0.0:
            errors.append(f"{key}: expected a positive finite number, got {value!r}")

    for key in NON_NEGATIVE:
        value = data.get(key)
        if not finite_number(value) or float(value) < 0.0:
            errors.append(f"{key}: expected a non-negative finite number, got {value!r}")

    for key in VECTORS:
        value = data.get(key)
        if not isinstance(value, list) or len(value) != 3 or not all(finite_number(v) for v in value):
            errors.append(f"{key}: expected three finite numbers, got {value!r}")

    if data.get("ntscSourceQBandwidthMHz", 1.0) > data.get("ntscSourceIBandwidthMHz", 0.0):
        errors.append("NTSC Q source bandwidth must not exceed I bandwidth")
    if data.get("beamMinWidth", 1.0) > data.get("beamMaxWidth", 0.0):
        errors.append("beamMinWidth must not exceed beamMaxWidth")
    if not 0.0 <= data.get("ntscColorKillerThreshold", -1.0) <= 1.0:
        errors.append("ntscColorKillerThreshold must be in [0, 1]")
    if data.get("phosphorMediumWeight", 0.0) + data.get("phosphorSlowWeight", 0.0) > 1.0:
        errors.append("phosphor medium and slow weights must sum to at most 1")
    if "ntscInterlace" in data:
        errors.append("ntscInterlace is forbidden: the pipeline is native 240p only")
    if data.get("glassRefractiveIndex", 0.0) <= 1.0:
        errors.append("glassRefractiveIndex must be greater than 1")
    if not 0.0 <= data.get("maskCrosstalk", -1.0) <= 0.25:
        errors.append("maskCrosstalk must be in [0, 0.25]")
    if not 0.0 <= data.get("ntscLineCombStrength", -1.0) <= 1.0:
        errors.append("ntscLineCombStrength must be in [0, 1]")
    absorption = data.get("glassAbsorption", [])
    if isinstance(absorption, list) and any(
            finite_number(value) and value < 0.0 for value in absorption):
        errors.append("glassAbsorption components must be non-negative")

    matrix = [data.get("tubeColorMatrixR"), data.get("tubeColorMatrixG"),
              data.get("tubeColorMatrixB")]
    if all(isinstance(row, list) and len(row) == 3 and
           all(finite_number(component) for component in row) for row in matrix):
        determinant = (
            matrix[0][0] * (matrix[1][1] * matrix[2][2] - matrix[1][2] * matrix[2][1])
            - matrix[0][1] * (matrix[1][0] * matrix[2][2] - matrix[1][2] * matrix[2][0])
            + matrix[0][2] * (matrix[1][0] * matrix[2][1] - matrix[1][1] * matrix[2][0])
        )
        if abs(determinant) < 1e-6:
            errors.append("tube color matrix must be invertible")

    fast = data.get("phosphorFastDecay", [])
    medium = data.get("phosphorMediumDecay", [])
    slow = data.get("phosphorSlowDecay", [])
    if all(isinstance(value, list) and len(value) == 3
           for value in (fast, medium, slow)):
        for channel, values in zip("RGB", zip(fast, medium, slow)):
            if not values[0] <= values[1] <= values[2]:
                errors.append(
                    f"phosphor {channel}: decay constants must satisfy fast <= medium <= slow"
                )

    shader_dir = path.parent / "Data" / "Shaders"
    for shader_name in REQUIRED_SHADERS:
        if not (shader_dir / shader_name).is_file():
            errors.append(f"missing required CRT stage: Data/Shaders/{shader_name}")

    forbidden_legacy_tokens = (
        "ntscInterlace",
        "interlac",
        "262.5",
        "480i",
        "crtField",
        "fieldRateHz",
        "fieldParity",
        "fieldSequence",
        "heldVideoField",
        "four-field",
        "four_field",
    )
    native_240p_sources = (
        path,
        path.parent / "src" / "Managers" / "shader_m.cpp",
        path.parent / "src" / "Managers" / "shader_m.h",
        path.parent / "src" / "imgui_layer.cpp",
        *sorted(shader_dir.glob("crt*.fs")),
        shader_dir / "phosphor_state.fs",
    )
    for source_path in native_240p_sources:
        source = source_path.read_text(encoding="utf-8")
        for token in forbidden_legacy_tokens:
            if token in source:
                errors.append(
                    f"{source_path.relative_to(path.parent)} contains forbidden "
                    f"legacy timing token {token!r}"
                )

    runtime_source = (path.parent / "src" / "Managers" /
                      "shader_m.cpp").read_text(encoding="utf-8")
    required_runtime_contract = (
        "constexpr int CRT_TOTAL_LINES = 262;",
        "constexpr int CRT_ACTIVE_LINES = 240;",
        "constexpr int CRT_ACTIVE_START_LINE = 20;",
        "constexpr int CRT_CONTENT_LINES = NATIVE_RES_HEIGHT;",
        "constexpr int CRT_CONTENT_START_LINE = 44;",
        "constexpr int CRT_RASTER_WIDTH = 3840;",
        "constexpr int CRT_RASTER_HEIGHT = 2160;",
        "loadHDRRenderTexture(CRT_SIGNAL_SAMPLES, CRT_TOTAL_LINES)",
        "pendingVideoRT_",
        "previousEmissionRT_",
        "currentEmissionRT_",
        "previousDriveRT_",
        "currentDriveRT_",
        "receiverVerticalRT_",
        "crtFramesAdvanced",
        "crtLastPipelineTime",
        "elapsed, 0.000001, 3600.0",
        "frameIndex - heldVideoFrame_, 0, 1000000",
    )
    for fragment in required_runtime_contract:
        if fragment not in runtime_source:
            errors.append(f"native 240p runtime contract is missing {fragment!r}")

    phosphor_source = (shader_dir / "phosphor_state.fs").read_text(
        encoding="utf-8")
    required_phosphor_contract = (
        "void accumulateStrikes",
        "firstCurrentFrame = 1.0 - max(crtFramesAdvanced, 1.0)",
    )
    for fragment in required_phosphor_contract:
        if fragment not in phosphor_source:
            errors.append(f"exact phosphor contract is missing {fragment!r}")
    if "prevExposureTexture" in phosphor_source:
        errors.append("phosphor display must derive from exact strike integration")
    combine_source = (shader_dir / "crt_phosphor_combine.fs").read_text(
        encoding="utf-8")
    for fragment in ("vec3 synchronizedExposure", "ntscContentStartLine",
                     "currentEmissionTexture", "previousEmissionTexture"):
        if fragment not in combine_source:
            errors.append(
                f"synchronized phosphor combine is missing {fragment!r}")
    if "phosphorExposureRT_" in runtime_source:
        errors.append("duplicate full-resolution phosphor exposure targets remain")

    temporal_shaders = (
        "phosphor_state.fs",
        "crt_apl_state.fs",
        "crt_mask_thermal_state.fs",
        "crt_ntsc_burst_state.fs",
        "crt_ntsc_video_state.fs",
        "crt_ntsc_vertical_state.fs",
        "crt_observer_response.fs",
    )
    for shader_name in temporal_shaders:
        source = (shader_dir / shader_name).read_text(encoding="utf-8")
        if "clamp(frameTime" in source or "min(frameTime" in source:
            errors.append(
                f"{shader_name} truncates elapsed time and can retain stale state"
            )

    for shader_name in ("crt_drive.fs", "crt_deflection.fs", "crt.fs"):
        source = (shader_dir / shader_name).read_text(encoding="utf-8")
        if "120.0 * rasterTime" not in source:
            errors.append(
                f"{shader_name} must evaluate supply ripple at per-pixel raster time"
            )

    deflection_source = (shader_dir / "crt_deflection.fs").read_text(
        encoding="utf-8")
    final_source = (shader_dir / "crt.fs").read_text(encoding="utf-8")
    for fragment in ("normalizedX", "normalizedY", "1.0 - rasterP.x * rasterP.x"):
        if fragment not in deflection_source:
            errors.append(f"edge-anchored deflection is missing {fragment!r}")
    scan_source = (shader_dir / "crt_scan_vertical.fs").read_text(
        encoding="utf-8")
    for fragment in ("generalizedGaussianArea", "signal * weight / kernelArea"):
        if fragment not in scan_source:
            errors.append(f"energy-conserving beam is missing {fragment!r}")
    for fragment in ("opticalScaleX", "opticalScaleY", "edgeTaper"):
        if fragment not in final_source:
            errors.append(f"edge-anchored faceplate optics is missing {fragment!r}")
    if "sdRoundBox(tubeP, vec2(1.0), rounded)" not in final_source:
        errors.append("tube aperture subtracts its corner radius more than once")
    bandpass_source = (shader_dir / "crt_ntsc_bandpass.fs").read_text(
        encoding="utf-8")
    for fragment in ("ntscLineCombStrength", "lineCombBand",
                     "previousBandSum", "nextBandSum"):
        if fragment not in bandpass_source:
            errors.append(
                f"two-line cross-colour suppression is missing {fragment!r}")
    for fragment in ("referenceWhiteRadiance",
                     "max(referenceWhiteRadiance, 0.01)"):
        if fragment not in final_source:
            errors.append(
                f"front-face luminance calibration is missing {fragment!r}")

    anode_voltage = metadata.get("anodeVoltageKv")
    anode_status = metadata.get("anodeVoltageStatus")
    if anode_voltage is not None and (
            not finite_number(anode_voltage) or float(anode_voltage) <= 0.0):
        errors.append("profile.anodeVoltageKv must be null or a positive measurement")
    if anode_voltage is not None and anode_status != "measured":
        errors.append("a numeric anode voltage must be marked measured")

    status = metadata.get("calibrationStatus", "missing")
    serial = metadata.get("measuredSerial", "")
    if status != "measured" or not serial:
        warnings.append(
            "tube-specific profile is provisional: provide measuredSerial and set "
            "calibrationStatus='measured' only after the documented calibration run"
        )
        if args.require_measured:
            errors.append("a measured CT-1358 profile is required")
    elif anode_voltage is None or anode_status != "measured":
        errors.append("a measured profile requires measured anode voltage")

    for warning in warnings:
        print(f"WARNING: {warning}")
    for error in errors:
        print(f"ERROR: {error}", file=sys.stderr)

    if errors:
        return 1
    print(f"OK: {path} satisfies the CRT profile schema and NTSC timing contract")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
