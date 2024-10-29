from sgp4.api import Satrec
import json

def propagate_satellite(tle_line1: str, tle_line2: str, start_time: float, stop_time: float, step_size: float):
    """
    Propagate satellite using SGP4 from TLE data.
    Returns the satellite's position and velocity at each time step in JSON format.
    """

    satellite = Satrec.twoline2rv(tle_line1.strip(), tle_line2.strip())

    epoch_days = satellite.jdsatepoch

    results = []

    for minutes_since_epoch in range(int(start_time), int(stop_time), int(step_size)):
        jd = epoch_days + minutes_since_epoch / (24 * 60)

        error_code, r, v = satellite.sgp4(jd, 0.0)

        if error_code == 0:
            results.append({
                "tsince_min": minutes_since_epoch,
                "x_km": r[0],
                "y_km": r[1],
                "z_km": r[2],
                "xdot_km_per_s": v[0],
                "ydot_km_per_s": v[1],
                "zdot_km_per_s": v[2]
            })
        else:
            results.append({
                "tsince_min": minutes_since_epoch,
                "error": "Error in propagation"
            })

    return json.dumps(results)
