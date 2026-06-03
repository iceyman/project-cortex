#include <M5StackChan.h>
#include "kira_faces.h"  // eyes + mouth only, no overlay
#include <ArduinoOTA.h>
#include <Avatar.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SD.h>
#include "esp_camera.h"
#include "wifi_networks.h"
#include "dance_music.h"
#include <HTTPUpdate.h>

// ── Firmware version — bump this string before deploying to Cerebro ──────
#define FIRMWARE_VERSION "2026.05.31.2313"

using namespace m5avatar;

// ── Servo position constants (BSP angle units: 10 = 1°) ──────────────────────
#define PAN_CENTER      0       // looking straight ahead
#define PAN_MIN      -960       // ~96° left (safe)
#define PAN_MAX       960       // ~96° right
#define TILT_CENTER   450       // 45° neutral
#define TILT_HOME     450
#define TILT_MIN       50       // 5°  (safe min)
#define TILT_MAX      850       // 85° (safe max)
#define TILT_SAFE_MIN   0    // 0 = max droop for sleep
#define TILT_SAFE_MAX 850
#define SERVO_SPEED   500       // 0–1000

void playImmortals(int loops=2);  // forward declaration
void ledOn();   // forward declaration — defined after Avatar
void ledOff();  // forward declaration — defined after Avatar
void rgbAll(uint8_t r, uint8_t g, uint8_t b);  // forward declaration
static void _wheel(uint8_t pos, uint8_t &r, uint8_t &g, uint8_t &b);  // forward declaration
void checkFirmwareUpdate();  // forward declaration — defined before loop

#define MIC_RATE      16000

// ── Camera (GC0308 on CoreS3 SE) ─────────────────────────────────────────────
#define CAM_PIN_PWDN   -1
#define CAM_PIN_RESET  -1
#define CAM_PIN_XCLK   45
#define CAM_PIN_SIOD   12
#define CAM_PIN_SIOC   11
#define CAM_PIN_D7     16
#define CAM_PIN_D6     17
#define CAM_PIN_D5     18
#define CAM_PIN_D4     15
#define CAM_PIN_D3     14
#define CAM_PIN_D2     13
#define CAM_PIN_D1      4
#define CAM_PIN_D0      5
#define CAM_PIN_VSYNC  42
#define CAM_PIN_HREF   26
#define CAM_PIN_PCLK   41



// ── Camera face tracking ─────────────────────────────────────────────────────
static bool camOk = false;

bool initCamera() {
    camera_config_t cfg = {};
    cfg.ledc_channel  = LEDC_CHANNEL_0; cfg.ledc_timer = LEDC_TIMER_0;
    cfg.pin_d0=CAM_PIN_D0; cfg.pin_d1=CAM_PIN_D1; cfg.pin_d2=CAM_PIN_D2;
    cfg.pin_d3=CAM_PIN_D3; cfg.pin_d4=CAM_PIN_D4; cfg.pin_d5=CAM_PIN_D5;
    cfg.pin_d6=CAM_PIN_D6; cfg.pin_d7=CAM_PIN_D7;
    cfg.pin_xclk=CAM_PIN_XCLK; cfg.pin_pclk=CAM_PIN_PCLK;
    cfg.pin_vsync=CAM_PIN_VSYNC; cfg.pin_href=CAM_PIN_HREF;
    cfg.pin_sccb_sda=CAM_PIN_SIOD; cfg.pin_sccb_scl=CAM_PIN_SIOC;
    cfg.pin_pwdn=CAM_PIN_PWDN; cfg.pin_reset=CAM_PIN_RESET;
    cfg.xclk_freq_hz=20000000; cfg.pixel_format=PIXFORMAT_GRAYSCALE;
    cfg.frame_size=FRAMESIZE_QQVGA;  // 160×120 — fast enough for tracking
    cfg.fb_count=1; cfg.grab_mode=CAMERA_GRAB_LATEST;
    camOk = (esp_camera_init(&cfg) == ESP_OK);
    Serial.printf("[Cam] init=%s\n", camOk?"OK":"FAIL");
    return camOk;
}

// Returns normalised x offset of brightest region (-1=left, 0=centre, +1=right)
// Good enough for face following — faces are typically the brightest skin-tone region
float camFaceOffset() {
    if (!camOk) return 0.0f;
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) return 0.0f;
    int W=fb->width, H=fb->height;
    // Sum brightness in left/centre/right thirds, top half only (where faces are)
    long L=0, C=0, R=0;
    for (int y=0; y<H/2; y++) {
        for (int x=0; x<W; x++) {
            uint8_t p = fb->buf[y*W+x];
            if (x < W/3)       L += p;
            else if (x < 2*W/3) C += p;
            else                R += p;
        }
    }
    esp_camera_fb_return(fb);
    // Ignore if all regions are very dark (no face present)
    if (L+C+R < (long)(W/2)*(H/2)*30) return 0.0f;
    // Return weighted offset
    float total = L + C + R;
    return (R - L) / total;  // -1..+1
}

uint32_t lastCamFollow = 0;


// ════════════════════════════════════════════════════════════
//  Config
// ════════════════════════════════════════════════════════════
char cfgRobotId[32]="robot1"; char cfgName[32]="Kira";
char cfgPiIP[64]="192.168.50.111"; int cfgPiPort=5005;
char cfgMode[16]="adult"; char cfgLedCol[16]="purple";
bool cfgHasKid=false; char cfgKidName[32]="buddy";
int cfgVadThresh=1100; int cfgSilenceMs=1400; int cfgMaxSecs=8;

// ════════════════════════════════════════════════════════════
//  State
// ════════════════════════════════════════════════════════════
enum RobotState { AMBIENT, THINKING, SPEAKING };
RobotState state=AMBIENT;
enum VadState { VAD_WAIT, VAD_PRIMED, VAD_RECORDING, VAD_DONE };
VadState vad=VAD_WAIT;

int16_t* micBuf=nullptr; size_t micBufMax=0, micBufPos=0, micBufFinal=0;
uint32_t vadPrimedAt=0, silenceAt=0;
uint32_t lastIdleMove=0, lastIdleExpr=0, lastIdleSpeak=0;
uint32_t lastMicroMove=0;    // tiny constant drift
uint32_t lastBigLook=0;      // deliberate looks around
int8_t   idleScanDir=1;      // for slow scan behaviour
uint8_t  idleBehaviour=0;    // current movement pattern
uint32_t lastTouchMs=0, lastWifiCheck=0, lastTap=0, lastPickup=0;
uint32_t lastDanceMs=0;  // prevents dance party spam
bool     serverSleeping  = false;  // server says sleep hours
bool     wasSleeping     = false;  // detect transitions
uint32_t lastSleepCheck  = 0;

// ════════════════════════════════════════════════════════════
//  Servo
// ════════════════════════════════════════════════════════════
struct SPos { float pan,tilt; };
SPos sTgt={PAN_CENTER,TILT_CENTER}, sCur={PAN_CENTER,TILT_CENTER};
TaskHandle_t servoTaskH=nullptr;

int clampS(float v,int lo,int hi){return(int)max((float)lo,min((float)hi,v));}

void servoFn(void*){
  while(true){
    float dp=sTgt.pan-sCur.pan, dt=sTgt.tilt-sCur.tilt;
    if(fabsf(dp)>2){
      sCur.pan+=(dp>0?1:-1)*min(12.0f,fabsf(dp));
      int16_t px = (int16_t)constrain(sCur.pan, -1280, 1280);
      M5StackChan.Motion.moveX(px, SERVO_SPEED);
    }
    if(fabsf(dt)>2){
      sCur.tilt+=(dt>0?1:-1)*min(8.0f,fabsf(dt));
      int16_t ty = (int16_t)constrain(sCur.tilt, TILT_SAFE_MIN, TILT_SAFE_MAX);
      M5StackChan.Motion.moveY(ty, SERVO_SPEED);
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

void moveTo(float p,float t){sTgt.pan=clampS(p,PAN_MIN,PAN_MAX);sTgt.tilt=clampS(t,TILT_MIN,TILT_MAX);}

void camFollowFace() {
    if (!camOk || state != AMBIENT || millis()-lastCamFollow < 800) return;
    lastCamFollow = millis();
    float offset = camFaceOffset();
    if (fabsf(offset) < 0.15f) return;  // dead zone — already centred
    // Nudge pan servo toward the face
    float nudge = offset * 120.0f;  // scale to angle units
    sTgt.pan = constrain(sTgt.pan + nudge, PAN_MIN, PAN_MAX);
    Serial.printf("[Cam] face offset=%.2f nudge=%.0f pan=%.0f\n", offset, nudge, sTgt.pan);
}
void moveCenter()   {moveTo(PAN_CENTER,TILT_CENTER);}
void moveListen()   {moveTo(PAN_CENTER,TILT_MIN+15);}
void moveThink()    {moveTo(PAN_CENTER+80,TILT_MIN+10);}
void moveHappy()    {moveTo(PAN_CENTER,TILT_CENTER+25);}
void moveSurprised(){moveTo(PAN_CENTER,TILT_MIN);}
void moveSad()      {moveTo(PAN_CENTER,TILT_MAX);}
void doDance(int n){
    // Suspend servo task — stops it fighting our direct BSP calls
    if(servoTaskH) vTaskSuspend(servoTaskH);
    delay(25);  // let any in-flight servo call finish

    const int NOTES[] = {523,587,659,698,784,880,988,1047,880,784,659,587};
    for(int i=0;i<n;i++){
        int16_t pan  = random(-900, 900);
        int16_t tilt = constrain((int)random(150, 750), TILT_SAFE_MIN, TILT_SAFE_MAX);
        M5StackChan.Motion.move(pan, tilt, 1000);  // max speed, servo task is suspended
        sTgt.pan=pan; sCur.pan=pan; sTgt.tilt=tilt; sCur.tilt=tilt;
        { uint8_t r,g,b; _wheel(((uint8_t)(i*21))&0xFF,r,g,b);
          for(int l=0;l<12;l++){_wheel(((uint8_t)(i*21+l*20))&0xFF,r,g,b);M5StackChan.setRgbColor(l,r,g,b);}
          M5StackChan.refreshRgb(); }
        M5.Speaker.tone(NOTES[i % 12], 180);
        delay(220);
    }
    ledOff(); M5.Speaker.stop();
    M5StackChan.Motion.goHome();
    sTgt={PAN_CENTER,TILT_CENTER}; sCur={PAN_CENTER,TILT_CENTER};

    // Resume servo task for normal operation
    if(servoTaskH) vTaskResume(servoTaskH);
}
void doRoar(){
    // Must begin speaker — playBuf() releases it before action triggers
    if(M5.Mic.isRunning()) M5.Mic.end();
    delay(30);
    M5.Speaker.begin();
    M5.Speaker.setVolume(220);
    int freqs[] = {520, 480, 420, 360, 300, 260, 220, 190};
    int durs[]  = {80,  80,  100, 110, 120, 130, 150, 200};
    for(int i=0;i<8;i++){
        M5.Speaker.tone(freqs[i], durs[i]);
        delay(durs[i]+10);
    }
    M5.Speaker.stop();
    M5.Speaker.end();
    delay(30);
    M5.Mic.begin();
    M5.Speaker.setVolume(180);

    // Head shake
    for(int i=0;i<6;i++){
        sTgt.pan = (i%2==0) ? -500 : 500;
        delay(160);
    }
    sTgt.pan=PAN_CENTER; sTgt.tilt=TILT_CENTER;
    delay(200);
}

// ════════════════════════════════════════════════════════════
//  Avatar — ALL display goes through Avatar only
//  NEVER call M5.Display directly while Avatar task is running
//  (multi-core SPI race condition on CoreS3 SE)
// ════════════════════════════════════════════════════════════
Avatar avatar;

// ═══════════════════════════════════════════════════════════// ── 12× WS2812C RGB LEDs via StackChan BSP ──────────────
//   0-5  = left side (top→bottom)
//   6-11 = right side (top→bottom)

static uint8_t _ledR=0, _ledG=0, _ledB=0;  // current colour

void rgbAll(uint8_t r, uint8_t g, uint8_t b) {
    _ledR=r; _ledG=g; _ledB=b;
    for (int i=0;i<12;i++) M5StackChan.setRgbColor(i,r,g,b);
    M5StackChan.refreshRgb();
}

// Eye glow (0=left eye, 6=right eye) + body ambient
void rgbEyes(uint8_t er, uint8_t eg, uint8_t eb,   // eye colour
             uint8_t br, uint8_t bg, uint8_t bb) {  // body colour
    M5StackChan.setRgbColor(0,  er, eg, eb);  // left eye
    M5StackChan.setRgbColor(6,  er, eg, eb);  // right eye
    for (int i=1;i<6;i++)  M5StackChan.setRgbColor(i,  br,bg,bb); // left body
    for (int i=7;i<12;i++) M5StackChan.setRgbColor(i,  br,bg,bb); // right body
    M5StackChan.refreshRgb();
}

// Wheel: 0-255 → RGB rainbow
static void _wheel(uint8_t pos, uint8_t &r, uint8_t &g, uint8_t &b) {
    pos = 255 - pos;
    if (pos < 85) { r=255-pos*3; g=0; b=pos*3; }
    else if (pos < 170) { pos-=85; r=0; g=pos*3; b=255-pos*3; }
    else { pos-=170; r=pos*3; g=255-pos*3; b=0; }
}

// Expression-aware colour (call once to get target r,g,b)
void ledColour(uint8_t &r, uint8_t &g, uint8_t &b) {
    bool isRumi = (strcmp(cfgName,"Rumi")==0);
    Expression exp = avatar.getExpression();
    if (isRumi) {
        switch(exp) {
            case Expression::Happy:  r=255;g=60; b=160; return;
            case Expression::Sad:    r=140;g=50; b=200; return;
            case Expression::Sleepy: r=170;g=80; b=220; return;
            case Expression::Doubt:  r=255;g=120;b=50;  return;
            default:                 r=255;g=100;b=185; return; // pink
        }
    } else {
        switch(exp) {
            case Expression::Happy:  r=255;g=200;b=0;   return; // gold
            case Expression::Sad:    r=40; g=80; b=255; return; // blue
            case Expression::Sleepy: r=100;g=50; b=200; return; // purple
            case Expression::Doubt:  r=255;g=60; b=60;  return; // red
            default:                 r=0;  g=180;b=255; return; // cyan
        }
    }
}

void ledOff()  { rgbAll(0,0,0); }
void ledOn()   {
    uint8_t r,g,b; ledColour(r,g,b);
    bool isRumi = (strcmp(cfgName,"Rumi")==0);
    uint8_t er=isRumi?0:160, eg=isRumi?200:100, eb=isRumi?170:255;
    rgbEyes(er,eg,eb, r,g,b);
}

// Breathing update — call from loop()
uint32_t lastLedMs=0;
void updateRGB() {
    if (millis()-lastLedMs < 60) return;
    lastLedMs = millis();
    if (state==SPEAKING || state==THINKING) return;
    float pulse = (avatar.getBreath()+1.0f)*0.5f;  // 0→1
    uint8_t dim  = (uint8_t)(40 + pulse*100);       // body 40–140
    uint8_t eyeD = (uint8_t)(80 + pulse*120);       // eyes brighter 80–200
    uint8_t r,g,b; ledColour(r,g,b);
    // Eyes full iris colour, body dim ambient
    bool isRumi = (strcmp(cfgName,"Rumi")==0);
    uint8_t er = isRumi ? 0   : 160;  // Kira: purple iris
    uint8_t eg = isRumi ? 158 : 80;   // Rumi: teal iris
    uint8_t eb = isRumi ? 117 : 255;
    // Scale eye brightness by pulse
    er=(uint8_t)(er*eyeD/255); eg=(uint8_t)(eg*eyeD/255); eb=(uint8_t)(eb*eyeD/255);
    rgbEyes(er, eg, eb,
            (uint8_t)(r*dim/255), (uint8_t)(g*dim/255), (uint8_t)(b*dim/255));
}
void ledFlash(int times, int onMs=80, int offMs=80) {
    for (int i=0; i<times; i++) { ledOn(); delay(onMs); ledOff(); delay(offMs); }
}

void ledDanceParty() {
    // Rapid strobe during dance
    for (int i=0; i<48; i++) { ledOn(); delay(40); ledOff(); delay(40); }
}

void ledDinoRoar() {
    // Three slow red-ish pulses
    for (int i=0; i<3; i++) { ledOn(); delay(300); ledOff(); delay(150); }
}

void ledThinking() {
    // Slow pulse while processing
    ledOn(); delay(500); ledOff();
}

void ledSpeaking() {
    ledOn(); // stays on while speaking, off when done
}


TaskHandle_t mouthTaskH=nullptr;
volatile bool mouthOn=false;
extern float gMouthRatio;  // defined in kira_faces.h

void mouthFn(void*){
  float t=0;
  while(mouthOn){t+=0.15f;float target=(sinf(t)+1.0f)*0.28f;gMouthRatio=gMouthRatio*0.6f+target*0.4f;avatar.setMouthOpenRatio(gMouthRatio);sTgt.tilt=TILT_CENTER+sinf(t*0.4f)*8;vTaskDelay(pdMS_TO_TICKS(55));}
  gMouthRatio=0.0f; avatar.setMouthOpenRatio(0); mouthTaskH=nullptr; vTaskDelete(nullptr);
}
void mouthStart(){if(mouthTaskH)return;mouthOn=true;xTaskCreatePinnedToCore(mouthFn,"mouth",2048,nullptr,1,&mouthTaskH,1);}
void mouthStop(){mouthOn=false;}

// ════════════════════════════════════════════════════════════
//  Touch
// ════════════════════════════════════════════════════════════
void handleTouch(){
  // ── Screen touch ─────────────────────────────────────────────────────
  auto t=M5.Touch.getDetail();
  if(!t.wasPressed())return;
  if(millis()-lastTouchMs<600)return;
  lastTouchMs=millis();
  if(t.y>210&&cfgHasKid){
    bool isKid=strcmp(cfgMode,"kid")==0;
    strcpy(cfgMode,isKid?"adult":"kid");
    avatar.setExpression(isKid?Expression::Neutral:Expression::Happy);
    if(!isKid)moveSurprised();else moveCenter();
    avatar.setSpeechText(isKid?"Adult mode":"Kid mode!");
    delay(1200); avatar.setSpeechText(""); moveCenter(); avatar.setExpression(Expression::Neutral);
    return;
  }
  if(t.x<22&&t.y>40&&t.y<190&&state==AMBIENT){
    HTTPClient h; h.begin("http://"+String(cfgPiIP)+":"+String(cfgPiPort)+"/reset");
    h.addHeader("X-Session",cfgRobotId); h.POST(""); h.end();
    avatar.setExpression(Expression::Happy); avatar.setSpeechText("Memory cleared!"); delay(1200);
    avatar.setSpeechText(""); avatar.setExpression(Expression::Neutral);
  }
}

// ════════════════════════════════════════════════════════════
//  SD card
// ════════════════════════════════════════════════════════════
void loadConfig(){
  // Re-init SPI for SD card after M5StackChan.begin() reconfigures the bus
  SPI.begin(36, 35, 37, 4);  // CoreS3 SD: CLK=36, MISO=35, MOSI=37, CS=4
  delay(100);
  if(!SD.begin(4, SPI, 4000000)||!SD.exists("/robot_config.json")){
    Serial.println("[SD] No config — using defaults");
    return;
  }
  File f=SD.open("/robot_config.json"); JsonDocument doc;
  if(deserializeJson(doc,f)!=DeserializationError::Ok){f.close();return;} f.close();
  auto str=[&](const char* k,char* d,size_t s){if(doc[k].is<const char*>())strlcpy(d,doc[k],s);};
  str("robot_id",cfgRobotId,sizeof(cfgRobotId)); str("robot_name",cfgName,sizeof(cfgName));
  str("default_mode",cfgMode,sizeof(cfgMode)); str("led_color",cfgLedCol,sizeof(cfgLedCol));
  str("kid_name",cfgKidName,sizeof(cfgKidName));
  if(doc["has_kid_mode"].is<bool>()) cfgHasKid=doc["has_kid_mode"];
  if(doc["vad_threshold"].is<int>()) cfgVadThresh=doc["vad_threshold"];
  if(doc["vad_silence_ms"].is<int>()) cfgSilenceMs=doc["vad_silence_ms"];
  if(doc["vad_max_secs"].is<int>()) cfgMaxSecs=doc["vad_max_secs"];
  if(doc["speaker_volume"].is<int>()) { int vol=doc["speaker_volume"]; M5.Speaker.setVolume(constrain(vol,0,255)); }
  loadNetworks(doc);
  Serial.printf("[Config] %s mode=%s vad=%d\n",cfgName,cfgMode,cfgVadThresh);
}

// ════════════════════════════════════════════════════════════
//  VAD
// ════════════════════════════════════════════════════════════
static int16_t pollChunk[256];
float calcRMS(int16_t* b,int n){float s=0;for(int i=0;i<n;i++)s+=(float)b[i]*b[i];return sqrtf(s/n);}

bool pollVAD(){
  if(!M5.Mic.isRunning()) M5.Mic.begin();
  M5.Mic.record(pollChunk,256);
  float rms=calcRMS(pollChunk,256);
  uint32_t now=millis();
  switch(vad){
    case VAD_WAIT:
      if(rms>cfgVadThresh){vad=VAD_PRIMED;vadPrimedAt=now;}
      break;
    case VAD_PRIMED:
      if(rms<cfgVadThresh*0.7f){vad=VAD_WAIT;break;}
      if(now-vadPrimedAt>280){vad=VAD_RECORDING;micBufPos=0;silenceAt=0;memcpy(micBuf,pollChunk,256*sizeof(int16_t));micBufPos=256;}
      break;
    case VAD_RECORDING:
      if(micBufPos+256<micBufMax){memcpy(micBuf+micBufPos,pollChunk,256*sizeof(int16_t));micBufPos+=256;}
      if(rms<cfgVadThresh*0.6f){if(!silenceAt)silenceAt=now;else if(now-silenceAt>(uint32_t)cfgSilenceMs){micBufFinal=micBufPos;vad=VAD_DONE;return true;}}
      else silenceAt=0;
      if(micBufPos>=MIC_RATE*(size_t)cfgMaxSecs){micBufFinal=micBufPos;vad=VAD_DONE;return true;}
      break;
    case VAD_DONE: break;
  }
  return false;
}

// ════════════════════════════════════════════════════════════
//  Audio
// ════════════════════════════════════════════════════════════
void playBuf(const uint8_t* buf,size_t len){
  if(len<2) return;
  M5.Mic.end();          // release I2S from mic
  delay(50);
  M5.Speaker.begin();    // take I2S for speaker
  M5.Speaker.setVolume(255);
  M5.Speaker.playRaw((const int16_t*)buf, len/2, MIC_RATE, false, 1, 0);
  uint32_t start = millis();
  while(M5.Speaker.isPlaying() && millis()-start < 30000){
    M5.update(); delay(10);
  }
  M5.Speaker.end();      // release I2S from speaker
  delay(50);
  M5.Mic.begin();        // give I2S back to mic
  delay(50);
}

// ════════════════════════════════════════════════════════════
//  Actions
// ════════════════════════════════════════════════════════════
void triggerAction(const String& action){
  if(action=="dance_party"){
    if(millis()-lastDanceMs < 60000){
      Serial.println("[Action] Dance party on cooldown — ignoring");
      return;
    }
    lastDanceMs = millis();
    // Clear "Thinking..." FIRST so robot doesn't freeze on it
    avatar.setSpeechText("");
    state = SPEAKING;
    avatar.setExpression(Expression::Happy); moveSurprised();
    M5.Mic.end(); delay(100);
    M5.Speaker.begin();
    M5.Speaker.setVolume(220);
    delay(80);
    doDance(24);  // reduced from 32 — less chance of watchdog
    M5.Speaker.stop();
    M5.Speaker.end();
    delay(2000);
    M5.Mic.begin();
    moveCenter(); avatar.setExpression(Expression::Happy);
  } else if(action=="play_music"){
    avatar.setExpression(Expression::Happy);
    // No text bubble
    M5.Mic.end(); delay(50); M5.Speaker.begin(); delay(50);
    playImmortals(2);
    M5.Speaker.end(); delay(1500); M5.Mic.begin();
    avatar.setSpeechText(""); avatar.setExpression(Expression::Neutral);
  } else if(action=="roar"){
    avatar.setExpression(Expression::Doubt);
    xTaskCreatePinnedToCore([](void*){
        ledDinoRoar(); vTaskDelete(nullptr);
    },"ledroar",4096,nullptr,1,nullptr,1);
    doRoar(); avatar.setExpression(Expression::Neutral);
  }
}

// ════════════════════════════════════════════════════════════
//  Immortals — Fall Out Boy chiptune (Big Hero 6)
//  Approximates the iconic main riff, ~168 BPM
// ════════════════════════════════════════════════════════════
struct NoteMs { uint16_t freq; uint16_t ms; };

// Immortals main hook — 168BPM, E minor feel
// "We are the Immortals" chorus approximation
const NoteMs IMMORTALS[] = {
    // Intro power riff
    {494,90},{0,30}, {494,90},{0,30}, {587,90},{0,30}, {659,180},{0,60},
    {587,90},{0,30}, {494,90},{0,30}, {392,90},{0,30}, {0,120},
    // Hook phrase 1
    {494,90},{0,30}, {587,90},{0,30}, {659,90},{0,30}, {784,180},{0,60},
    {659,90},{0,30}, {587,90},{0,30}, {494,360},{0,120},
    // Hook phrase 2
    {392,90},{0,30}, {440,90},{0,30}, {494,90},{0,30}, {587,90},{0,30},
    {659,360},{0,120}, {0,120},
    // Big finish
    {784,90},{0,30}, {659,90},{0,30}, {587,90},{0,30}, {494,90},{0,30},
    {392,360},{0,180},
};
const int IMMORTALS_COUNT = sizeof(IMMORTALS)/sizeof(NoteMs);

void playImmortals(int loops){
    if(servoTaskH) vTaskSuspend(servoTaskH);
    M5.Speaker.setVolume(200);
    for(int loop=0; loop<loops; loop++){
        for(int i=0; i<IMMORTALS_COUNT; i++){
            // Gentle sway while music plays
            if(IMMORTALS[i].freq > 0){
                M5StackChan.Motion.moveX((int16_t)random(-300,300), 300);
                { uint8_t r,g,b; _wheel((i*42)&0xFF,r,g,b); rgbAll(r,g,b); }
                M5.Speaker.tone(IMMORTALS[i].freq, IMMORTALS[i].ms);
            }
            delay(IMMORTALS[i].ms + 10);
        }
    }
    ledOff(); M5.Speaker.stop();
    M5StackChan.Motion.goHome();
    sTgt={PAN_CENTER,TILT_CENTER}; sCur={PAN_CENTER,TILT_CENTER};
    if(servoTaskH) vTaskResume(servoTaskH);
}

// ════════════════════════════════════════════════════════════
//  Sound effects — generated tones played after speech
// ════════════════════════════════════════════════════════════
void playSFX(const String& sfx) {
    if (sfx.isEmpty() || sfx == "none") return;
    M5.Speaker.begin();
    M5.Speaker.setVolume(200);

    // Rocket ship — ascending sweep like an engine building
    if (sfx.indexOf("sfx_rocket") >= 0) {
        for (int f=150; f<2200; f+=40) { M5.Speaker.tone(f,18); delay(18); }
        // Whoosh tail
        for (int f=2200; f>400; f-=80) { M5.Speaker.tone(f,12); delay(12); }
        delay(100);
    }
    // Musical notes — cheerful ascending scale
    if (sfx.indexOf("sfx_notes") >= 0) {
        int scale[] = {262,294,330,349,392,440,494,523};
        for (int n : scale) { M5.Speaker.tone(n,120); delay(130); }
        delay(80);
    }
    // Laser zap — classic descending pew
    if (sfx.indexOf("sfx_laser") >= 0) {
        for (int i=0; i<2; i++) {
            for (int f=1800; f>150; f-=60) { M5.Speaker.tone(f,15); delay(15); }
            delay(120);
        }
    }
    // Magic / twinkle — high sparkly arpeggio
    if (sfx.indexOf("sfx_magic") >= 0) {
        int magic[] = {523,659,784,1047,1319,1047,784,659,523};
        for (int n : magic) { M5.Speaker.tone(n,80); delay(85); }
        delay(80);
    }
    // Bounce / boing — descending glide
    if (sfx.indexOf("sfx_bounce") >= 0) {
        for (int f=800; f>100; f-=20) { M5.Speaker.tone(f,15); delay(15); }
        delay(80);
    }
    // Win / hooray — triumphant fanfare
    if (sfx.indexOf("sfx_win") >= 0) {
        int win[] = {523,523,523,659,523,659,784};
        int dur[] = {120,120,120,480,120,120,600};
        for (int i=0;i<7;i++) { M5.Speaker.tone(win[i],dur[i]); delay(dur[i]+20); }
        delay(100);
    }
    // Alarm / uh oh — urgent beeps
    if (sfx.indexOf("sfx_alarm") >= 0) {
        for (int i=0; i<3; i++) {
            M5.Speaker.tone(880,180); delay(200);
            M5.Speaker.tone(660,180); delay(200);
        }
    }

    M5.Speaker.stop();
    M5.Speaker.end();
    delay(100);
    M5.Mic.begin();
}

// ════════════════════════════════════════════════════════════
//  Chat with Cerebro
// ════════════════════════════════════════════════════════════
void sendAndRespond(){
  stopDanceMusic();
  state=THINKING;
  avatar.setExpression(Expression::Neutral); moveThink();
  avatar.setSpeechText("");
  // Thinking — purple pulse on all 12 LEDs
  rgbAll(120, 0, 255);

  HTTPClient http;
  http.begin("http://"+String(cfgPiIP)+":"+String(cfgPiPort)+"/chat");
  // MUST register headers before POST or http.header() always returns ""
  const char* hdrs[] = {"X-Reply-Text","X-Action","X-Robot-Name","X-SFX"};
  http.collectHeaders(hdrs, 4);
  http.addHeader("Content-Type","application/octet-stream");
  http.addHeader("X-Session",cfgRobotId); http.addHeader("X-Robot-ID",cfgRobotId);
  http.addHeader("X-Mode",cfgMode); http.addHeader("X-Kid-Name",cfgKidName);
  http.addHeader("X-Sample-Rate",String(MIC_RATE));
  http.setTimeout(60000);
  int code=http.POST((uint8_t*)micBuf,micBufFinal*sizeof(int16_t));

  if(code==204){http.end();goto done;}
  if(code!=200){
    avatar.setExpression(Expression::Sad); moveSad();
    avatar.setSpeechText("Cerebro offline!");
    http.end(); delay(1800); goto done;
  }
  {
    String replyText=http.header("X-Reply-Text");
    String action=http.header("X-Action");
    if(action.isEmpty())action="none";
    size_t cLen=http.getSize();
    uint8_t* buf=(uint8_t*)ps_malloc(cLen+16);
    if(!buf){http.end();goto done;}
    {
      WiFiClient* s=http.getStreamPtr(); size_t got=0; uint8_t tmp[512];
      while(got<cLen&&s->connected()){size_t av=s->available();if(av){size_t r=s->readBytes(tmp,min(av,sizeof(tmp)));memcpy(buf+got,tmp,r);got+=r;}else delay(2);}
    }
    http.end();
    state=SPEAKING;
    avatar.setExpression(Expression::Happy); moveHappy();
    // No speech bubble for replies — face animation only
    mouthStart(); playBuf(buf,cLen); mouthStop();
    free(buf);
    String sfx=http.header("X-SFX");
    if(sfx.length()>0 && sfx!="none") playSFX(sfx);
    if(action!="none") triggerAction(action);
    delay(500);
  }
done:
  ledOff();
  avatar.setExpression(Expression::Neutral); moveCenter();
  avatar.setSpeechText("");
  delay(1500);  // cooldown — prevents mic picking up speaker echo/residual noise
  if(M5.Mic.isRunning()) M5.Mic.end();
  delay(200);
  M5.Mic.begin();
  delay(300);   // let mic settle before listening again
  vad=VAD_WAIT; state=AMBIENT;
}

// ════════════════════════════════════════════════════════════
//  Idle
// ════════════════════════════════════════════════════════════
const Expression EXPRS[]={Expression::Neutral,Expression::Happy,Expression::Neutral,Expression::Sleepy,Expression::Doubt,Expression::Neutral};

void idleAnimate(){
  uint32_t now=millis();

  // ── Micro drift — tiny human-like fidgets every 0.5-1.2s ─────────────
  if(now>lastMicroMove){
    lastMicroMove = now + 1200 + random(1800);  // slower — every 1.2-3s
    float np = sTgt.pan  + random(-6, 6);       // smaller drift
    float nt = sTgt.tilt + random(-4, 4);
    moveTo(np, nt);
  }

  // ── Behaviour patterns — deliberate movements every 2-5s ─────────────
  if(now>lastIdleMove){
    uint8_t b = random(0, 10);  // weighted random behaviours
    switch(b){
      case 0: case 1:  // Casual glance left (20%)
        lastIdleMove = now + 1800 + random(1200);
        moveTo(PAN_CENTER - 120 - random(60), TILT_CENTER + random(-8,12));
        break;
      case 2: case 3:  // Casual glance right (20%)
        lastIdleMove = now + 1800 + random(1200);
        moveTo(PAN_CENTER + 120 + random(60), TILT_CENTER + random(-8,12));
        break;
      case 4:          // Thinking — look down-left (10%)
        lastIdleMove = now + 2500 + random(1500);
        moveTo(PAN_CENTER - 130, TILT_CENTER - 40);
        break;
      case 5:          // Daydream — drift up slightly (10%)
        lastIdleMove = now + 3000 + random(2000);
        moveTo(PAN_CENTER + random(-40,40), TILT_CENTER + 50 + random(20));
        break;
      case 6:          // Alert — snap to attention, face forward (10%)
        lastIdleMove = now + 1000 + random(600);
        moveTo(PAN_CENTER + random(-20,20), TILT_CENTER + 20);
        break;
      case 7:          // Long look right then drift back (10%)
        lastIdleMove = now + 2200 + random(1000);
        moveTo(PAN_CENTER + 220 + random(40), TILT_CENTER + random(-10,10));
        break;
      case 8:          // Slow scan — advance scan position (10%)
        lastIdleMove = now + 600 + random(400);
        idleScanDir = (sTgt.pan > PAN_CENTER + 200) ? -1 : (sTgt.pan < PAN_CENTER - 200) ? 1 : idleScanDir;
        moveTo(sTgt.pan + idleScanDir * (60 + random(40)), TILT_CENTER + random(-10,10));
        break;
      default:         // Return near centre — rest pose (10%)
        lastIdleMove = now + 3500 + random(2500);
        moveTo(PAN_CENTER + random(-30,30), TILT_CENTER + random(-10,20));
        break;
    }
  }

  if(now>lastIdleExpr){lastIdleExpr=now+8000+random(7000);avatar.setExpression(EXPRS[random(0,6)]);}
  if(now>lastIdleSpeak&&wifiConnected){
    lastIdleSpeak=now+300000+random(240000);
    HTTPClient h; h.begin("http://"+String(cfgPiIP)+":"+String(cfgPiPort)+"/idle?robot_id="+String(cfgRobotId));
    h.setTimeout(8000);
    const char* ihdrs[] = {"X-Reply-Text","X-Action"};
    h.collectHeaders(ihdrs, 2);
    if(h.GET()==200){
      String txt=h.header("X-Reply-Text"); size_t cLen=h.getSize();
      uint8_t* buf=(uint8_t*)ps_malloc(cLen+16);
      if(buf){
        WiFiClient* s=h.getStreamPtr(); size_t got=0; uint8_t tmp[512];
        while(got<cLen&&s->connected()){size_t av=s->available();if(av){size_t r=s->readBytes(tmp,min(av,sizeof(tmp)));memcpy(buf+got,tmp,r);got+=r;}else delay(2);}
        avatar.setExpression(Expression::Happy);
        mouthStart(); playBuf(buf,cLen); mouthStop();
        free(buf); delay(600);
        avatar.setExpression(Expression::Neutral);
      }
    }
    h.end(); M5.Mic.begin(); vad=VAD_WAIT;
  }
}

// ════════════════════════════════════════════════════════════
//  IMU
// ════════════════════════════════════════════════════════════
void checkIMU(){
  auto d=M5.Imu.getImuData();
  float ax=d.accel.x,ay=d.accel.y,az=d.accel.z,mag=sqrtf(ax*ax+ay*ay+az*az);
  uint32_t now=millis();
  if(fabsf(mag-1.0f)>3.5f&&now-lastTap>3000){lastTap=now;avatar.setExpression(Expression::Doubt);moveSurprised();delay(350);avatar.setExpression(Expression::Neutral);moveCenter();}
  if(mag<0.1f&&now-lastPickup>8000){lastPickup=now;avatar.setExpression(Expression::Doubt);avatar.setSpeechText("");moveSurprised();delay(1000);avatar.setSpeechText("");moveCenter();avatar.setExpression(Expression::Neutral);}
}

// ════════════════════════════════════════════════════════════
//  Setup — M5.begin() auto-detects CoreS3 SE (board 27)
//  Latest M5Unified required for proper CoreS3 SE display support
// ════════════════════════════════════════════════════════════
void setup(){
  Serial.begin(115200);
  delay(300);

  // ONE call handles everything: M5.begin, PY32 IO expander, VM_EN, UART1 on GPIO6/7
  // Requires: build_unflags=-std=gnu++11, build_flags=-std=gnu++17 -fpermissive -DUART_SCLK_DEFAULT=UART_SCLK_XTAL
  M5StackChan.begin();

  // LED is single-colour via PY32 I2C — already handled by ledOn()/ledOff()

  loadConfig();


  // BSP already called above — servos ready

  micBufMax=MIC_RATE*(size_t)(cfgMaxSecs+2);
  micBuf=(int16_t*)ps_malloc(micBufMax*sizeof(int16_t));
  if(!micBuf){while(true)delay(1000);}

  // Avatar starts BEFORE WiFi — pins to Core 0 to avoid SPI race with WiFi on Core 0
  avatar.init(8);  // 8-bit — safe PSRAM usage, no crash during dance party

  delay(300);

  // ── Colour palette — skin tone background + iris colour per robot
  // COLOR_BACKGROUND = skin fills the entire sprite → face is skin coloured naturally
  ColorPalette palette;
  bool isRumi = (strcmp(cfgLedCol,"teal")==0 || strcmp(cfgName,"Rumi")==0);
  uint16_t irisColor;
  if (isRumi) {
    irisColor = M5.Lcd.color565(180, 60, 200);         // Mirko violet
    palette.set(COLOR_PRIMARY,   M5.Lcd.color565(80,  45,  20));
    palette.set(COLOR_SECONDARY, M5.Lcd.color565(40,  20,   8));
  } else {
    irisColor = M5.Lcd.color565(120, 70, 230);         // bold purple
    palette.set(COLOR_PRIMARY,   M5.Lcd.color565(127, 119, 221));
    palette.set(COLOR_SECONDARY, M5.Lcd.color565(60,   50, 140));
  }
  // Background colour — Kira warm dark, Rumi pink
  if (isRumi) {
    palette.set(COLOR_BACKGROUND, M5.Lcd.color565(255, 160, 200));  // vibrant Mirko pink
  } else {
    palette.set(COLOR_BACKGROUND, M5.Lcd.color565(235, 225, 250));  // soft lavender bg
  }
  avatar.setColorPalette(palette);

  // ── Bold graphic eyes + mouth (Project Cortex style)
  avatar.getFace()->setRightEye(new BigEye(false, irisColor));
  avatar.getFace()->setLeftEye(new BigEye(true,  irisColor));
  avatar.getFace()->setMouth(new BoldMouth());

  // Face plate border + hair drawn first, details on top
  avatar.addDrawable(new FacePlate(
    isRumi ? M5.Lcd.color565(220, 60, 120) : M5.Lcd.color565(140, 90, 255),   // hot pink border : purple border
    isRumi ? M5.Lcd.color565(255, 160, 200) : M5.Lcd.color565(235, 225, 250),  // Mirko pink : lavender bg
    isRumi
  ));
  avatar.addDrawable(new CharDetails(isRumi));

  avatar.setExpression(Expression::Happy);
  moveSurprised();
  avatar.setSpeechText("Starting...");
  delay(1000);

  avatar.setSpeechText("Connecting...");
  connectMultiWiFi(cfgPiIP,&cfgPiPort,nullptr);

  if(wifiConnected){
    avatar.setSpeechText("Online!");
    // OTA setup
    ArduinoOTA.setHostname(cfgRobotId);
    ArduinoOTA.setPassword("YOUR_OTA_PASSWORD");
    ArduinoOTA.onStart([](){
      Serial.println("[OTA] Starting update...");
      avatar.setSpeechText("Updating!");
    });
    ArduinoOTA.onEnd([](){
      Serial.println("[OTA] Done!");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total){
      Serial.printf("[OTA] %u%%\n", progress*100/total);
    });
    ArduinoOTA.onError([](ota_error_t error){
      Serial.printf("[OTA] Error %u\n", error);
    });
    ArduinoOTA.begin();
    Serial.printf("[OTA] Ready — hostname: %s\n", cfgRobotId);
  } else {
    avatar.setExpression(Expression::Sad); moveSad();
    avatar.setSpeechText("No WiFi!");
  }
  delay(1200);
  avatar.setSpeechText("");
  // Camera disabled — DMA conflict with BSP on CoreS3 SE
  // initCamera();

  avatar.setExpression(Expression::Happy); moveHappy();

  // Spoken greeting — fetch TTS from Cerebro and play it out loud
  if(wifiConnected){
    String gurl = "http://"+String(cfgPiIP)+":"+String(cfgPiPort)+
                  "/greet?robot_id="+String(cfgRobotId)+
                  "&mode="+String(cfgMode)+
                  "&kid_name="+String(cfgKidName);
    HTTPClient hg; hg.begin(gurl); hg.setTimeout(8000);
    if(hg.GET()==200){
      size_t gLen=hg.getSize();
      uint8_t* gBuf=(uint8_t*)ps_malloc(gLen+16);
      if(gBuf){
        WiFiClient* gs=hg.getStreamPtr(); size_t got=0; uint8_t tmp[512];
        while(got<gLen&&gs->connected()){
          size_t av=gs->available();
          if(av){size_t r=gs->readBytes(tmp,min(av,sizeof(tmp)));memcpy(gBuf+got,tmp,r);got+=r;}
          else delay(2);
        }
        hg.end();
        mouthStart(); playBuf(gBuf,gLen); mouthStop();
        free(gBuf);
      } else { hg.end(); }
    } else { hg.end(); }
  }
  moveCenter(); avatar.setExpression(Expression::Neutral);

  // OTA check — before servo task spawns
  checkFirmwareUpdate();  // reboots if update found

  xTaskCreatePinnedToCore(servoFn,"servo",2048,nullptr,2,&servoTaskH,0);

  randomSeed(esp_random());
  lastIdleMove=millis()+3000; lastIdleExpr=millis()+8000;
  lastIdleSpeak=millis()+180000+random(120000);
  lastWifiCheck=millis()+30000;
  state=AMBIENT; vad=VAD_WAIT;
  Serial.printf("[Boot] %s board=%d vad=%d\n",cfgName,(int)M5.getBoard(),cfgVadThresh);
  lastSleepCheck = 0;  // force immediate sleep check on first loop

}

// ════════════════════════════════════════════════════════════
//  Loop
// ════════════════════════════════════════════════════════════


// ─── Pull-based OTA update ─────────────────────────────────
void checkFirmwareUpdate() {
    if (!wifiConnected) return;

    // Ask server what version it has for this robot
    HTTPClient h;
    h.begin("http://"+String(cfgPiIP)+":"+String(cfgPiPort)+
            "/ota/version?robot_id="+String(cfgRobotId));
    h.setTimeout(5000);
    if (h.GET() != 200) { h.end(); return; }
    String serverVer = h.getString();
    serverVer.trim();
    h.end();

    if (serverVer == "none" || serverVer == String(FIRMWARE_VERSION)) return;

    Serial.printf("[OTA] Update available: %s → %s\n", FIRMWARE_VERSION, serverVer.c_str());

    // Show on screen before tasks start
    M5.Display.fillRect(0,100,320,40,TFT_BLACK);
    M5.Display.setTextColor(TFT_GREEN); M5.Display.setTextSize(2);
    M5.Display.drawString("Updating...", 80, 108);
    if (M5.Mic.isRunning()) M5.Mic.end();
    delay(300);

    // Download + flash from Cerebro
    WiFiClient client;
    String url = "http://"+String(cfgPiIP)+":"+String(cfgPiPort)+
                 "/ota/firmware/"+String(cfgRobotId);

    httpUpdate.setLedPin(-1);
    httpUpdate.rebootOnUpdate(true);
    t_httpUpdate_return ret = httpUpdate.update(client, url);

    switch (ret) {
        case HTTP_UPDATE_OK:
            Serial.println("[OTA] Flashed OK — rebooting!");
            break;  // auto-reboots
        case HTTP_UPDATE_FAILED:
            Serial.printf("[OTA] Failed: %s\n", httpUpdate.getLastErrorString().c_str());
            avatar.setSpeechText("Update failed!");
            delay(2000);
            avatar.setSpeechText("");
            M5.Mic.begin(); vad = VAD_WAIT;
            break;
        case HTTP_UPDATE_NO_UPDATES:
            Serial.println("[OTA] Server says no updates");
            break;
    }
}

// ─── Sleep schedule ─────────────────────────────────────
void enterSleepMode() {
    Serial.println("[Sleep] Entering sleep mode — goodnight!");
    // Stop listening
    if (M5.Mic.isRunning()) M5.Mic.end();
    vad = VAD_WAIT; state = AMBIENT;
    // Sleep face + droop head DOWN (low tilt value = head droops toward chest)
    avatar.setExpression(Expression::Sleepy);
    moveTo(PAN_CENTER, TILT_SAFE_MIN);  // ~5° — deeper chin-down droop
    // LEDs off + dim screen (low power)
    ledOff();
    M5.Display.setBrightness(5);
    Serial.println("[Sleep] LEDs off, screen dimmed, mic stopped, head drooped");
}

void exitSleepMode() {
    Serial.println("[Sleep] Good morning! Waking up...");
    // Restore screen + wake face
    M5.Display.setBrightness(128);
    avatar.setExpression(Expression::Happy);
    moveCenter();
    ledOn(); delay(300); ledOff();
    // Check for firmware updates before resuming
    checkFirmwareUpdate();

    // Restart mic
    delay(200); M5.Mic.begin(); delay(200);
    vad = VAD_WAIT;
    // Fetch wake greeting from server
    String gurl = "http://"+String(cfgPiIP)+":"+String(cfgPiPort)+
                  "/greet?robot_id="+String(cfgRobotId)+
                  "&mode="+String(cfgMode)+"&kid_name="+String(cfgKidName);
    HTTPClient hg; hg.begin(gurl); hg.setTimeout(8000);
    if (hg.GET()==200) {
        size_t gLen=hg.getSize();
        uint8_t* gBuf=(uint8_t*)ps_malloc(gLen+16);
        if (gBuf) {
            WiFiClient* gs=hg.getStreamPtr(); size_t got=0; uint8_t tmp[512];
            while(got<gLen&&gs->connected()){size_t av=gs->available();if(av){size_t r=gs->readBytes(tmp,min(av,sizeof(tmp)));memcpy(gBuf+got,tmp,r);got+=r;}else delay(2);}
            hg.end(); mouthStart(); playBuf(gBuf,gLen); mouthStop(); free(gBuf);
        } else { hg.end(); }
    } else { hg.end(); }
    avatar.setExpression(Expression::Neutral); moveCenter();
}

void checkSleepSchedule() {
    if (!wifiConnected) return;
    if (millis()-lastSleepCheck < 30000 && lastSleepCheck > 0) return;   // every 30s
    lastSleepCheck = millis();

    HTTPClient h;
    h.begin("http://"+String(cfgPiIP)+":"+String(cfgPiPort)+"/health");
    h.setTimeout(4000);
    bool nowSleeping = serverSleeping;  // keep last known if request fails
    if (h.GET() == 200) {
        String body = h.getString();
        nowSleeping = body.indexOf("\"sleeping\":true") >= 0;
        Serial.printf("[Sleep] Server says: %s\n", nowSleeping ? "sleeping" : "awake");
    }
    h.end();

    if (nowSleeping && !wasSleeping) {
        serverSleeping = true; wasSleeping = true;
        enterSleepMode();
    } else if (!nowSleeping && wasSleeping) {
        serverSleeping = false; wasSleeping = false;
        exitSleepMode();
    } else {
        serverSleeping = nowSleeping;
    }
}

// ─── Battery monitor ─────────────────────────────────────
uint32_t lastBattCheck  = 0;
uint32_t battWarnAt     = 0;
bool     battWarnActive = false;

void checkBattery() {
    if (!wifiConnected) return;
    if (state != AMBIENT) return;
    if (millis()-lastBattCheck < 180000) return;
    lastBattCheck = millis();

    int batt = (int)M5.Power.getBatteryLevel();
    bool charging = M5.Power.isCharging();
    Serial.printf("[Batt] level=%d charging=%d\n", batt, (int)charging);
    if (batt < 0 || batt > 100) return;
    if (charging) { battWarnActive=false; return; }

    bool low = (batt <= 30);  // warn at 30%
    if (low && (millis()-battWarnAt > 1800000 || !battWarnActive)) {
        battWarnAt     = millis();
        battWarnActive = true;
        Serial.printf("[Batt] %d%% low\n", batt);

        String url = "http://"+String(cfgPiIP)+":"+String(cfgPiPort)+
                     "/battery?robot_id="+String(cfgRobotId)+
                     "&level="+String(batt);
        HTTPClient bh;
        bh.begin(url);
        bh.setTimeout(6000);
        const char* bph[] = {"X-Reply-Text","X-Action","X-SFX"};
        bh.collectHeaders(bph, 3);
        if (bh.GET() == 200) {
            size_t cLen = bh.getSize();
            uint8_t* buf = (uint8_t*)ps_malloc(cLen+16);
            if (buf) {
                WiFiClient* s=bh.getStreamPtr();
                size_t got=0; uint8_t tmp[512];
                while(got<cLen&&s->connected()){
                    size_t av=s->available();
                    if(av){size_t r=s->readBytes(tmp,min(av,sizeof(tmp)));
                           memcpy(buf+got,tmp,r);got+=r;}
                    else delay(2);
                }
                bh.end();
                rgbAll(255, 30, 0);  // red alert on all 12 LEDs
        avatar.setExpression(batt<=10 ? Expression::Sad : Expression::Doubt);
                mouthStart(); playBuf(buf, cLen); mouthStop();
                String sfx = bh.header("X-SFX");
                if (sfx.length()>0 && sfx!="none") playSFX(sfx);
                free(buf);
                avatar.setExpression(Expression::Neutral);
                M5.Mic.begin(); vad=VAD_WAIT;
            } else { bh.end(); }
        } else { bh.end(); }
    }
    if (batt > 25) battWarnActive = false;
}

// ─── Inter-robot message check ───────────────────────────
uint32_t lastPendingCheck = 0;
void checkPendingMessages() {
    if (state != AMBIENT || millis()-lastPendingCheck < 20000) return;
    lastPendingCheck = millis();
    HTTPClient h;
    h.begin("http://"+String(cfgPiIP)+":"+String(cfgPiPort)+
            "/pending?robot_id="+String(cfgRobotId));
    h.setTimeout(5000);
    const char* ph[] = {"X-Reply-Text","X-Action","X-SFX"};
    h.collectHeaders(ph, 3);
    if (h.GET() != 200) { h.end(); return; }
    String txt  = h.header("X-Reply-Text");
    String act  = h.header("X-Action");
    size_t cLen = h.getSize();
    uint8_t* buf = (uint8_t*)ps_malloc(cLen+16);
    if (!buf) { h.end(); return; }
    WiFiClient* s=h.getStreamPtr(); size_t got=0; uint8_t tmp[512];
    while(got<cLen&&s->connected()){
        size_t av=s->available();
        if(av){size_t r=s->readBytes(tmp,min(av,sizeof(tmp)));memcpy(buf+got,tmp,r);got+=r;}
        else delay(2);
    }
    h.end();
    // Play the message from the other robot
    avatar.setExpression(Expression::Happy);
    mouthStart(); playBuf(buf,cLen); mouthStop();
    free(buf);
    if(act=="force_update"){
        Serial.println("[OTA] Force update requested from dashboard");
        avatar.setExpression(Expression::Happy);
        checkFirmwareUpdate();
    } else if(act=="reboot"){
        Serial.println("[Dashboard] Reboot requested");
        avatar.setExpression(Expression::Sleepy);
        delay(2000);
        ESP.restart();
    } else if(act=="shutdown"){
        Serial.println("[Dashboard] Sleep requested");
        enterSleepMode();
    } else if(act.startsWith("save_config:")){
        String cfgJson = act.substring(12);
        Serial.printf("[Config] Writing to SD: %s\n", cfgJson.c_str());
        // Read existing config
        File f = SD.open("/robot_config.json", FILE_READ);
        JsonDocument doc;
        if(f){ deserializeJson(doc, f); f.close(); }
        // Parse and apply new values
        JsonDocument patch;
        if(deserializeJson(patch, cfgJson) == DeserializationError::Ok){
            if(patch.containsKey("vad_threshold"))  { doc["vad_threshold"]  = patch["vad_threshold"];  cfgVadThresh = patch["vad_threshold"]; }
            if(patch.containsKey("kid_name"))        { doc["kid_name"]        = patch["kid_name"].as<String>();  strlcpy(cfgKidName, patch["kid_name"].as<const char*>(), sizeof(cfgKidName)); }
            if(patch.containsKey("speaker_volume"))  { doc["speaker_volume"]  = patch["speaker_volume"]; M5.Speaker.setVolume(constrain((int)patch["speaker_volume"],0,255)); }
            if(patch.containsKey("default_mode"))    { doc["default_mode"]    = patch["default_mode"].as<String>();  strlcpy(cfgMode, patch["default_mode"].as<const char*>(), sizeof(cfgMode)); }
            if(patch.containsKey("robot_name"))      { doc["robot_name"]      = patch["robot_name"].as<String>();    strlcpy(cfgName, patch["robot_name"].as<const char*>(),  sizeof(cfgName)); }
            // Write back to SD
            File fw = SD.open("/robot_config.json", FILE_WRITE);
            if(fw){ serializeJson(doc, fw); fw.close(); Serial.println("[Config] SD card updated ✓"); }
        }
    } else if(act!="none") triggerAction(act);
    delay(400);
    avatar.setExpression(Expression::Neutral);
    M5.Mic.begin(); vad=VAD_WAIT;
}

void loop(){
  M5.update(); handleTouch(); ArduinoOTA.handle();
  checkSleepSchedule();

  // When server says sleep hours — head already drooped, LEDs off, just idle
  if (serverSleeping) {
    delay(500);
    return;
  }

  updateRGB();
  checkBattery();
  checkPendingMessages();

  if(millis()>lastWifiCheck){
    lastWifiCheck=millis()+30000;
    if(WiFi.status()!=WL_CONNECTED){
      avatar.setSpeechText("Reconnecting...");
      ensureWiFi(cfgPiIP,&cfgPiPort,nullptr);
      avatar.setSpeechText("");
    }
  }

  camFollowFace();  // nudge servos toward detected face when idle

  if(state==AMBIENT){
    bool done=pollVAD();
    if(vad==VAD_RECORDING){avatar.setExpression(Expression::Happy);moveListen();}
    if(done) sendAndRespond();
    else if(vad==VAD_WAIT){idleAnimate();checkIMU();}
    if(M5.BtnA.wasPressed()) lastIdleSpeak=0;
    if(M5.BtnC.wasPressed()&&state==AMBIENT){
      M5.Mic.end();
      HTTPClient h; h.begin("http://"+String(cfgPiIP)+":"+String(cfgPiPort)+"/reset");
      h.addHeader("X-Session",cfgRobotId); h.POST(""); h.end();
      avatar.setExpression(Expression::Happy); avatar.setSpeechText("Memory cleared!"); moveSurprised();
      delay(1400); avatar.setSpeechText(""); moveCenter(); avatar.setExpression(Expression::Neutral);
      M5.Mic.begin(); vad=VAD_WAIT;
    }
  }
  delay(8);
}
