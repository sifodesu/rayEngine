#!/usr/bin/env python3
"""Deterministic numerical invariants shared by the real-time CRT stages."""

from __future__ import annotations

import math


FSC_HZ = 3_579_545.0
LINE_HZ = 15_734.264
TOTAL_LINES = 262
ACTIVE_LINES = 240
ACTIVE_START_LINE = 20
SOURCE_LINES = 192
CONTENT_LINES = SOURCE_LINES
CONTENT_START_LINE = 44
FRAME_HZ = LINE_HZ / TOTAL_LINES
SAMPLES_PER_LINE = 910
RASTER_WIDTH = 3840
RASTER_HEIGHT = 2160
TUBE_WIDTH = 3520
TUBE_HEIGHT = 2112
TUBE_X = 160
TUBE_Y = 24
NATIVE_SCALE = 11


def close(actual: float, expected: float, tolerance: float, label: str) -> None:
    if abs(actual - expected) > tolerance:
        raise AssertionError(
            f"{label}: expected {expected} +/- {tolerance}, got {actual}"
        )


def test_signal_clock() -> None:
    samples = 4.0 * FSC_HZ / LINE_HZ
    close(samples, SAMPLES_PER_LINE, 0.01, "4Fsc samples per System-M line")
    cycles_per_line = FSC_HZ / LINE_HZ
    close(cycles_per_line, 227.5, 0.002, "subcarrier cycles per line")


def test_native_240p_frame() -> None:
    close(FRAME_HZ, 60.0544427480916, 1e-12,
          "262-line progressive frame rate")
    cycles_per_frame = 227.5 * TOTAL_LINES
    close(cycles_per_frame % 1.0, 0.0, 1e-12,
          "progressive colour phase repeats every frame")
    assert TOTAL_LINES == 262
    assert ACTIVE_START_LINE + ACTIVE_LINES == 260
    assert CONTENT_START_LINE - ACTIVE_START_LINE == 24
    assert ACTIVE_START_LINE + ACTIVE_LINES - (
        CONTENT_START_LINE + CONTENT_LINES) == 24
    display_aspect = 320.0 / SOURCE_LINES
    close(display_aspect, 5.0 / 3.0, 1e-15,
          "320x192 native presentation aspect")


def test_uhd_raster_layout() -> None:
    assert (RASTER_WIDTH, RASTER_HEIGHT) == (3840, 2160)
    assert TUBE_WIDTH == 320 * NATIVE_SCALE
    assert TUBE_HEIGHT == 192 * NATIVE_SCALE
    assert TUBE_X * 2 + TUBE_WIDTH == RASTER_WIDTH
    assert TUBE_Y * 2 + TUBE_HEIGHT == RASTER_HEIGHT
    close(TUBE_WIDTH / TUBE_HEIGHT, 5.0 / 3.0, 1e-15,
          "UHD tube presentation aspect")


def test_ntsc_setup_roundtrip() -> None:
    setup = 0.075
    excursion = 0.925
    for source in (0.0, 0.18, 0.50, 0.75, 1.0):
        composite = setup + source * excursion
        decoded = (composite - setup) / excursion
        close(decoded, source, 1e-12, "7.5 IRE setup roundtrip")


def test_yiq_primary_roundtrip() -> None:
    rgb_to_yiq = (
        (0.299000, 0.587000, 0.114000),
        (0.595716, -0.274453, -0.321263),
        (0.211456, -0.522591, 0.311135),
    )
    yiq_to_rgb = (
        (1.0, 0.9563, 0.6210),
        (1.0, -0.2721, -0.6474),
        (1.0, -1.1070, 1.7046),
    )

    def multiply(matrix: tuple[tuple[float, ...], ...], vector: tuple[float, ...]) -> tuple[float, ...]:
        return tuple(sum(row[i] * vector[i] for i in range(3)) for row in matrix)

    for name, primary in (
        ("red", (1.0, 0.0, 0.0)),
        ("green", (0.0, 1.0, 0.0)),
        ("blue", (0.0, 0.0, 1.0)),
        ("white", (1.0, 1.0, 1.0)),
    ):
        decoded = multiply(yiq_to_rgb, multiply(rgb_to_yiq, primary))
        for channel, (actual, expected) in enumerate(zip(decoded, primary)):
            close(actual, expected, 0.0007, f"YIQ {name} channel {channel}")


def test_quadrature_modulation() -> None:
    # The encoder and synchronous detector must agree on the 33-degree I/Q
    # axes. Integrating an integer number of carrier cycles rejects cross-talk.
    samples = 720
    source_i, source_q = 0.37, -0.21
    decoded_i = 0.0
    decoded_q = 0.0
    for index in range(samples):
        phase = 2.0 * math.pi * 12.0 * (index + 0.5) / samples + math.radians(33.0)
        chroma = source_i * math.cos(phase) + source_q * math.sin(phase)
        decoded_i += chroma * 2.0 * math.cos(phase) / samples
        decoded_q += chroma * 2.0 * math.sin(phase) / samples
    close(decoded_i, source_i, 1e-12, "quadrature I")
    close(decoded_q, source_q, 1e-12, "quadrature Q")


def test_two_line_cross_color_suppression() -> None:
    def line_comb(previous: float, current: float, following: float) -> float:
        return 0.5 * current - 0.25 * (previous + following)

    # The NTSC carrier reverses on the adjacent lines, so genuine chroma is
    # reconstructed at unity gain.
    close(line_comb(-0.37, 0.37, -0.37), 0.37, 1e-15,
          "two-line comb genuine chroma")
    # Monochrome detail at the same horizontal phase on every line cancels.
    close(line_comb(0.37, 0.37, 0.37), 0.0, 1e-15,
          "two-line comb cross-colour rejection")


def test_phosphor_weight_normalization() -> None:
    medium_weight = 0.065
    slow_weight = 0.028
    fast_weight = 1.0 - medium_weight - slow_weight
    close(fast_weight + medium_weight + slow_weight, 1.0, 1e-12,
          "phosphor component energy")


def test_beam_energy_normalization() -> None:
    # Numerical integration of the configured generalized-Gaussian spot must
    # preserve one line-average unit for both narrow and bright broad beams.
    shape = 2.2
    for width in (0.20, 0.37, 0.54):
        step = 0.0005
        samples = 20_001
        integral = 0.0
        for index in range(samples):
            x = (index - (samples - 1) / 2.0) * step
            integral += math.exp(-abs(x / width) ** shape) * step
        exact_area = 2.0 * width * math.gamma(1.0 + 1.0 / shape)
        close(integral, exact_area, 2e-6,
              f"generalized beam integral width={width}")
        normalized = integral / exact_area
        close(normalized, 1.0, 2e-6,
              f"normalized beam energy width={width}")


def test_front_face_luminance_calibration() -> None:
    # A photometer reads the spatially averaged white after the slot mask,
    # phosphor transfer and faceplate. The arbitrary simulated radiance of
    # that reference must cancel exactly before conversion to host nits.
    reference_white_radiance = 0.48
    tube_peak_nits = 125.0
    host_peak_nits = 160.0
    output_linear = (reference_white_radiance * tube_peak_nits /
                     host_peak_nits / reference_white_radiance)
    close(output_linear, 0.78125, 1e-15,
          "front-face reference white on calibrated host")


def raster_strikes(start: float, end: float, offset: float,
                   period: float) -> list[float]:
    first = math.floor((start - offset) / period) + 1
    last = math.floor((end - offset) / period)
    return [index * period + offset for index in range(first, last + 1)]


def analytic_strike_range(excitation: float, first_frame: int,
                          last_frame: int, strike_offset: float,
                          end_time: float, period: float,
                          tau: float) -> tuple[float, float]:
    """CPU reference for phosphor_state.fs::accumulateStrikes."""
    if last_frame < first_frame:
        return 0.0, 0.0
    count = last_frame - first_frame + 1
    newest_strike = last_frame * period + strike_offset
    newest_decay = math.exp(-max(end_time - newest_strike, 0.0) / tau)
    per_frame_decay = math.exp(-period / tau)
    geometric = (1.0 - per_frame_decay ** count) / (1.0 - per_frame_decay)
    endpoint = excitation * period / tau * newest_decay * geometric
    integral = max(excitation * period * count - endpoint * tau, 0.0)
    return endpoint, integral


def explicit_strike_range(excitation: float, strikes: list[float],
                          end_time: float, period: float,
                          tau: float) -> tuple[float, float]:
    endpoint = 0.0
    integral = 0.0
    for strike in strikes:
        event_endpoint = (excitation * period / tau *
                          math.exp(-(end_time - strike) / tau))
        endpoint += event_endpoint
        integral += excitation * period - event_endpoint * tau
    return endpoint, integral


def test_analytic_phosphor_exposure() -> None:
    period = 1.0 / FRAME_HZ
    offset = 100.0 / LINE_HZ + 9.40e-6 + 0.5 * 52.655e-6
    for tau in (0.0010, 0.0055, 0.024):
        for dt in (1.0 / 144.0, 1.0 / 60.0, 0.333, 10.0):
            end = 0.31 * period
            start = end - dt
            strikes = raster_strikes(start, end, offset, period)
            first = math.floor((start - offset) / period) + 1
            last = math.floor((end - offset) / period)
            analytic = analytic_strike_range(
                0.73, first, last, offset, end, period, tau)
            explicit = explicit_strike_range(
                0.73, strikes, end, period, tau)
            close(analytic[0], explicit[0], 2e-12,
                  f"analytic endpoint tau={tau} dt={dt}")
            close(analytic[1], explicit[1], 2e-12,
                  f"analytic exposure tau={tau} dt={dt}")

            initial = 0.41
            decay = math.exp(-dt / tau)
            endpoint = initial * decay + analytic[0]
            integrated = initial * tau * (1.0 - decay) + analytic[1]
            assert math.isfinite(endpoint) and endpoint >= 0.0
            assert math.isfinite(integrated) and integrated >= 0.0
            assert math.isfinite(integrated / dt)


def test_synchronized_phosphor_exposure() -> None:
    period = 1.0 / FRAME_HZ
    excitation = 0.73
    strike_offset = 0.61 * period
    for tau in (0.0010, 0.0055, 0.024):
        per_frame_decay = math.exp(-period / tau)
        for phase in (0.0, 0.13, 0.60, 0.61, 0.87, 0.999):
            end = phase * period
            latest_strike = (strike_offset if end >= strike_offset
                             else strike_offset - period)
            endpoint = (excitation * period / tau *
                        math.exp(-(end - latest_strike) / tau) /
                        (1.0 - per_frame_decay))
            time_to_strike = strike_offset - end
            if time_to_strike <= 1e-7:
                time_to_strike += period
            post_strike = max(period - time_to_strike, 0.0)
            integrated = endpoint * tau * (1.0 - per_frame_decay)
            integrated += excitation * period * (
                1.0 - math.exp(-post_strike / tau))
            close(integrated / period, excitation, 2e-12,
                  f"synchronized exposure tau={tau} phase={phase}")


def test_microsecond_raster_energy() -> None:
    period = 1.0 / FRAME_HZ
    tau = 0.0014
    offset = 100.0 / LINE_HZ + 9.40e-6 + 0.5 * 52.655e-6
    # Over four progressive frames every phosphor location is struck exactly
    # four times. An impulse of E*T/tau has a period-average radiance E.
    strikes = raster_strikes(offset, offset + 4.0 * period,
                             offset, period)
    assert len(strikes) == 4
    impulse = period / tau
    energy_per_period = impulse * tau / period
    close(energy_per_period, 1.0, 1e-12, "raster impulse energy")

    def endpoint(refresh: float) -> float:
        state = 0.0
        duration = 1.0
        steps = round(duration * refresh)
        impulse_value = period / tau
        for step in range(steps):
            start = step / refresh
            end = (step + 1) / refresh
            state *= math.exp(-(end - start) / tau)
            for strike in raster_strikes(start, end, offset, period):
                state += impulse_value * math.exp(-(end - strike) / tau)
        return state

    reference = endpoint(60.0)
    close(endpoint(120.0), reference, 2e-12,
          "event phosphor endpoint at 120 Hz")
    close(endpoint(144.0), reference, 2e-12,
          "event phosphor endpoint at 144 Hz")


def rc_step(previous: float, target: float, dt: float, tau: float) -> float:
    return target + (previous - target) * math.exp(-dt / tau)


def test_supply_rc_invariance() -> None:
    target = 0.94
    for refresh in (60.0, 120.0, 144.0):
        voltage = 1.0
        dt = 1.0 / refresh
        for _ in range(round(refresh)):
            voltage = rc_step(voltage, target, dt, 0.080)
        close(voltage, rc_step(1.0, target, 1.0, 0.080), 2e-6,
              f"EHT RC at {refresh:g} Hz")


def test_faceplate_optics() -> None:
    refractive_index = 1.52
    f0 = ((refractive_index - 1.0) /
          (refractive_index + 1.0)) ** 2
    close(f0, 0.04257999496094735, 1e-12, "glass normal Fresnel")
    absorption = (0.0060, 0.0045, 0.0035)
    transmission = tuple(math.exp(-coefficient * 11.0)
                         for coefficient in absorption)
    assert 0.90 < transmission[0] < transmission[1] < transmission[2] < 1.0


def test_edge_anchored_geometry() -> None:
    def mapped(x: float, y: float, curvature_x: float,
               curvature_y: float, pincushion: float) -> tuple[float, float]:
        radius2 = x * x + y * y
        scale_x = ((1.0 + curvature_x * radius2) /
                   (1.0 + curvature_x * (1.0 + y * y)))
        scale_y = ((1.0 + curvature_y * radius2) /
                   (1.0 + curvature_y * (1.0 + x * x)))
        source_x = (x * scale_x + pincushion * x * y * y *
                    max(1.0 - x * x, 0.0))
        source_y = (y * scale_y + pincushion * y * x * x *
                    max(1.0 - y * y, 0.0))
        return source_x, source_y

    for coordinate in (-1.0, -0.7, 0.0, 0.7, 1.0):
        close(mapped(-1.0, coordinate, 0.055, 0.075, 0.012)[0],
              -1.0, 1e-12, "left raster edge")
        close(mapped(1.0, coordinate, 0.055, 0.075, 0.012)[0],
              1.0, 1e-12, "right raster edge")
        close(mapped(coordinate, -1.0, 0.055, 0.075, 0.012)[1],
              -1.0, 1e-12, "top raster edge")
        close(mapped(coordinate, 1.0, 0.055, 0.075, 0.012)[1],
              1.0, 1e-12, "bottom raster edge")

    # Residual geometry is present but cannot become another large barrel
    # transform or push an interior sample outside the source rectangle.
    for y_index in range(21):
        for x_index in range(21):
            x = x_index / 10.0 - 1.0
            y = y_index / 10.0 - 1.0
            source_x, source_y = mapped(x, y, 0.055, 0.075, 0.012)
            assert -1.0 <= source_x <= 1.0
            assert -1.0 <= source_y <= 1.0
            assert abs(source_x - x) < 0.022
            assert abs(source_y - y) < 0.027


def test_vertical_interval_contract() -> None:
    line_period_us = 1_000_000.0 / LINE_HZ
    equalizing_width_us = 2.30
    broad_width_us = line_period_us * 0.5 - 4.70
    assert 2.0 < equalizing_width_us < 2.6
    assert 26.5 < broad_width_us < 27.5
    assert broad_width_us < line_period_us * 0.5

    pulse_kinds = []
    for half_line in range(18):
        line = half_line // 2
        pulse_kinds.append("equalizing" if line < 3 or line >= 6 else "broad")
    assert pulse_kinds.count("equalizing") == 12
    assert pulse_kinds.count("broad") == 6


def test_progressive_scanout_geometry() -> None:
    frame_period = 1.0 / FRAME_HZ

    def strike_phase(active_row: float, x: float = 0.5) -> float:
        raster_line = CONTENT_START_LINE + active_row
        active_time = 9.40e-6 + x * 52.655e-6
        return (raster_line / LINE_HZ + active_time) / frame_period

    top = strike_phase(0.0, 0.0)
    middle_top = strike_phase(48.0, 0.0)
    middle_bottom = strike_phase(144.0, 1.0)
    bottom = strike_phase(CONTENT_LINES - 1.0, 1.0)
    assert 0.0 < top < middle_top < middle_bottom < bottom < 1.0
    # Progressive landing is identical on every frame; there is no parity term.
    landing = strike_phase(100.0)
    first_strike = 3.0 * frame_period + landing * frame_period
    next_strike = 4.0 * frame_period + landing * frame_period
    close(next_strike - first_strike, frame_period, 1e-15,
          "same-line progressive repeat period")


def test_skipped_host_frame_history() -> None:
    def uses_current_emission(frames_advanced: int,
                              synthetic_frame: int) -> bool:
        first_current_frame = 1 - max(frames_advanced, 1)
        return synthetic_frame >= first_current_frame

    assert not uses_current_emission(1, -1)
    assert uses_current_emission(1, 0)
    assert not uses_current_emission(2, -2)
    assert uses_current_emission(2, -1)
    assert uses_current_emission(2, 0)
    assert uses_current_emission(600, -599)
    # Once more than one full CRT period elapsed, the progressive scan has
    # covered the whole current framebuffer and no stale partial image remains.
    def scan_mix(frames_advanced: int, strike_phase: float,
                 current_phase: float) -> float:
        return 1.0 if frames_advanced > 1 else float(
            strike_phase <= current_phase)

    assert scan_mix(1, 0.90, 0.10) == 0.0
    assert scan_mix(2, 0.90, 0.10) == 1.0


def test_raster_timed_mains_ripple() -> None:
    frame_period = 1.0 / FRAME_HZ
    frame_start = 12.0 - 0.37 * frame_period

    def raster_time(y: float, x: float) -> float:
        line = CONTENT_START_LINE + y * CONTENT_LINES
        active_time = 9.40e-6 + x * 52.655e-6
        return frame_start + line / LINE_HZ + active_time

    top_left = raster_time(0.0, 0.0)
    bottom_right = raster_time(1.0, 1.0)
    assert bottom_right > top_left
    phase_span = 120.0 * (bottom_right - top_left)
    assert 1.45 < phase_span < 1.48


def main() -> int:
    test_signal_clock()
    test_native_240p_frame()
    test_uhd_raster_layout()
    test_ntsc_setup_roundtrip()
    test_yiq_primary_roundtrip()
    test_quadrature_modulation()
    test_two_line_cross_color_suppression()
    test_phosphor_weight_normalization()
    test_beam_energy_normalization()
    test_front_face_luminance_calibration()
    test_analytic_phosphor_exposure()
    test_synchronized_phosphor_exposure()
    test_microsecond_raster_energy()
    test_supply_rc_invariance()
    test_faceplate_optics()
    test_edge_anchored_geometry()
    test_vertical_interval_contract()
    test_progressive_scanout_geometry()
    test_skipped_host_frame_history()
    test_raster_timed_mains_ripple()
    print("OK: native 240p timing, raster, colour phase, YIQ, setup, exact phosphor exposure and supply invariants")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
