## Format for an S7comm adapter for S7 300/400

# All configs for S7comm adapters must follow this scheme, depending on use case (db, a, e, m, t, c, arr)
# Example

{
  "connection": {
    "ip": "192.168.10.20",
    "rack": 0,
    "slot": 2
  },
  "readBlocks": [

{ "mode": "single", 
  "tags": [
    { "id": 0, "name": "Temperature", "target": "db", "dbno": 1, "offset": 0, "type": "REAL" },
    { "id": 1, "name": "AnalogInput", "target": "e", "offset": 212, "type": "WORD" },
    { "id": 2, "name": "AutoActive", "target": "a", "offset": 50, "type": "BOOL", "bit": 5 },
    { "id": 3, "name": "ErrorActive", "target": "m", "offset": 3, "type": "BOOL", "bit": 7 },
    { "id": 4, "name": "AutoTimer", "target": "t", "tno": 65, "type": "TIMER" },
    { "id": 5, "name": "AutoCounter", "target": "c", "cno": 255, "type": "COUNTER" },
    { "id": 6, "name": "Temperature", "target": "db", "dbno": 1, "offset": 0, "type": "REAL" },
    { "id": 7, "name": "AnalogInput", "target": "e", "offset": 212, "type": "WORD" },
    { "id": 8, "name": "AutoActive", "target": "a", "offset": 50, "type": "BOOL", "bit": 5 },
    { "id": 9, "name": "ErrorActive", "target": "m", "offset": 3, "type": "BOOL", "bit": 7 },
    { "id": 10, "name": "AutoTimer", "target": "t", "tno": 65, "type": "TIMER" },
    { "id": 11, "name": "AutoCounter", "target": "c", "cno": 255, "type": "COUNTER" },
    { "id": 12, "name": "Temperature", "target": "db", "dbno": 1, "offset": 0, "type": "REAL" },
    { "id": 13, "name": "AnalogInput", "target": "e", "offset": 212, "type": "WORD" },
    { "id": 14, "name": "AutoActive", "target": "a", "offset": 50, "type": "BOOL", "bit": 5 },
    { "id": 15, "name": "ErrorActive", "target": "m", "offset": 3, "type": "BOOL", "bit": 7 },
    { "id": 16, "name": "Temperature", "target": "db", "dbno": 1, "offset": 0, "type": "REAL" }, 
    { "id": 17, "name": "AnalogInput", "target": "e", "offset": 212, "type": "WORD" },           
    { "id": 18, "name": "AutoActive", "target": "a", "offset": 50, "type": "BOOL", "bit": 5 },   
    { "id": 19, "name": "ErrorActive", "target": "m", "offset": 3, "type": "BOOL", "bit": 7 },   
    { "id": 21, "name": "AutoTimer", "target": "t", "tno": 65, "type": "TIMER" },                
    { "id": 22, "name": "AutoCounter", "target": "c", "cno": 255, "type": "COUNTER" },           
    { "id": 23, "name": "Temperature", "target": "db", "dbno": 1, "offset": 0, "type": "REAL" }, 
    { "id": 24, "name": "AnalogInput", "target": "e", "offset": 212, "type": "WORD" },           
    { "id": 25, "name": "AutoActive", "target": "a", "offset": 50, "type": "BOOL", "bit": 5 },   
    { "id": 26, "name": "ErrorActive", "target": "m", "offset": 3, "type": "BOOL", "bit": 7 },   
    { "id": 27,  "name": "AutoTimer", "target": "t", "tno": 65, "type": "TIMER" },               
    { "id": 28,  "name": "AutoCounter", "target": "c", "cno": 255, "type": "COUNTER" },          
    { "id": 29,  "name": "Temperature", "target": "db", "dbno": 1, "offset": 0, "type": "REAL" },
    { "id": 30, "name": "AutoCounter", "target": "c", "cno": 255, "type": "COUNTER" }
    ]
  }
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
    "ip": "192.168.10.20",
    "rack": 0,
    "slot": 2
  },
  "readBlocks": [

{ "mode": "area", " 
  "tags": [
    { "id": 0, "name": "Temperature", "target": "db", "dbno": 1, "offset": 0, "type": "REAL" },
    { "id": 1, "name": "AnalogInput", "target": "e", "offset": 212, "type": "WORD" },
    { "id": 2, "name": "AutoActive", "target": "a", "offset": 50, "type": "BOOL", "bit": 5 },
    { "id": 3, "name": "ErrorActive", "target": "m", "offset": 3, "type": "BOOL", "bit": 7 },
    { "id": 4, "name": "AutoTimer", "target": "t", "tno": 65, "type": "TIMER" },
    { "id": 5, "name": "AutoCounter", "target": "c", "cno": 255, "type": "COUNTER" },
    { "id": 6, "name": "Temperature", "target": "db", "dbno": 1, "offset": 0, "type": "REAL" },
    { "id": 7, "name": "AnalogInput", "target": "e", "offset": 212, "type": "WORD" },
    { "id": 8, "name": "AutoActive", "target": "a", "offset": 50, "type": "BOOL", "bit": 5 },
    { "id": 9, "name": "ErrorActive", "target": "m", "offset": 3, "type": "BOOL", "bit": 7 },
    { "id": 10, "name": "AutoTimer", "target": "t", "tno": 65, "type": "TIMER" },
    { "id": 11, "name": "AutoCounter", "target": "c", "cno": 255, "type": "COUNTER" },
    { "id": 12, "name": "Temperature", "target": "db", "dbno": 1, "offset": 0, "type": "REAL" },
    { "id": 13, "name": "AnalogInput", "target": "e", "offset": 212, "type": "WORD" },
    { "id": 14, "name": "AutoActive", "target": "a", "offset": 50, "type": "BOOL", "bit": 5 },
    { "id": 15, "name": "ErrorActive", "target": "m", "offset": 3, "type": "BOOL", "bit": 7 },
    { "id": 16, "name": "Temperature", "target": "db", "dbno": 1, "offset": 0, "type": "REAL" }, 
    { "id": 17, "name": "AnalogInput", "target": "e", "offset": 212, "type": "WORD" },           
    { "id": 18, "name": "AutoActive", "target": "a", "offset": 50, "type": "BOOL", "bit": 5 },   
    { "id": 19, "name": "ErrorActive", "target": "m", "offset": 3, "type": "BOOL", "bit": 7 },   
    { "id": 21, "name": "AutoTimer", "target": "t", "tno": 65, "type": "TIMER" },                
    { "id": 22, "name": "AutoCounter", "target": "c", "cno": 255, "type": "COUNTER" },           
    { "id": 23, "name": "Temperature", "target": "db", "dbno": 1, "offset": 0, "type": "REAL" }, 
    { "id": 24, "name": "AnalogInput", "target": "e", "offset": 212, "type": "WORD" },           
    { "id": 25, "name": "AutoActive", "target": "a", "offset": 50, "type": "BOOL", "bit": 5 },   
    { "id": 26, "name": "ErrorActive", "target": "m", "offset": 3, "type": "BOOL", "bit": 7 },   
    { "id": 27,  "name": "AutoTimer", "target": "t", "tno": 65, "type": "TIMER" },               
    { "id": 28,  "name": "AutoCounter", "target": "c", "cno": 255, "type": "COUNTER" },          
    { "id": 29,  "name": "Temperature", "target": "db", "dbno": 1, "offset": 0, "type": "REAL" },
    { "id": 30, "name": "AutoCounter", "target": "c", "cno": 255, "type": "COUNTER" }
    ]
  }
]
}