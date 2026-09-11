## Format for an S7comm adapter for S7 300/400

# All configs for S7comm adapters must follow this scheme, depending on use case (db, a, e, m, t, c, arr)
# Example

{
  "connection": {
    "ip": "192.168.0.1",
    "rack": 0,
    "slot": 2
  },
  "tags": [
    { "id": 0, "name": "Temperature", "target": "db", "dbno": 1, "offset": 0, "type": "REAL" },
    { "id": 1, "name": "AnalogInput", "target": "e", "offset": 212, "type": "WORD" },
    { "id": 2, "name": "AutoActive", "target": "a", "offset": 50, "type": "BOOL", "bit": 5 },
    { "id": 3, "name": "ErrorActive", "target": "m", "offset": 3, "type": "BOOL", "bit": 7 },
    { "id": 4, "name": "AutoTimer", "target": "t", "tno": 65 },
    { "id": 5, "name": "AutoCounter", "target": "c", "cno": 255 },
    { "id": 6, "name": "PartnumbersArray", "target": "arr", "dbno": 50, "offset": 26, "type": "INT", "amount": 25  }
  ]
}

# Rack and slot are usually 0, 2 but check it against zour TIA/Step7 project
# Tags are the variables to be extracted
# It is more convenient to read a big consecutive field of data instead of reading in 20 different DBs

# You can also pack the required data into a dedicated DB, therefore the whole area can be read at once
# CAUTION: This only works if the given data is consecutive!

# Example

{
  "connection": {
    "ip": "192.168.0.1",
    "rack": 0,
    "slot": 2
  },
  "area":{
    "dbno": 25,
    "offset": 20,
    "tags": [
      { "id": 0, "name": "Temperature", "type": "REAL" },
      { "id": 1, "name": "Partcounter", "type": "DINT" },
      { "id": 2, "name": "Cycletime", "type": "REAL" },
      { "id": 3, "name": "ErrorActive", "type": "BOOL", "bit": 6 },
      { "id": 4, "name": "AutoActive", "type": "BOOL", "bit": 7 }
    ]
  }
}