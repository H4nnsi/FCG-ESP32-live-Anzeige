obslua = require("obslua")

-- Standardmäßig eine oder mehrere IPs mit Komma getrennt eintragen
local default_ips = ""
local esp_ips_raw = default_ips

-- Hilfsfunktion: Teilt einen String am Komma in eine Array-Tabelle auf
function split_ips(inputstr)
    local t = {}
    if inputstr == nil or inputstr == "" then
        return t
    end
    for str in string.gmatch(inputstr, "([^,]+)") do
        -- Entfernt Leerzeichen vor/nach der IP-Adresse
        local clean_ip = string.gsub(str, "%s+", "")
        if clean_ip ~= "" then
            table.insert(t, clean_ip)
        end
    end
    return t
end

-- Sende-Funktion mit Schleife durch alle IP-Adressen
function trigger_esp_all(state)
    local raw_str = esp_ips_raw
    if raw_str == nil or raw_str == "" then
        raw_str = default_ips
    end

    -- Erstellt das Array aus dem Eingabetext
    local ip_list = split_ips(raw_str)

    if #ip_list == 0 then
        print("[ESP32 Skript] FEHLER: Keine IP-Adressen konfiguriert!")
        return
    end

    -- SCHLEIFE (Loop): Geht jede IP-Adresse im Array nacheinander durch
    for index, ip in ipairs(ip_list) do
        local url = "http://" .. ip .. "/live?state=" .. state
        
        -- Befehl im Hintergrund senden (mit 1s Timeout je Anfrage)
        local cmd = string.format('start /b curl -s --max-time 1 "%s"', url)
        os.execute(cmd)
    end
end

-- Timer-Funktion: Sendet den Heartbeat an alle Uhren im Array
function heartbeat_tick()
    trigger_esp_all("keepalive")
end

-- Manuelle Test-Buttons
function test_on_clicked(props, p)
    print("[ESP32 Skript] Manueller Test an alle Uhren: LIVE ON")
    trigger_esp_all("on")
    return true
end

function test_off_clicked(props, p)
    print("[ESP32 Skript] Manueller Test an alle Uhren: LIVE OFF")
    trigger_esp_all("off")
    return true
end

-- Eingabemaske im OBS-Skript-Menü
function script_properties()
    local props = obslua.obs_properties_create()
    
    -- Eingabefeld für mehrere IP-Adressen
    obslua.obs_properties_add_text(
        props, 
        "esp_ips", 
        "ESP32 IP-Adressen (mit Komma trennen)", 
        obslua.OBS_TEXT_DEFAULT
    )
    
    -- Test-Buttons
    obslua.obs_properties_add_button(props, "btn_on", "Alle testen: ON (Rot)", test_on_clicked)
    obslua.obs_properties_add_button(props, "btn_off", "Alle testen: OFF (Türkis)", test_off_clicked)
    
    return props
end

function script_update(settings)
    local input_ips = obslua.obs_data_get_string(settings, "esp_ips")
    
    if input_ips ~= nil and input_ips ~= "" then
        esp_ips_raw = input_ips
    else
        esp_ips_raw = default_ips
    end
    
    print("[ESP32 Skript] Aktive IP-Adressen:")
    local current_list = split_ips(esp_ips_raw)
    for i, ip in ipairs(current_list) do
        print(string.format("  -> Uhr %d: %s", i, ip))
    end
end

-- Event-Erkennung (Stream Start / Stop)
function on_event(event)
    if event == obslua.OBS_FRONTEND_EVENT_STREAMING_STARTED then
        print("[ESP32 Skript] STREAM GESTARTET -> Sende ON an alle Uhren")
        trigger_esp_all("on")
        -- Startet den 5-Sekunden-Heartbeat für alle Uhren
        obslua.timer_add(heartbeat_tick, 5000)
        
    elseif event == obslua.OBS_FRONTEND_EVENT_STREAMING_STOPPED then
        print("[ESP32 Skript] STREAM GESTOPPT -> Sende OFF an alle Uhren")
        obslua.timer_remove(heartbeat_tick)
        trigger_esp_all("off")
    end
end

function script_unload()
    obslua.timer_remove(heartbeat_tick)
end

function script_load(settings)
    obslua.obs_frontend_add_event_callback(on_event)
    print("[ESP32 Skript] Multi-Uhr Skript geladen!")
end
