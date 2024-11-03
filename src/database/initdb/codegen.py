import json

with open('../../../resources/satellite_data.json', 'r') as f:
    data = json.load(f)

with open('init_satellites.sql', 'w') as f:
    f.write("""
CREATE TABLE IF NOT EXISTS satellites (
    satellite_id INTEGER PRIMARY KEY,
    name TEXT NOT NULL,
    description TEXT
);

BEGIN;
""")
    for satellite in data:
        norad_id = satellite['NORAD_ID']
        name = satellite['Current_Official_Name'].replace("'", "''")
        source = satellite.get('Source', '').replace("'", "''") if satellite.get('Source') else ''
        f.write(f"INSERT INTO satellites (satellite_id, name, description) VALUES ({norad_id}, '{name}', '{source}') ON CONFLICT DO NOTHING;\n")
    f.write('COMMIT;\n')
