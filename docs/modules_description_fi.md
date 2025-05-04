# Ohjelmistomoduulien vuorovaikutus

## Pääkomponentit

### Pääsovellus (main.c)
- Koordinoi kaikkien moduulien toimintaa
- Hallitsee laitteen elinkaarta
- Käsittelee virheet ja järjestelmän tilat

### Tiedonkeruu
- `sensors.c`: Hallinnoi BLE-skannausta ja RuuviTag-tietojen käsittelyä
- `battery_monitor.c`: Seuraa akun tilaa
- `time_manager.c`: Synkronoi ajan GSM:n kautta

### Tietojen tallennus
- `storage.c`: Hallinnoi SPIFFS- ja NVS-tallennusta
- `system_states.c`: Valvoo järjestelmän tiloja NVS:n kautta
- `json_helper.c`: Muotoilee tiedot tallennusta ja lähetystä varten
- `config_manager.c`: Hallinnoi järjestelmän konfiguraatiota

### Viestintä ja raportointi
- `gsm_modem.cpp`: Hallinnoi GSM-moduulia
- `discord_api.c`: Lähettää tiedot Discordiin
- `firebase_api.c`: Lähettää tiedot Firebaseen
- `reporter.c`: Koordinoi tietojen lähetystä ja raportointia

### Virranhallinta
- `power_management.c`: Hallinnoi virtatiloja
- Ohjaa deep sleep -tilaa
- Optimoi virrankulutusta

## Vuorovaikutussekvenssi

1. Alustus:
   - Main → Power Management 
   - Main → System States → Storage → NVS
   - Main → Config Manager → NVS
   - Time Manager → GSM → Ajan synkronointi

2. Tiedonkeruu:
   - Main → Sensors → BLE → RuuviTag
   - Main → Storage → SPIFFS
   - Main → Battery Monitor → ADC

3. Käsittely ja lähetys:
   - Storage → Reporter → JSON Helper
   - Reporter → Config Manager → Lähetysasetukset
   - Reporter → GSM → Discord/Firebase
   - Reporter → Storage (lokit)

4. Virranhallinta:
   - Main → Power Management
   - Main → GSM- ja BT-moduulit (deinit) 