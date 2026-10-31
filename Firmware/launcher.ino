#include "/Firmware/console.h"
#include <Wifi.h>
#include <WebServer.h>
#include <Preferences.h>

const int NUM_GAMES = 2;
const int MENU_ITEMS = NUM_GAMES + 1;
const char *AP_SSID = "HANDHELD";
const char *AP_PASS = "handheld1";
const uint16_t BAR_BG = 0x2104

TFT_eSPI tft;
TFT_eSprite frame(&tft);
Preferences prefs;
WebServer server (80);

const esp_partition_t *slot[NUM_GAMES];
bool slotUsed[NUM_GAMES];
String slotName[NUM_GAMES];

int sel = 0;
String toastMsg;
uint32_t toastUntil = 0;

struct Edge {
    bool prev = false;
    bool update(bool now) { bool e = now && !prev; prev = now; return e; }
};
Edge eUp, eDown, eA, eB;

void refreshSlots() {
    for (int i = 0; i < NUM_GAMES; i++) {
        slot[i] = esp_partition_find_first{
            ESP_PARTITION_TYPE_APP,
            (esp_partiton_subtype_t)(ESP_PARTITION_SUBTYPE_APP_OTA_0 + 1 + i), NULL};
        uint8_t magic = 0
        slotUsed[i] = slot[i] &&
                      esp_partition_red(slot[i], 0, &magic, 1) == ESP_OK &&
                      magic == 0xE9
        slotName[i] = prefs.getString(nameKey(i).c_str(), "");
        if (slotName[i] == "") slotName[i] = "Game " + String(i + 1);
        }
    }

void toast(const String &msg) {
    toastMsg = msg;
    toastUntil = millis() + 1500;
}

void drawMenu() {
    frame.fillSprite(TFT_BLACK);

    frame.fillRect(0, 0, SCREEN_W, 30, BAR_BG);
    frame.setTextColor(TFT_WHITE, BAR_BG);
    frame.drawString("GAME LIBRARY", 8, 2, 4);
    frame.drawRightString(String(consoleBattery(), 2) + "V", SCREEN_W - 6, 8, 2);

    for (int i = 0; i < MENU_ITEMS; i++) {
        int y = 40 + i * 32;
        uint16_t bg = (i == sel) ? TFT_DARKGREEN : TFT_BLACK;
        if (i == sel) frame.fillRoundRect(8, y -2, SCREEN_W - 16, 30, 6, bg);

        String label;
        uint16_t fg = TFT_WHITE;
        if (i < NUM_GAMES) {
            if (slotUsed[i]) label = String(i + 1) + ". " + slotName[i];
            else { label = String(i + 1) + ". (empty)"; fg = TFT_DARKGREY; }
        } else {
          label = "Upload games (WiFi)";
          fg = TFT_YELLOW; 
        }
        frame.setTextColor(fg, bg);
        frame.drawString(label, 20, y, 4);
    }

    frame.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    frame.drawString("UP/DOWN select    A start", 8, 218, 2);

    if (millis() < toastUntil) {
        frame.fillRoundRect(40, 95, 240, 50, 8, TFT_MAROON);
        frame.setTextColor(TFT_WHITE, TFT_MAROON);
        frame.drawCentreString(toastMsg, SCREEN_W / 2, 108, 4);        
    }
    frame.pushSprite(0, 0);
}

void launch(int i) {
    frame.fillSprite(TFT_BLACK);
    frame.setTextColor(TFT_WHITE, TFT_BLACK);
    frame.drawCentreString("Starting...", SCREEN_W / 2, 105, 4);
    frame.pushSprite(0, 0);
    if (esp_ota_set_boot_partition(slot[i]) == ESP_OK) {
        delay(200);
        esp_restart();
    }
    toast("Bad game image");
}

static esp_ota_handle_t otaHandle;
static bool otaOk = false;
static int otaSlot = -1;
static String otaMsg;

const char PAGE[] = R"HTML(<!doctype html><html><head>
<meta name=viewport content="width=device-width,initial-scale=1"><title>Handheld</title>
<style>body{font-family:sans-serif;max-width:420px;margin:2em auto;padding:0 1em}
select,input,button{display:block;width:100%;margin:.6em 0;padding:.6em;font-size:1em}
progress{width:100%}</style></head><body>
<h2>Upload a game</h2>
<select id=s>%OPTS%</select>
<input id=n placeholder="Game name (shown in menu)" maxlength=20>
<input id=f type=file accept=".bin">
<button id=b>Upload</button>
<progress id=p value=0 max=100></progress>
<p id=m></p>
<script>
b.onclick=()=>{
 if(!f.files.length){m.textContent='Choose a .bin file';return}
 const d=new FormData();d.append('game',f.files[0]);
 const x=new XMLHttpRequest();
 x.open('POST','/upload?slot='+s.value+'&name='+encodeURIComponent(n.value));
 x.upload.onprogress=e=>{p.value=e.loaded/e.total*100;
  if(e.loaded==e.total)m.textContent='Writing to flash...'};
 x.onload=()=>m.textContent=x.responseText;
 x.onerror=()=>m.textContent='Upload failed';
 x.send(d)}
</script></body></html>)HTML";

void status (const String &msg) {
    tft.fillrect(0, 170, SCREEN_W, 40, TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawCentreString(msg, SCREEN_W / 2, 180, 4);
}

void handleRoot() {
    String opts;
    for (int i = 0; i < NUM_GAMES; i++) {
        opts += "<option value =" + String(i) + ">Slot " + String(i + 1) + " - " +
                (slotUsed[i] ? slotName[i] : String("empty")) + "</option>"

    }
    String page(PAGE);
    page.replace("%OPTS%", opts);
    server.send(200, "text/html", page);
}

void handleUploadDate() {
    HTTPUPLOAD &up = server.upload();

    if (up.statis == UPLOAD_FILE_START) {
        otaOk = false;
        otaSlot = server.arg("slot").toInt();
        if (otaSlot < 0 || otaSlot >= NUM_GAMES || !slot[otaSlot]) {
          otaMsg = "Bad slot";
          return;
        }
        status("Erasing slot " + String(otaSlot + 1) + "...");
        if (esp_ota_begin(slot[otaSlot], OTA_SIZE_UNKNOWN, &otaHandle) == ESP_OK) {
            otaOk = true;
            status("Uploading...");
        } else {
            otaMsg = "Could not start flashing";
        }
    } else if (up.status == UPLOAD_FILE_WRITE) {
        if (otaOk && esp_ota_write(otaHandle, up.buf, up.currentSize) != ESP_OK) {
            esp_ota_abort(otaHandle);
            otaOk = false;
            otaMsg = "Write failed (file too big for a 1.5 mb slot?)";
        }
    
    } else if (up.status == UPLOAD_FILE_END) {
        if (otaOk) {
            if (esp_ota_end(otaHandle) == ESP_OK) {
                String name = server.arg("name");
                name.trim();
                if (name == "") name = "Game " + String(otaSlot + 1);
                prefs.putString(nameKey(otaSlot).c_str(), name);
                refreshSlots();
                otaMsg = "OK - saved to slot " + String(otaSlot + 1) +
                        ". Press B on the console to go back to the menu.";
                status("Done: slot " + String(otaSlot + 1));
              } else {
                otaOk = false;
                otaMsg = "Not a valid ESP32-S3 app image";
                status("Invalid file");
              }
            }

        } else if (up.status == UPLOAD_FILE_ABORTED) {
            if (otaOk) esp_ota_abort(otaHandle);
            otaOk = false;
            otaMsg = "Upload aborted";
        }
}

void handleUploadDone() {
    server.send(otaOk ? 200 : 500, "text/plain", otaMsg);
}

void runUploadMode() {
    frame.deleteSprite();
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawCentreString("UPLOAD MODE", SCREEN_W / 2, 10, 4);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("1. Join WiFi:", 10, 55, 2);
    tft.drawString(String("   ") + AP_SSID + "  /  " + AP_PASS, 10, 75, 2);
    tft.drawString("2. Open in a browser:", 10, 105, 2);
    tft.drawString("   http://192.168.4.1", 10, 125, 2);
    tft.drawString("3. Pick a slot and upload the .bin", 10, 145, 2);
    tft.setTextcolor(TFT_LIGHTGREY_ TFT_BLACK);
    tft.drawString("B = exit", 10, 218, 2);

    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASS);

    server.on("/", HTTP_GET, handleRoot);
    server.on("/upload", HTTP_POST, handleUploadDone, handleUploadData);
    server.begin();

    while (true) {
        server.handleClient();
        if (eB.update(btnB())) esp_restart();    
        delay(2);
    }
}

void setup() {
    Serial.begin(115200);
    prefs.begin("games", false);

    if (!consoleBegin(tft, frame)) {
        Serial.println("Sprite allocation failed");
        while (true) delay(1000);
    }

    refreshSlots();
    if (!slot[0]) {
        frame.fillSprite(TFT_BLACK)
        frame.setTextColor(TFT_RED, TFT_BLACK);
        frame.drawCentreString("Partition table missing", SCREEN_W / 2, 90, 4);
        frame.setTextColor(TFT_WHITE, TFT_BLACK);
        frame.drawCentreString("Use partitions.csv, 8MB flash", SCREEN_W / 2, 130, 2);
        frame.pushSprite(0, 0);
        while (true) delay(1000);
    }
}

void loop() {
    if (eUp.update(btnUp()))     sel = (sel + MENU_ITEMS - 1) % MENU_ITEMS;
    if (eDown.update(btnDown())) sel = (sel + 1) % MENU_ITEMS;

    if (eA.update(btnA())) {
        if (sel == NUM_GAMES) runUploadMode();
        else if (slotUsed[sel]) launch(sel);
        else                    toast("Slot is empty");
    }

    static uint32_t lastFrame = 0;
    if (millis() - lastFrame >= 33) {
        lastFrame = millis();
        drawMenu();
    }
}