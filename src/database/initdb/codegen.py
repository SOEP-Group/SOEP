import json

def get_numeric_value(satellite, key):
    value = satellite.get(key)
    if value is None or value == '':
        return 'NULL'
    else:
        return value

def escape_string(value):
    return value.replace("'", "''") if value else ''

with open('../../../resources/satellite_data.json', 'r') as f:
    data = json.load(f)

with open('init_satellites.sql', 'w') as f:
    f.write("""
CREATE TABLE IF NOT EXISTS satellites (
    satellite_id INTEGER PRIMARY KEY,
    name TEXT NOT NULL,
    object_id TEXT,
    object_type TEXT,
    status TEXT,
    owner TEXT,
    launch_date TEXT,
    launch_site TEXT,
    revolution REAL,
    inclination REAL,
    farthest_orbit_distance INTEGER,
    lowest_orbit_distance INTEGER,
    RCS REAL,
    description TEXT
);

BEGIN;
""")
    for satellite in data:
        norad_id = satellite['norad_id']
        name = escape_string(satellite['name'])
        object_id = escape_string(satellite.get('object_id', ''))
        object_type = escape_string(satellite.get('object_type', ''))
        status = escape_string(satellite.get('status', ''))
        owner = escape_string(satellite.get('owner', ''))
        launch_date = escape_string(satellite.get('launch_date', ''))
        launch_site = escape_string(satellite.get('launch_site', ''))
        revolution = get_numeric_value(satellite, 'revolution')
        inclination = get_numeric_value(satellite, 'inclination')
        farthest_orbit_distance = get_numeric_value(satellite, 'farthest_orbit_distance')
        lowest_orbit_distance = get_numeric_value(satellite, 'lowest_orbit_distance')
        rcs = get_numeric_value(satellite, 'RCS')
        description = escape_string(satellite.get('description', ''))

        f.write(f"""INSERT INTO satellites (satellite_id, name, object_id, object_type, status, owner, launch_date, launch_site, revolution, inclination, farthest_orbit_distance, lowest_orbit_distance, rcs, description)
VALUES ({norad_id}, '{name}', '{object_id}', '{object_type}', '{status}', '{owner}', '{launch_date}', '{launch_site}', {revolution}, {inclination}, {farthest_orbit_distance}, {lowest_orbit_distance}, {rcs}, '{description}')
ON CONFLICT DO NOTHING;\n""")
    f.write('COMMIT;\n')
